// Defining __KERNEL__ and MODULE allows us to access kernel-level code not usually available to userspace programs.
#undef __KERNEL__
#define __KERNEL__
 
#undef MODULE
#define MODULE
 
// Linux Kernel/LKM headers: module.h is needed by all modules and kernel.h is needed for KERN_INFO.
#include <linux/module.h>    // included for all kernel modules
#include <linux/kernel.h>    // included for KERN_INFO
#include <linux/init.h>        // included for __init and __exit macros
#include <linux/fs.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/buffer_head.h>

struct file *test_file;

struct file* file_open(const char* path, int flags, int rights) {
    struct file* filp = NULL;
    mm_segment_t oldfs;
    int err = 0;

    oldfs = get_fs();
    set_fs(get_ds());
    filp = filp_open(path, flags, rights);
    set_fs(oldfs);
    if(IS_ERR(filp)) {
        err = PTR_ERR(filp);
        return NULL;
    }
    
    return filp;
}

void file_close(struct file* file) {
    filp_close(file, NULL);
}

int file_read(struct file* file, unsigned char* data, unsigned int size) {
    mm_segment_t oldfs;
    int ret;

    oldfs = get_fs();
    set_fs(get_ds());

    ret = vfs_read(file, data, size, &test_file->f_pos);

    set_fs(oldfs);
    return ret;
}

int file_write(struct file* file, unsigned char* data, unsigned int size) {
    mm_segment_t oldfs;
    int ret;

    oldfs = get_fs();
    set_fs(get_ds());

    ret = vfs_write(file, data, size, &test_file->f_pos);

    set_fs(oldfs);
    return ret;
}

static int __init hello_init(void)
{
    printk(KERN_INFO "Write to file!\n");
    
   /* test_file = file_open("/home/das/job/km/test_file", O_RDWR, 0);
    
    file_write(test_file, "Hello!\n123", sizeof("Hello!\n123"));
    
    
    unsigned char *data;
    
    file_read(test_file, data, sizeof("Hello!\n123"));
    
    printk(KERN_INFO "Read from file = %s\n", data);*/
    
    test_file = filp_open("/home/das/job/EBE_driver/r_wr", O_RDWR, 0);
    
	if(test_file == NULL) {
       printk(KERN_INFO "Open error\n");
    }
    
    mm_segment_t oldfs;
    oldfs = get_fs();
    set_fs(get_ds());
    
    unsigned char buff[100];
    int i;
    
    for(i=0; i<100; i++)
		buff[i] = 0;
    
    vfs_read(test_file, buff, 100, &test_file->f_pos);
    printk(KERN_INFO "Buff = %s\n", buff);
    
    for(i=0; i<100; i++)
		buff[i] = 2*i;
   
    vfs_write(test_file, buff, 100, &test_file->f_pos);
    for(i=0; i<100; i++)
		buff[i] = 0;
    
    vfs_read(test_file, buff, 100, &test_file->f_pos);
    printk(KERN_INFO "Buff = %s, test_file->f_pos = %lld\n", buff, test_file->f_pos);
    
    return 0;    // Non-zero return means that the module couldn't be loaded.
}
 
static void __exit hello_cleanup(void)
{
	file_close(test_file);
	
    printk(KERN_INFO "Cleaning up module.\n");
}
 
module_init(hello_init);
module_exit(hello_cleanup);
