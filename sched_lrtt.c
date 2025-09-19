// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/kernel.h>
#include <net/mptcp.h>
#include "protocol.h"

#ifndef pr_fmt
#define pr_fmt(fmt) "MPTCP-LRTT: " fmt
#endif

static int lrtt_get_send(struct mptcp_sock *msk)
{
	struct mptcp_subflow_context *subflow, *best = NULL;
	struct sock *ssk;
	u32 rtt, lowest_rtt = ~0U;

	mptcp_for_each_subflow(msk, subflow) {
		ssk = mptcp_subflow_tcp_sock(subflow);

		// Ensure subflow is active and has buffer space
		if (!__mptcp_subflow_active(subflow) || !sk_stream_memory_free(ssk))
			continue;

		// RTT in usec, >>3 from fixed-point
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
		return 0;
	}

	pr_info("No usable subflow found\n");
	return -EINVAL;
}

static struct mptcp_sched_ops lrtt_sched = {
	.get_send = lrtt_get_send,
	.name     = "lrtt",
	.owner    = THIS_MODULE,
};

static int __init lrtt_register(void)
{
	return mptcp_register_scheduler(&lrtt_sched);
}

static void __exit lrtt_unregister(void)
{
	mptcp_unregister_scheduler(&lrtt_sched);
}

module_init(lrtt_register);
module_exit(lrtt_unregister);

MODULE_AUTHOR("Nathaneal");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Lowest RTT First (LRTT) MPTCP scheduler");
