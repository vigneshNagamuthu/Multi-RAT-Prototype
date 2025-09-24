#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

MODULE_INFO(intree, "Y");



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x6cafd9ab, "inet_diag_unregister" },
	{ 0xce4c38c3, "mptcp_token_get_sock" },
	{ 0x31131af3, "__alloc_skb" },
	{ 0x0560edc2, "netlink_net_capable" },
	{ 0x8f5aa268, "inet_sk_diag_fill" },
	{ 0x57c06783, "netlink_unicast" },
	{ 0x6458024e, "sk_skb_reason_drop" },
	{ 0xd70dc42c, "sk_free" },
	{ 0x0296695f, "refcount_warn_saturate" },
	{ 0xc58248c1, "mptcp_token_iter_next" },
	{ 0xfc405e53, "inet_diag_bc_sk" },
	{ 0xc07351b3, "__SCT__cond_resched" },
	{ 0x8d522714, "__rcu_read_lock" },
	{ 0xba8fbd64, "_raw_spin_lock" },
	{ 0xe2d5255a, "strcmp" },
	{ 0xb5b54b34, "_raw_spin_unlock" },
	{ 0x2469810f, "__rcu_read_unlock" },
	{ 0xa648e561, "__ubsan_handle_shift_out_of_bounds" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0x82fd3a60, "inet_diag_register" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0x12aba4f1, "mptcp_diag_fill_info" },
	{ 0xb33d9bee, "module_layout" },
};

MODULE_INFO(depends, "inet_diag");


MODULE_INFO(srcversion, "E300250E6702A7D41453249");
