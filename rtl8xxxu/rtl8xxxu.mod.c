#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xd5f999fc, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x2d3385d3, __VMLINUX_SYMBOL_STR(system_wq) },
	{ 0x6ac59621, __VMLINUX_SYMBOL_STR(ieee80211_rx_irqsafe) },
	{ 0xb064334f, __VMLINUX_SYMBOL_STR(kmalloc_caches) },
	{ 0xf9a482f9, __VMLINUX_SYMBOL_STR(msleep) },
	{ 0xc5604665, __VMLINUX_SYMBOL_STR(param_ops_int) },
	{ 0x344796f5, __VMLINUX_SYMBOL_STR(usb_init_urb) },
	{ 0x448eac3e, __VMLINUX_SYMBOL_STR(kmemdup) },
	{ 0xfea4453, __VMLINUX_SYMBOL_STR(ieee80211_unregister_hw) },
	{ 0xeae3dfd6, __VMLINUX_SYMBOL_STR(__const_udelay) },
	{ 0x57a22038, __VMLINUX_SYMBOL_STR(param_ops_bool) },
	{ 0xd06b75c, __VMLINUX_SYMBOL_STR(mutex_unlock) },
	{ 0x7d11c268, __VMLINUX_SYMBOL_STR(jiffies) },
	{ 0x9ed9eea, __VMLINUX_SYMBOL_STR(usb_unanchor_urb) },
	{ 0xc55d74ed, __VMLINUX_SYMBOL_STR(__netdev_alloc_skb) },
	{ 0x9e88526, __VMLINUX_SYMBOL_STR(__init_waitqueue_head) },
	{ 0xfb578fc5, __VMLINUX_SYMBOL_STR(memset) },
	{ 0x5624bc4f, __VMLINUX_SYMBOL_STR(dev_err) },
	{ 0x1916e38c, __VMLINUX_SYMBOL_STR(_raw_spin_unlock_irqrestore) },
	{ 0x1aa9f784, __VMLINUX_SYMBOL_STR(usb_deregister) },
	{ 0x39799adb, __VMLINUX_SYMBOL_STR(ieee80211_alloc_hw_nm) },
	{ 0x86382af2, __VMLINUX_SYMBOL_STR(__mutex_init) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0x3ff56275, __VMLINUX_SYMBOL_STR(usb_control_msg) },
	{ 0x16305289, __VMLINUX_SYMBOL_STR(warn_slowpath_null) },
	{ 0x4e27fa10, __VMLINUX_SYMBOL_STR(skb_push) },
	{ 0xd5e316dd, __VMLINUX_SYMBOL_STR(mutex_lock) },
	{ 0x67be975a, __VMLINUX_SYMBOL_STR(skb_pull) },
	{ 0x6e423951, __VMLINUX_SYMBOL_STR(request_firmware_nowait) },
	{ 0x167c5967, __VMLINUX_SYMBOL_STR(print_hex_dump) },
	{ 0x9c846037, __VMLINUX_SYMBOL_STR(_dev_info) },
	{ 0xef1e5fd7, __VMLINUX_SYMBOL_STR(usb_submit_urb) },
	{ 0x78764f4e, __VMLINUX_SYMBOL_STR(pv_irq_ops) },
	{ 0x9f682512, __VMLINUX_SYMBOL_STR(usb_get_dev) },
	{ 0x12a38747, __VMLINUX_SYMBOL_STR(usleep_range) },
	{ 0xdb7305a1, __VMLINUX_SYMBOL_STR(__stack_chk_fail) },
	{ 0x308e15c8, __VMLINUX_SYMBOL_STR(usb_put_dev) },
	{ 0xa3bf0599, __VMLINUX_SYMBOL_STR(ieee80211_tx_status_irqsafe) },
	{ 0xa202a8e5, __VMLINUX_SYMBOL_STR(kmalloc_order_trace) },
	{ 0x8693d6e, __VMLINUX_SYMBOL_STR(ieee80211_find_sta) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
	{ 0x96e2982a, __VMLINUX_SYMBOL_STR(kmem_cache_alloc_trace) },
	{ 0x50d8f0e3, __VMLINUX_SYMBOL_STR(__dynamic_dev_dbg) },
	{ 0x680ec266, __VMLINUX_SYMBOL_STR(_raw_spin_lock_irqsave) },
	{ 0xd79d9809, __VMLINUX_SYMBOL_STR(ieee80211_register_hw) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0x69acdf38, __VMLINUX_SYMBOL_STR(memcpy) },
	{ 0x93d64405, __VMLINUX_SYMBOL_STR(usb_register_driver) },
	{ 0x55128c3b, __VMLINUX_SYMBOL_STR(request_firmware) },
	{ 0x62680111, __VMLINUX_SYMBOL_STR(ieee80211_free_hw) },
	{ 0xa4ef98da, __VMLINUX_SYMBOL_STR(dev_warn) },
	{ 0x2e0d2f7f, __VMLINUX_SYMBOL_STR(queue_work_on) },
	{ 0xb2d5a552, __VMLINUX_SYMBOL_STR(complete) },
	{ 0xa2d3c76, __VMLINUX_SYMBOL_STR(consume_skb) },
	{ 0x47e42c7b, __VMLINUX_SYMBOL_STR(skb_put) },
	{ 0x4c97333f, __VMLINUX_SYMBOL_STR(usb_free_urb) },
	{ 0x254479b1, __VMLINUX_SYMBOL_STR(release_firmware) },
	{ 0x9e7d6bd0, __VMLINUX_SYMBOL_STR(__udelay) },
	{ 0x39cf6d55, __VMLINUX_SYMBOL_STR(usb_anchor_urb) },
	{ 0xd3144653, __VMLINUX_SYMBOL_STR(usb_alloc_urb) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=mac80211";

MODULE_ALIAS("usb:v0BDAp8724d*dc*dsc*dp*icFFiscFFipFFin*");
MODULE_ALIAS("usb:v0BDAp1724d*dc*dsc*dp*icFFiscFFipFFin*");
MODULE_ALIAS("usb:v0BDAp0724d*dc*dsc*dp*icFFiscFFipFFin*");
MODULE_ALIAS("usb:v0BDAp818Bd*dc*dsc*dp*icFFiscFFipFFin*");
MODULE_ALIAS("usb:v0BDApB720d*dc*dsc*dp*icFFiscFFipFFin*");
MODULE_ALIAS("usb:v0BDAp8179d*dc*dsc*dp*icFFiscFFipFFin*");
MODULE_ALIAS("usb:v0BDAp0179d*dc*dsc*dp*icFFiscFFipFFin*");

MODULE_INFO(srcversion, "6AC5C41AD2206170C918D4B");
