// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <net/mptcp.h>
#include "protocol.h"

static u32 rr_idx = 0;
static ktime_t last_switch;

static int rr_get_send(struct mptcp_sock *msk)
{
	struct mptcp_subflow_context *subflow;
	int count = 0, idx = 0, j = 0;
	ktime_t now = ktime_get();

	msk_owned_by_me(msk);

	// Count active+sendable subflows
	mptcp_for_each_subflow(msk, subflow) {
		if (__tcp_can_send(subflow->tcp_sock) &&
		    sk_stream_memory_free(subflow->tcp_sock))
			count++;
	}

	if (count == 0)
		return -EINVAL;

	// Switch subflow every 10 seconds
	if (ktime_to_ms(ktime_sub(now, last_switch)) >= 20000) {
		rr_idx = (rr_idx + 1) % count;
		last_switch = now;
		pr_info("RR-Time: Switched to subflow index %u\n", rr_idx);
	}

	idx = rr_idx;
	j = 0;

	mptcp_for_each_subflow(msk, subflow) {
		if (!__tcp_can_send(subflow->tcp_sock) ||
		    !sk_stream_memory_free(subflow->tcp_sock))
			continue;

		if (j++ != idx)
			continue;

		mptcp_subflow_set_scheduled(subflow, true);
		pr_info("RR-Time: Sending on subflow idx=%d ifindex=%d\n",
			idx, subflow->tcp_sock->sk_bound_dev_if);
		return 0;
	}

	return -EINVAL;
}

static int rr_get_retrans(struct mptcp_sock *msk)
{
	return rr_get_send(msk);
}

static void rr_init(struct mptcp_sock *msk)
{
	rr_idx = 0;
	last_switch = ktime_get();
}

static void rr_release(struct mptcp_sock *msk) { }

static struct mptcp_sched_ops mptcp_rr_time_sched = {
	.get_send    = rr_get_send,
	.get_retrans = rr_get_retrans,
	.init        = rr_init,
	.release     = rr_release,
	.name        = "rrtime",
	.owner       = THIS_MODULE,
};

static int __init rr_register(void)
{
	return mptcp_register_scheduler(&mptcp_rr_time_sched);
}

static void __exit rr_unregister(void)
{
	mptcp_unregister_scheduler(&mptcp_rr_time_sched);
}

module_init(rr_register);
module_exit(rr_unregister);

MODULE_AUTHOR("Modified by ChatGPT");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Time-based Round-Robin MPTCP Scheduler");
