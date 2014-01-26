#include <linux/kernel.h>    
#include <linux/module.h>  
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/init.h>  
#include <linux/workqueue.h> 
#include <linux/sched.h>     
#include <linux/interrupt.h> 
#include <linux/fs.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/netlink.h>
#include <net/sock.h>
#include <net/net_namespace.h>
#include <linux/time.h>
#include <linux/jiffies.h>


#define IRQ_NUM		3
#define BASEPORT	0x380
#define NUM_BLOCKS	8
#define ARRAY_EL	4098
#define MAX_CH	4096
#define MY_WORK_QUEUE_NAME "JINR"
#define TIMER_FREQ	HZ
#define NETLINK_NITRO 17

#define BLOCK1(x)	(x) ? (0x0405) : (0x0404)
#define BLOCK2(x)	(x) ? (0x0805) : (0x0804)
#define BLOCK3(x)	(x) ? (0x0c05) : (0x0c04)
#define BLOCK4(x)	(x) ? (0x1005) : (0x1004)
#define BLOCK6(x)	(x) ? (0x1805) : (0x1804)
#define BLOCK7(x)	(x) ? (0x1c01) : (0x1c00)

int LD[8] = {0, 4095, 0, 4095, 0, 4095, 0, 4095};
int RD[8] = {0, 4095, 0, 4095, 0, 4095, 0, 4095};

static struct sock *nl_sk = NULL;
static long int data[ARRAY_EL];

static int simple_major = 241;
module_param(simple_major, int, 0);

static int TimerIntrpt = 0, die = 0, NeedSec = 0;
void intrpt_routine(struct work_struct *test);
static struct workqueue_struct *my_workqueue;
static DECLARE_DELAYED_WORK(Task, intrpt_routine);

volatile unsigned char *baseptr;

struct truedata {
	int timespk[12][MAX_CH];
	int energyspk[4][MAX_CH];
};
struct truedata spectras;

/*
typedef struct {
	int timespk[12][MAX_CH];
	int energyspk[4][MAX_CH];
} dataspk;

dataspk *truedata;
*/

static void nl_data_ready (struct sk_buff *skb)
{
	struct nlmsghdr *nlh = NULL;
	char buf[64];
	
	if(skb == NULL) {
		printk("skb is NULL \n");
		return ;
	}
	
	nlh = (struct nlmsghdr *)skb->data;
	printk(KERN_INFO "%s: received netlink message payload: %s\n", __FUNCTION__, NLMSG_DATA(nlh));
	
	strcpy(buf, NLMSG_DATA(nlh));
	NeedSec = ((int) simple_strtol( buf, NULL, 0));
	TimerIntrpt = 0;
	printk(KERN_INFO "NeedSec = %d\n", NeedSec);
	if(NeedSec == 0) die = 2;
}

static irqreturn_t jinr_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	printk(KERN_INFO "Interrupted\n");
//	printk(KERN_INFO, "0x0004 = %b 0x0000 = %b 0x0005 = %b", baseptr[0x0004], baseptr[0x0000], baseptr[0x0005]);
	
/*	short x, y, z;
	
	x = readw(baseptr+0x0804);
	x = (short)baseptr[0x0804];
	x = x & 0b0000111111111111;
	y = (short)baseptr[0x2000];
	z = (short)baseptr[0x1c00];
	data[x]++;
	
	printk(KERN_INFO "0x1 = %d 0x4 = %d 0x804 = %d 0x1c00 = %d 0x2000 = %d", (char)baseptr[0x0001], (char)baseptr[0x0004], x, z, y); */
	
	short N[NUM_BLOCKS];
	int i;
	static long int num_interrupts=0;
	
	num_interrupts++;
	
	for(i=0; i<=100; i++)
		if((char)baseptr[0x0001] == 94) break; 
	
/*	N[2] = (short)baseptr[BLOCK2];
	N[3] = (short)baseptr[BLOCK3];
	N[4] = (short)baseptr[BLOCK4];
	N[6] = (short)baseptr[BLOCK6];
	N[7] = (short)baseptr[BLOCK7];
	N[1] = (short)baseptr[BLOCK1]; */
	
	N[2] = 256*baseptr[BLOCK2(1)] + baseptr[BLOCK2(0)];
	N[3] = 256*baseptr[BLOCK3(1)] + baseptr[BLOCK3(0)];
	N[4] = 256*baseptr[BLOCK4(1)] + baseptr[BLOCK4(0)];
	N[6] = 256*baseptr[BLOCK6(1)] + baseptr[BLOCK6(0)];
	N[7] = 256*baseptr[BLOCK7(1)] + baseptr[BLOCK7(0)];
	N[1] = 256*baseptr[BLOCK1(1)] + baseptr[BLOCK1(0)];
	
	if(i==100) return IRQ_HANDLED;
	
	N[7] &= 0b00001111;
	/*
	switch(N[7]) {
		case 4 : {spectras.energyspk[0][N[2]]++; spectras.energyspk[1][N[3]]++; 
					if (N[2] >= LD[0] && N[2] <= LD[1] && N[3] >= RD[2] && N[3] <= RD[3])
						spectras.timespk[1][N[1]]++;
					else if (N[2] >= LD[2] && N[2] <= LD[3] && N[3] >= RD[0] && N[3] <= RD[1])
						spectras.timespk[0][N[1]]++;
					break;
					};
		case 8 : {spectras.energyspk[0][N[2]]++; spectras.energyspk[2][N[4]]++; 
					if (N[2] >= LD[0] && N[2] <= LD[1] && N[4] >= RD[4] && N[4] <= RD[5])
						spectras.timespk[3][N[1]]++;
					else if (N[2] >= LD[4] && N[2] <= LD[5] && N[4] >= RD[0] && N[4] <= RD[1])
						spectras.timespk[2][N[1]]++;
					break;
					};
		case 12 : {spectras.energyspk[0][N[2]]++; spectras.energyspk[3][N[6]]++; 
					if (N[2] >= LD[0] && N[2] <= LD[1] && N[6] >= RD[6] && N[6] <= RD[7])
						spectras.timespk[5][N[1]]++;
					else if (N[2] >= LD[6] && N[2] <= LD[7] && N[6] >= RD[0] && N[6] <= RD[1])
						spectras.timespk[4][N[1]]++;
					break;
					};
		case 9 : {spectras.energyspk[1][N[3]]++; spectras.energyspk[2][N[4]]++; 
					if (N[3] >= LD[2] && N[3] <= LD[3] && N[4] >= RD[4] && N[4] <= RD[5])
						spectras.timespk[7][N[1]]++;
					else if (N[3] >= LD[4] && N[3] <= LD[5] && N[4] >= RD[2] && N[4] <= RD[3])
						spectras.timespk[6][N[1]]++;
					break;
					};
		case 13 : {spectras.energyspk[1][N[3]]++; spectras.energyspk[3][N[6]]++; 
					if (N[3] >= LD[2] && N[3] <= LD[3] && N[6] >= RD[6] && N[6] <= RD[7])
						spectras.timespk[9][N[1]]++;
					else if (N[3] >= LD[6] && N[3] <= LD[7] && N[6] >= RD[2] && N[6] <= RD[3])
						spectras.timespk[8][N[1]]++;
					break;
					};
		case 14 : {spectras.energyspk[2][N[4]]++; spectras.energyspk[3][N[6]]++; 
					if (N[4] >= LD[4] && N[4] <= LD[5] && N[6] >= RD[6] && N[6] <= RD[7])
						spectras.timespk[11][N[1]]++;
					else if (N[4] >= LD[6] && N[4] <= LD[7] && N[6] >= RD[2] && N[6] <= RD[3])
						spectras.timespk[10][N[1]]++;
					break;
					};
		default: break;
	}
	*/


	int j;
	for(i=0; i<=MAX_CH-1; i++) {
		for(j=0; j<=3; j++)
			spectras.energyspk[j][i] = i*i;
		for(j=0; j<=11; j++)
			spectras.timespk[j][i] = i;
	}


	return IRQ_HANDLED;
}

static ssize_t my_read_file(struct file *file, char __user *userbuf,
                                size_t count, loff_t *ppos)
{
	copy_to_user(userbuf, &spectras, sizeof(struct truedata));

	return count;
} 

static ssize_t my_write_file(struct file *file, const char __user *buf,
                                size_t count, loff_t *ppos)
{
	printk(KERN_INFO "%d", count);
	if(count > sizeof(struct truedata)) {
		return -EINVAL;
		printk(KERN_INFO "Write error!\n");
	}
	copy_from_user(&spectras, buf, sizeof(struct truedata));
//	printk(KERN_INFO "%d", data[ARRAY_EL-2]);	

	return count;
}

/*
 * Set up the cdev structure for a device.
 */
static void simple_setup_cdev(struct cdev *dev, int minor,
                  struct file_operations *fops)
{
    int err, devno = MKDEV(simple_major, minor);
 
    cdev_init(dev, fops);
    dev->owner = THIS_MODULE;
    dev->ops = fops;
    err = cdev_add(dev, devno, 1);
    if (err)
        printk(KERN_NOTICE "Error %d adding simple %d", err, minor);
}

static struct file_operations my_fops =
{
	.read = my_read_file,
	.write = my_write_file,
};

static struct cdev SimpleDevs[1];

void intrpt_routine(struct work_struct *test)
{
	TimerIntrpt++;
	data[ARRAY_EL-1]++;
	 
	if (TimerIntrpt == NeedSec || die == 2) {
		printk(KERN_INFO "The end\n");
		die = 1;		/* keep intrp_routine from queueing itself */
		free_irq(IRQ_NUM, NULL);
	}
	 
	if (die == 0)
		queue_delayed_work(my_workqueue, &Task, TIMER_FREQ-1);
		
	printk(KERN_INFO "timer = %d", TimerIntrpt);
	
	printk(KERN_INFO "0x0c04 = %d", baseptr[0x0c04]); 
	
//	printk(KERN_INFO "0x0804 = %02x 0x4 = %02x ", (short)baseptr[0x0804], baseptr[0x4]);
	
//	printk(KERN_INFO "timerintr = %d 0x0804 = %04x 0x4 = %04x 0x1 = %04x\n", TimerIntrpt, (short)baseptr[0x0804], (short)baseptr[0x4], (short)baseptr[0x1]);

//	printk(KERN_INFO "0x1 = %d, 0x4 = %d, 0x804 = %d, 0x1c00 = %d, 0x2000 = %d", (char)baseptr[0x0001], (char)baseptr[0x0004], (short)baseptr[0x0804], baseptr[0x1c00], baseptr[0x2000]);
}

int init_module()
{
	int result;

	free_irq(IRQ_NUM, NULL);
	enable_irq(IRQ_NUM);
	printk(KERN_INFO "IRQ Enabled\n");
	
//	truedata = (dataspk *)kmalloc(sizeof(dataspk), GFP_KERNEL);
	
/*	if(!request_region(BASEPORT, 3, "my_drv"))
		printk(KERN_INFO "BASEPORT not available\n");
	else {
		printk(KERN_INFO "BASEPORT available\n");
	}*/
	
/*	check_mem_region(0xD0000, 0xF);
	if(check_mem_region(0xD0000, 0xF) < 0) { printk(KERN_INFO "check mem failed\n"); return -1; }
	
	request_mem_region(0xD0000, 0xF, "my_drv");
	
	baseptr = ioremap(0xD0000, 0xF);
	if(baseptr == NULL) { printk(KERN_INFO "ioremap failed\n"); return -1; }
	writeb(0,baseptr+0x2);
	writeb(0,baseptr+0x4);
	writeb(0b10000010,baseptr+0xF);
	printk(KERN_INFO "baseptr = %d\n", (int)baseptr);
	baseptr[0x4] = (char)0x0;
	baseptr[0x2] = (char)0x0;
	baseptr[0xF] = (char)130;*/
	
	baseptr = (unsigned char *)phys_to_virt(0xd0000);
	printk(KERN_INFO "baseptr = %p\n", baseptr);
//	printk(KERN_INFO "0x0004 = %d 0x0000 = %d 0x0005 = %d", baseptr[0x0004], baseptr[0x0000], baseptr[0x0005]);
	
	baseptr[0x4] = (char)0x0; //init crate #0
	baseptr[0x2] = (char)0x0; //select crate #0
//	baseptr[0xF] = (char)0x0; //reset signa inhibit - I
	baseptr[0x0] = (short)129; //set IRQ
	baseptr[0x1c20] = (char)0x0; //anti coinsidence N7
	
//	printk(KERN_INFO "0x0004 = %d 0x0000 = %d 0x0005 = %d", baseptr[0x0004], baseptr[0x0000], baseptr[0x0005]);
	
//	printk(KERN_INFO "0x0804 = %02x ", (short)baseptr[0x0804]);
   
   	if (request_irq(IRQ_NUM, jinr_interrupt, 0x00000020, "my_drv", 
NULL))
		printk(KERN_INFO "cannot register IRQ %d\n", (int)IRQ_NUM);
   
   /* QUEUE */
   	my_workqueue = create_workqueue(MY_WORK_QUEUE_NAME);
	queue_delayed_work(my_workqueue, &Task, 0);
   
   /* Char DEV */
    dev_t dev = MKDEV(simple_major, 0);
 
    /* Figure out our device number. */
    if (simple_major)
        result = register_chrdev_region(dev, 1, "simple");
    else {
        result = alloc_chrdev_region(&dev, 0, 1, "simple");
        simple_major = MAJOR(dev);
    }
    if (result < 0) {
        printk(KERN_WARNING "simple: unable to get major %d\n",
               simple_major);
        return result;
    }
    if (simple_major == 0)
        simple_major = result;
 
    /* Now set up two cdevs. */
    simple_setup_cdev(SimpleDevs, 0, &my_fops);

	printk(KERN_INFO "Initializing Netlink Socket");
	nl_sk = netlink_kernel_create(&init_net, NETLINK_NITRO, 0, nl_data_ready, NULL, THIS_MODULE);
	
	return 1;
}

void cleanup_module()
{
	release_region(BASEPORT, 3);
	free_irq(IRQ_NUM, NULL);
	iounmap(baseptr);

	die = 1;		/* keep intrp_routine from queueing itself */
	cancel_delayed_work(&Task);	/* no "new ones" */
	flush_workqueue(my_workqueue);	/* wait till all "old ones" finished */
	destroy_workqueue(my_workqueue);
	cdev_del(SimpleDevs);
	unregister_chrdev_region(MKDEV(simple_major, 0), 1);
	sock_release(nl_sk->sk_socket);
} 

MODULE_LICENSE("GPL");
