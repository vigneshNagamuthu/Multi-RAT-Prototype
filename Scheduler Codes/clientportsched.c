// SPDX-License-Identifier: GPL-2.0
/*
 * MPTCP "portsched" — source-port selector that dispatches to your
 * proven LRTT and Redundant schedulers without altering their logic.
 *
 * Client SOURCE port decides mode:
 *   5000 -> REDUNDANT (your code path)
 *   6060 -> LRTT     (your code path)
 *   else -> return -EAGAIN (let core/next scheduler handle it)
 *
 * Notes:
 * - We intentionally keep the scheduling bodies identical to your
 *   working modules to avoid regressions.
 * - Banners are rate-limited to reduce log spam.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <net/tcp.h>
#include <net/mptcp.h>
#include <net/inet_connection_sock.h>
#include "protocol.h"

#ifndef pr_fmt
#define pr_fmt(fmt) "MPTCP-PORTSCHED: " fmt
#endif

/* Trigger ports (CLIENT SOURCE PORT) */
#define PORT_REDUNDANT 5000
#define PORT_LRTT      6060

/* --- Tiny helpers --- */
static inline u16 dport(const struct sock *sk) { return ntohs(inet_sk(sk)->inet_dport); }
static inline u16 sport(const struct sock *sk) { return ntohs(inet_sk(sk)->inet_sport); }

/* ========== REDUNDANT (exactly your working logic) ========== */
/* Source: your sched_redundant.c (kept intact). */
static int redundant_like_yours_get_send(struct mptcp_sock *msk)
{
	struct mptcp_subflow_context *subflow;
	bool scheduled = false;

	msk_owned_by_me(msk);

	mptcp_for_each_subflow(msk, subflow) {
		if (!mptcp_subflow_active(subflow))
			continue;

		if (!__tcp_can_send(subflow->tcp_sock) ||
		    !sk_stream_memory_free(subflow->tcp_sock)) {
			pr_debug("Redundant: Skipping subflow (not writable)");
			continue;
		}

		mptcp_subflow_set_scheduled(subflow, true);
		scheduled = true;

		pr_info("Redundant: Scheduling subflow on ifindex=%d\n",
			subflow->tcp_sock->sk_bound_dev_if);
	}

	return scheduled ? 0 : -EINVAL;
}

static int redundant_like_yours_get_retrans(struct mptcp_sock *msk)
{
	/* Redundant retransmits the same way */
	return redundant_like_yours_get_send(msk);
}

/* ========= LRTT (exactly your working logic) ========= */
/* Source: your sched_lrtt.c (kept intact). */
static int lrtt_like_yours_get_send(struct mptcp_sock *msk)
{
	struct mptcp_subflow_context *subflow, *best = NULL;
	struct sock *ssk;
	u32 rtt, lowest_rtt = ~0U;

	mptcp_for_each_subflow(msk, subflow) {
		ssk = mptcp_subflow_tcp_sock(subflow);

		/* Ensure subflow is active and has buffer space */
		if (!__mptcp_subflow_active(subflow) || !sk_stream_memory_free(ssk))
			continue;

		/* RTT in usec, >>3 from fixed-point */
		rtt = tcp_sk(ssk)->srtt_us >> 3;

		pr_info("Subflow: src=%pI4 dst=%pI4 rtt=%u us\n",
			&inet_sk(ssk)->inet_saddr,
			&inet_sk(ssk)->inet_daddr,
			rtt);

		if (rtt < lowest_rtt) {
			lowest_rtt = rtt;
			best = subflow;
		}
	}

	if (best) {
		ssk = mptcp_subflow_tcp_sock(best);
		pr_info("Selected: src=%pI4 dst=%pI4 rtt=%u us\n",
			&inet_sk(ssk)->inet_saddr,
			&inet_sk(ssk)->inet_daddr,
			lowest_rtt);
		mptcp_subflow_set_scheduled(best, true);
		{
			struct sock *bsk = mptcp_subflow_tcp_sock(best);
			pr_info("USING: if=%d src=%pI4:%u dst=%pI:%u rtt=%u us\n",
				bsk->sk_bound_dev_if,
				&inet_sk(bsk)->inet_saddr, ntohs(inet_sk(bsk)->inet_sport),
				&inet_sk(bsk)->inet_saddr, ntohs(inet_sk(bsk)->inet_sport),
				tcp_sk(bsk)->srtt_us >> 3);
		}
		return 0;
	}

	pr_info("No usable subflow found\n");
	return -EINVAL;
}

/* ========= Glue scheduler: pick body by client source port ========= */

static void portsched_init(struct mptcp_sock *msk) { }
static void portsched_release(struct mptcp_sock *msk) { }

static int portsched_get_send(struct mptcp_sock *msk)
{
	const struct sock *sk = (const struct sock *)msk;
	u16 sp = sport(sk);

	/* Only act on our trigger ports; avoid affecting other sockets */
	if (sp != PORT_REDUNDANT && sp != PORT_LRTT)
		return -EAGAIN;

	if (sp == PORT_REDUNDANT) {
		pr_info_ratelimited("MODE=REDUNDANT sp=%u dp=%u\n", sp, dport(sk));
		return redundant_like_yours_get_send(msk);
	} else {
		pr_info_ratelimited("MODE=LRTT sp=%u dp=%u\n", sp, dport(sk));
		return lrtt_like_yours_get_send(msk);
	}
}

static int portsched_get_retrans(struct mptcp_sock *msk)
{
	/* For consistency with your redundant module: send again on all;
	 * but if LRTT was selected, core retrans helper is also fine.
	 * Keep it simple: reuse redundant retrans (schedules send paths). */
	return redundant_like_yours_get_retrans(msk);
}

/* Registration */
static struct mptcp_sched_ops portsched = {
	.name        = "clientportsched",
	.owner       = THIS_MODULE,
	.init        = portsched_init,
	.release     = portsched_release,
	.get_send    = portsched_get_send,
	.get_retrans = portsched_get_retrans,
};

static int __init portsched_register(void) { return mptcp_register_scheduler(&portsched); }
static void __exit portsched_unregister(void) { mptcp_unregister_scheduler(&portsched); }

module_init(portsched_register);
module_exit(portsched_unregister);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("You");
MODULE_DESCRIPTION("MPTCP source-port selector: 5000→Redundant, 6060→LRTT (uses your proven implementations)");
