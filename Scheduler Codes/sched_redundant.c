// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/kernel.h>
#include <net/mptcp.h>
#include "protocol.h"

static int redundant_get_send(struct mptcp_sock *msk)
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

static int redundant_get_retrans(struct mptcp_sock *msk)
{
	// Redundant retransmits the same way
	return redundant_get_send(msk);
}

static void redundant_init(struct mptcp_sock *msk)
{
	pr_info("Redundant: Scheduler initialized\n");
}

static void redundant_release(struct mptcp_sock *msk)
{
	pr_info("Redundant: Scheduler released\n");
}

static struct mptcp_sched_ops mptcp_redundant_sched = {
	.get_send    = redundant_get_send,
	.get_retrans = redundant_get_retrans,
	.init        = redundant_init,
	.release     = redundant_release,
	.name        = "redundant",
	.owner       = THIS_MODULE,
};

static int __init redundant_register(void)
{
	return mptcp_register_scheduler(&mptcp_redundant_sched);
}

static void __exit redundant_unregister(void)
{
	mptcp_unregister_scheduler(&mptcp_redundant_sched);
}

module_init(redundant_register);
module_exit(redundant_unregister);

MODULE_AUTHOR("ChatGPT");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Redundant MPTCP Scheduler - Sends on all subflows simultaneously");
