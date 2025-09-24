# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_MPTCP) += mptcp.o
obj-$(CONFIG_MPTCP_SCHED_RR_TIME) += sched_rr_time.o
obj-$(CONFIG_MPTCP_SCHED_RR) += sched_rr.o
obj-$(CONFIG_MPTCP_SCHED_REDUNDANT) += sched_redundant.o
obj-$(CONFIG_MPTCP_SCHED_RTTTHRESH) += sched_rtt_thresh.o
obj-$(CONFIG_MPTCP_LRTT) += sched_lrtt.o
obj-$(CONFIG_MPTCP_SCHED_STRICTPRIO) += sched_sp.o
obj-$(CONFIG_MPTCP_SCHED_PORTSCHED) += portsched.o


mptcp-y := protocol.o subflow.o options.o token.o crypto.o ctrl.o pm.o diag.o \
	   mib.o pm_netlink.o sockopt.o pm_userspace.o fastopen.o sched.o \
	   mptcp_pm_gen.o pm_kernel.o

obj-$(CONFIG_SYN_COOKIES) += syncookies.o
obj-$(CONFIG_INET_MPTCP_DIAG) += mptcp_diag.o

mptcp_crypto_test-objs := crypto_test.o
mptcp_token_test-objs := token_test.o
obj-$(CONFIG_MPTCP_KUNIT_TEST) += mptcp_crypto_test.o mptcp_token_test.o

obj-$(CONFIG_BPF_SYSCALL) += bpf.o

# for GCOV coverage profiling
ifdef CONFIG_GCOV_PROFILE_MPTCP
GCOV_PROFILE := y
endif
