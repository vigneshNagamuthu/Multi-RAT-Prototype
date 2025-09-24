// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/kernel.h>
#include <net/mptcp.h>
#include "protocol.h"

#define RTT_THRESHOLD_US 300000  // 300 ms

static int rtt_thresh_get_send(struct mptcp_sock *msk)
{
	struct mptcp_subflow_context *subflow;
	bool found = false;

	msk_owned_by_me(msk);

	mptcp_for_each_subflow(msk, subflow) {
		struct sock *sk = subflow->tcp_sock;
		struct tcp_sock *tp = tcp_sk(sk);
		u32 rtt_us = tp->srtt_us >> 3;

		pr_info("RTT-Check: ifindex=%d [rtt=%u us] [can_send=%d] [mem_free=%d]\n",
		        sk->sk_bound_dev_if,
		        rtt_us,
		        __tcp_can_send(sk),
		        sk_stream_memory_free(sk));

		if (!__tcp_can_send(sk) || !sk_stream_memory_free(sk))
			continue;

		if (rtt_us && rtt_us < RTT_THRESHOLD_US) {
			mptcp_subflow_set_scheduled(subflow, true);
			pr_info("RTT-Threshold: Selected subflow ifindex=%d with RTT=%u us\n",
			        sk->sk_bound_dev_if, rtt_us);
			found = true;
		} else {
			mptcp_subflow_set_scheduled(subflow, false);
		}
	}

	if (!found) {
		pr_info("RTT-Threshold: No usable subflow below %u us\n", RTT_THRESHOLD_US);
		return -EINVAL;
	}

	return 0;
}

static int rtt_thresh_get_retrans(struct mptcp_sock *msk)
{
	return rtt_thresh_get_send(msk);
}

static void rtt_thresh_init(struct mptcp_sock *msk) { }
static void rtt_thresh_release(struct mptcp_sock *msk) { }

static struct mptcp_sched_ops mptcp_rtt_thresh_sched = {
	.get_send    = rtt_thresh_get_send,
	.get_retrans = rtt_thresh_get_retrans,
	.init        = rtt_thresh_init,
	.release     = rtt_thresh_release,
	.name        = "rtt_thresh",
	.owner       = THIS_MODULE,
};

static int __init rtt_thresh_register(void)
{
	pr_info("RTT-Threshold scheduler loaded\n");
	return mptcp_register_scheduler(&mptcp_rtt_thresh_sched);
}

static void __exit rtt_thresh_unregister(void)
{
	pr_info("RTT-Threshold scheduler unloaded\n");
	mptcp_unregister_scheduler(&mptcp_rtt_thresh_sched);
}

module_init(rtt_thresh_register);
module_exit(rtt_thresh_unregister);

MODULE_AUTHOR("Nathaneal + ChatGPT");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MPTCP Scheduler: Allow all subflows under 300ms RTT");
