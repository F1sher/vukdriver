#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
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
	{ 0xa4bb88f0, "module_layout" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0x50eedeb8, "printk" },
	{ 0x4cc995c5, "vfs_write" },
	{ 0xc80f5f82, "vfs_read" },
	{ 0x187037e0, "filp_close" },
	{ 0x61980b3a, "filp_open" },
	{ 0xb4390f9a, "mcount" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "E0602847023CE2FA3F22CC8");
