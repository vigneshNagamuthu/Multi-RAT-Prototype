// SPDX-License-Identifier: GPL-2.0
/*
 * MPTCP "portsched" — port-based scheduler with optional NIC pinning.
 *
 * - Port 5000  -> REDUNDANT: duplicate data on all subflows (or on a pinned NIC)
 * - Port 6060  -> LRTT-like: schedule only the best-RTT subflow (optionally pinned)
 * - You can change PORT_REDUNDANT / PORT_LRTT below.
 *
 * Optional NIC pinning (module params):
 *   force_if_redundant=<ifname>
 *   force_if_lrtt=<ifname>
 *
 * Notes:
 * - Matches on either src or dst port so server-side sockets on :5000/:6060 also match.
 * - If the pinned NIC is missing/down or has no subflows yet, we log once and fall back.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>          /* dev_get_by_name_rcu, netif_running */
#include <net/tcp.h>
#include <net/inet_connection_sock.h>
#include <net/mptcp.h>
#include "protocol.h"                 /* mptcp_sock, mptcp_for_each_subflow, helpers */

#define PORT_REDUNDANT 5000
#define PORT_LRTT      6060

/* ---- Module parameters: optional NIC pinning per mode ---- */
static char *force_if_redundant = "";
module_param(force_if_redundant, charp, 0644);
MODULE_PARM_DESC(force_if_redundant, "Interface to use in REDUNDANT mode (e.g. ens33)");

static char *force_if_lrtt = "";
module_param(force_if_lrtt, charp, 0644);
MODULE_PARM_DESC(force_if_lrtt, "Interface to use in LRTT mode (e.g. ens34)");

/* ---- Helpers ---- */
static inline u16 dport(const struct sock *sk)
{
    return ntohs(inet_sk(sk)->inet_dport);
}

static inline u16 sport(const struct sock *sk)
{
    return ntohs(inet_sk(sk)->inet_sport);
}

/* Match either endpoint so this works for client and server sides */
static inline bool port_matches(const struct mptcp_sock *msk, u16 p)
{
    const struct sock *sk = (const struct sock *)msk;
    return dport(sk) == p || sport(sk) == p;
}

static inline bool is_redundant_mode(const struct mptcp_sock *msk)
{
    return port_matches(msk, PORT_REDUNDANT);
}

/* If a specific NIC name is configured, translate to ifindex (0 = not pinned) */
static int want_ifindex(const struct mptcp_sock *msk, bool redundant_mode)
{
    const char *ifname = redundant_mode ? force_if_redundant : force_if_lrtt;
    struct net *netns = sock_net((struct sock *)msk);
    int idx = 0;

    if (!ifname || !*ifname)
        return 0;

    rcu_read_lock();
    {
        struct net_device *dev = dev_get_by_name_rcu(netns, ifname);
        if (dev && netif_running(dev) && netif_carrier_ok(dev))
            idx = dev->ifindex;
    }
    rcu_read_unlock();
    return idx; /* 0 => not found/not up */
}

/* ---- Core scheduling paths ---- */

/* REDUNDANT: schedule every subflow (optionally only those on ifindex) */
static int schedule_redundant(struct mptcp_sock *msk, int ifindex)
{
    bool any = false, matched = false;
    struct mptcp_subflow_context *subflow;

    mptcp_for_each_subflow(msk, subflow) {
        struct sock *ssk;
        if (!subflow)
            continue;
        any = true;

        /* Underlying TCP socket of the subflow */
        ssk = subflow->tcp_sock; /* field name is stable in recent trees */

        /* If pinned, only schedule subflows bound to that device */
        if (ifindex && ssk->sk_bound_dev_if != ifindex)
            continue;

        mptcp_subflow_set_scheduled(subflow, true);
        matched = true;
    }

    if (ifindex && any && !matched) {
        pr_info_ratelimited("portsched: pinned ifindex=%d has no subflows; falling back to all.\n",
                            ifindex);
        /* Fallback: schedule all */
        mptcp_for_each_subflow(msk, subflow) {
            if (subflow)
                mptcp_subflow_set_scheduled(subflow, true);
        }
    }
    return 0;
}

/* LRTT-like: pick the lowest-RTT subflow, optionally restricted to ifindex */
static int schedule_lrtt(struct mptcp_sock *msk, int ifindex)
{
    struct mptcp_subflow_context *subflow;
    struct sock *best = NULL;
    u32 best_srtt = U32_MAX;

    /* Try to pick the best among matching NIC subflows first */
    mptcp_for_each_subflow(msk, subflow) {
        struct sock *ssk;
        u32 srtt;

        if (!subflow)
            continue;

        ssk = subflow->tcp_sock;
        if (ifindex && ssk->sk_bound_dev_if != ifindex)
            continue;

        /* srtt_us is scaled by 8; comparing raw values is fine */
        srtt = tcp_sk(ssk)->srtt_us;
        if (!best || srtt < best_srtt) {
            best = ssk;
            best_srtt = srtt;
        }
    }

    if (!best) {
        /* No subflow on the pinned NIC (or none yet): let core choose */
        best = mptcp_subflow_get_send(msk);
        if (!best)
            return -EINVAL;
    }

    mptcp_subflow_set_scheduled(mptcp_subflow_ctx(best), true);
    return 0;
}

/* ---- Scheduler ops ---- */

static void portsched_init(struct mptcp_sock *msk) { }
static void portsched_release(struct mptcp_sock *msk) { }

/* Your tree’s ops expect: int (*get_send)(struct mptcp_sock *) */
static int portsched_get_send(struct mptcp_sock *msk)
{
    bool red = is_redundant_mode(msk);
    int ifidx = want_ifindex(msk, red);

    pr_debug("portsched: %s dport=%u sport=%u ifidx=%d\n",
             red ? "REDUNDANT" : "LRTT",
             dport((const struct sock *)msk),
             sport((const struct sock *)msk),
             ifidx);

    return red ? schedule_redundant(msk, ifidx)
               : schedule_lrtt(msk, ifidx);
}

/* And: int (*get_retrans)(struct mptcp_sock *) */
static int portsched_get_retrans(struct mptcp_sock *msk)
{
    /* Keep retransmissions simple: use the core helper (could add pinning if needed) */
    struct sock *ssk = mptcp_subflow_get_retrans(msk);
    if (!ssk)
        return -EINVAL;

    mptcp_subflow_set_scheduled(mptcp_subflow_ctx(ssk), true);
    return 0;
}

static struct mptcp_sched_ops portsched = {
    .name        = "portsched",
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
MODULE_DESCRIPTION("MPTCP port-based scheduler (5000→redundant, 6060→lrtt) with optional NIC pinning");
