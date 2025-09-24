// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/kernel.h>
#include <net/mptcp.h>
#include "protocol.h"

static void mptcp_sched_strictprio_init(struct mptcp_sock *msk) {}

static int mptcp_sched_strictprio_get_send(struct mptcp_sock *msk)
{
    struct mptcp_subflow_context *subflow;
    struct sock *selected = NULL;
    u32 best_prio = U32_MAX;

    list_for_each_entry(subflow, &msk->conn_list, node) {
        struct sock *ssk = subflow->tcp_sock;

        if (!ssk || !subflow->fully_established)
            continue;

        u32 prio = subflow->local_id;

        if (prio < best_prio) {
            best_prio = prio;
            selected = ssk;
        }
    }

    return selected != NULL;  // ✅ This tells the kernel: "I found a valid subflow"
}

static struct mptcp_sched_ops mptcp_sched_strictprio = {
    .get_send = mptcp_sched_strictprio_get_send,
    .init     = mptcp_sched_strictprio_init,
    .name     = "strictprio",
    .owner    = THIS_MODULE,
};

static int __init mptcp_sched_strictprio_register(void)
{
    return mptcp_register_scheduler(&mptcp_sched_strictprio);
}

static void __exit mptcp_sched_strictprio_unregister(void)
{
    mptcp_unregister_scheduler(&mptcp_sched_strictprio);
}

module_init(mptcp_sched_strictprio_register);
module_exit(mptcp_sched_strictprio_unregister);

MODULE_AUTHOR("Your Name");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Strict Priority MPTCP Scheduler");
