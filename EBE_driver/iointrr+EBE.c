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

int LD[8] = {100, 130, 75, 108, 65, 95, 100, 139};
int RD[8] = {140, 170, 116, 155, 103, 130, 148, 195};

static struct sock *nl_sk = NULL;
//static long int data[ARRAY_EL];

static int simple_major = 241;
module_param(simple_major, int, 0);

static int TimerIntrpt = 0, die = 0, NeedSec = 0;
void intrpt_routine(struct work_struct *test);
irqreturn_t jinr_interrupt(int irq, void *dev_id, struct pt_regs *regs);
static struct workqueue_struct *my_workqueue;
static DECLARE_DELAYED_WORK(Task, intrpt_routine);

volatile unsigned char *baseptr;

struct true_exp_data true_data_info;

struct file *test_file;

typedef struct {
	int timespk[12][MAX_CH];
	int energyspk[4][MAX_CH];
} dataspk;
dataspk *truedata;

struct true_exp_data {
	int status;
	
	int time, expos;
	
	int LeftRange[8];
	int RghtRange[8];
};

irqreturn_t jinr_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	int i, j;
	unsigned char N[12];
	static long int num_interrupts=-8;
//	static int spk[12][MAX_CH];
	
	static unsigned char M[4096];
	
	num_interrupts+=8;
	
	//read LAM L6+L4+L3+L2+L1; 94 - test code ADC L1-L6 (no L5)
	for(i=0; i<=200; i++)
		if((char)baseptr[0x0001] == 94) {break;} 
	
	if (true_data_info.status == 2) {
		
//	N[2] = (int)(baseptr[(BLOCK2(0))] + 256*(baseptr[(BLOCK2(1))] & 0b00001111));
	N[2] = baseptr[(BLOCK2(0))];
	N[3] = baseptr[(BLOCK2(1))] & 0b00001111;
	
//	N[3] = (int)(baseptr[(BLOCK3(0))] + 256*(baseptr[(BLOCK3(1))] & 0b00001111));
	N[4] = baseptr[(BLOCK3(0))];
	N[5] = baseptr[(BLOCK3(1))] & 0b00001111;
	
//	N[4] = (int)(baseptr[(BLOCK4(0))] + 256*(baseptr[(BLOCK4(1))] & 0b00001111));
	N[6] = baseptr[(BLOCK4(0))];
	N[7] = baseptr[(BLOCK4(1))] & 0b00001111;
	
//	N[6] = (int)(baseptr[(BLOCK6(0))] + 256*(baseptr[(BLOCK6(1))] & 0b00001111));
	N[8] = baseptr[(BLOCK6(0))];
	N[9] = baseptr[(BLOCK6(1))] & 0b00001111;
	
	N[10] = baseptr[(BLOCK7(0))];
	N[11] = baseptr[(BLOCK7(1))] & 0b00001111;
	
	N[0] = baseptr[(BLOCK1(0))];
	N[1] = baseptr[(BLOCK1(1))];
		
	//printk(KERN_INFO "Interrupted, TimerIntrpt = %d, num_interr = %ld, N0=%u, N1=%u, block7=%d\n", TimerIntrpt, num_interrupts, N[0], N[1], (int)(N[10] + 256*N[11]) & 0b00001111);
	
	//printk(KERN_INFO "`M0 = %u, M1 = %u\n", M[num_interrupts+0], M[num_interrupts+1]);

	switch((int)(N[10] + 256*N[11]) & 0b00001111) {
		case 4 : { 
					/*M[num_interrupts+2] = baseptr[(BLOCK2(0))];
					M[num_interrupts+3] = baseptr[(BLOCK2(1))];
					M[num_interrupts+4] = baseptr[(BLOCK3(0))];
					M[num_interrupts+5] = baseptr[(BLOCK3(1))];*/
					M[num_interrupts+2] = N[2];
					M[num_interrupts+3] = N[3];
					M[num_interrupts+4] = N[4];
					M[num_interrupts+5] = N[5];
			
					M[num_interrupts+6] = 1; //N7
					M[num_interrupts+7] = 2;
						
					break;
				};
		case 8 : {
					/*M[num_interrupts+2] = baseptr[(BLOCK2(0))];
					M[num_interrupts+3] = baseptr[(BLOCK2(1))];
					M[num_interrupts+4] = baseptr[(BLOCK4(0))];
					M[num_interrupts+5] = baseptr[(BLOCK4(1))];*/
					M[num_interrupts+2] = N[2];
					M[num_interrupts+3] = N[3];
					M[num_interrupts+4] = N[6];
					M[num_interrupts+5] = N[7];
			
					M[num_interrupts+6] = 1; //N7
					M[num_interrupts+7] = 3;
					
					break;
				};
		case 12 : {
					/*M[num_interrupts+2] = baseptr[(BLOCK2(0))];
					M[num_interrupts+3] = baseptr[(BLOCK2(1))];
					M[num_interrupts+4] = baseptr[(BLOCK6(0))];
					M[num_interrupts+5] = baseptr[(BLOCK6(1))];*/
					M[num_interrupts+2] = N[2];
					M[num_interrupts+3] = N[3];
					M[num_interrupts+4] = N[8];
					M[num_interrupts+5] = N[9];
			
					M[num_interrupts+6] = 1; //N7
					M[num_interrupts+7] = 4;
	
					break;
				};
		case 9 : {
					/*M[num_interrupts+2] = baseptr[(BLOCK3(0))];
					M[num_interrupts+3] = baseptr[(BLOCK3(1))];
					M[num_interrupts+4] = baseptr[(BLOCK4(0))];
					M[num_interrupts+5] = baseptr[(BLOCK4(1))];*/
					M[num_interrupts+2] = N[4];
					M[num_interrupts+3] = N[5];
					M[num_interrupts+4] = N[6];
					M[num_interrupts+5] = N[7];
				
					M[num_interrupts+6] = 2; //N7
					M[num_interrupts+7] = 3;
	
					break;
				};
		case 13 : {
					/*M[num_interrupts+2] = baseptr[(BLOCK3(0))];
					M[num_interrupts+3] = baseptr[(BLOCK3(1))];
					M[num_interrupts+4] = baseptr[(BLOCK6(0))];
					M[num_interrupts+5] = baseptr[(BLOCK6(1))];*/
					M[num_interrupts+2] = N[4];
					M[num_interrupts+3] = N[5];
					M[num_interrupts+4] = N[8];
					M[num_interrupts+5] = N[9];
			
					M[num_interrupts+6] = 2; //N7
					M[num_interrupts+7] = 4;
			
					break;
				};
		case 14 : {
					/*M[num_interrupts+2] = baseptr[(BLOCK4(0))];
					M[num_interrupts+3] = baseptr[(BLOCK4(1))];
					M[num_interrupts+4] = baseptr[(BLOCK6(0))];
					M[num_interrupts+5] = baseptr[(BLOCK6(1))];*/
					M[num_interrupts+2] = N[6];
					M[num_interrupts+3] = N[7];
					M[num_interrupts+4] = N[8];
					M[num_interrupts+5] = N[9];
			
					M[num_interrupts+6] = 3; //N7
					M[num_interrupts+7] = 4;
			
					break;
				};
		default: break;
	}
	M[num_interrupts+0] = N[0]; //N1
	M[num_interrupts+1] = N[1];
	
	if (num_interrupts >= 4096-1) {
		mm_segment_t oldfs;
		oldfs = get_fs();
		set_fs(get_ds());
		
		char *str_to_write = (char *)kmalloc(32*512*sizeof(char), GFP_KERNEL);
		char *str_temp = (char *)kmalloc(32*sizeof(char), GFP_KERNEL);
		
		sprintf(str_temp, "%u %u %u %u %u %u %u %u\n", M[0], M[1], M[2], M[3], M[4], M[5], M[6], M[7]);
		printk(KERN_INFO "str_temp = %s", str_temp);
		strcpy(str_to_write, str_temp);
		
		for(j=1; j<512; j++) {
			if(M[8*j+0] == M[8*j+1] && M[8*j+0] == 0) printk(KERN_INFO "Warning! all m=0 i=%d\n", j); 
			sprintf(str_temp, "%u %u %u %u %u %u %u %u\n", M[8*j+0], M[8*j+1], M[8*j+2], M[8*j+3], M[8*j+4], M[8*j+5], M[8*j+6], M[8*j+7]);
			strcat(str_to_write, str_temp);
		}
		
	//	printk(KERN_INFO "str = %s; m0 = %u, m1 = %u\n", str, M[0], M[1]);
		vfs_write(test_file, M, 4096*sizeof(unsigned char), &test_file->f_pos);
		
		//vfs_write(test_file, (char *)M, 4096, &test_file->f_pos);
		//vfs_write(test_file, str, 8, &test_file->f_pos);
		
		//memset(M, 0, 4096);
		
		num_interrupts = -8;
		
		kfree(str_to_write);
		kfree(str_temp);
	}
	}

	return IRQ_HANDLED;
}

static void trueread_nl_msg(const char *msg, struct true_exp_data *struct_to_info)
{	
	int i;
	int truetime = 0;
	int expos_truetime = 0;
	
	int status_pos = (sizeof("STATUS=")-1)/sizeof(char);
	int time_pos = status_pos + 1 + 1 + (sizeof("TIME=")-1)/sizeof(char);
	int expos_pos = time_pos + 6 + 1 + (sizeof("EXPOS=")-1)/sizeof(char);
	int LD_pos[8], RD_pos[8];
	
	struct_to_info->status = msg[status_pos]-'0';
	
	truetime = 100000*(msg[time_pos]-'0') + 10000*(msg[time_pos+1]-'0') + 1000*(msg[time_pos+2]-'0') + 100*(msg[time_pos+3]-'0') + 10*(msg[time_pos+4]-'0') + 1*(msg[time_pos+5]-'0');
	struct_to_info->time = truetime;
	
	expos_truetime = 100000*(msg[expos_pos]-'0') + 10000*(msg[expos_pos+1]-'0') + 1000*(msg[expos_pos+2]-'0') + 100*(msg[expos_pos+3]-'0') + 10*(msg[expos_pos+4]-'0') + 1*(msg[expos_pos+5]-'0');
	struct_to_info->expos = expos_truetime;
	
	for(i=0; i<8; i+=2) {
		LD_pos[i] = expos_pos + 6 + 1 + 1 + 22*i/2;
		RD_pos[i] = expos_pos + 6 + 1 + 1 + 10 + 22*i/2;
	}
	
	for(i=1; i<8; i+=2) {
		LD_pos[i] = expos_pos + 6 + 1 + 1 + 22*(i-1)/2 + 5;
		RD_pos[i] = expos_pos + 6 + 1 + 1 + 10 + 22*(i-1)/2 + 5;
	}
	
	for(i=0; i<8; i++) {
		(struct_to_info->LeftRange)[i] = 1000*(msg[LD_pos[i]]-'0') + 100*(msg[LD_pos[i]+1]-'0') + 10*(msg[LD_pos[i]+2]-'0') + 1*(msg[LD_pos[i]+3]-'0');
		(struct_to_info->RghtRange)[i] = 1000*(msg[RD_pos[i]]-'0') + 100*(msg[RD_pos[i]+1]-'0') + 10*(msg[RD_pos[i]+2]-'0') + 1*(msg[RD_pos[i]+3]-'0');
	}
}

static void nl_data_ready (struct sk_buff *skb)
{
	int i, j;
	struct nlmsghdr *nlh = NULL;
	char msg[1024];
	
	if(skb == NULL) {
		printk("skb is NULL \n");
		return ;
	}
	
	nlh = (struct nlmsghdr *)skb->data;
	printk(KERN_INFO "%s: received netlink message payload: %s\n", __FUNCTION__, (char *)NLMSG_DATA(nlh));
	
	strcpy(msg, NLMSG_DATA(nlh));
	
	trueread_nl_msg(msg, &true_data_info);
	
	switch(true_data_info.status) {
		case 0 : {	die = 2; //stop
			
					break;
				};
		case 1 : {	die = 2; //pause
			
					break;
				};
		case 2 : {	die = 0; //start
			
					NeedSec = true_data_info.time;
					TimerIntrpt = 0;
					queue_delayed_work(my_workqueue, &Task, 0);
					printk(KERN_INFO "NeedSec = %d\n", NeedSec);
		
					for(i=0; i<8; i++) {
						LD[i] = true_data_info.LeftRange[i];
						RD[i] = true_data_info.RghtRange[i];
					}
					break;
				};
		case 3 : {	// clear

					for(i=0; i<=MAX_CH-1; i++) {
						for(j=0; j<4; j++)
							truedata->energyspk[j][i] = 0;
						for(j=0; j<12; j++)
							truedata->timespk[j][i] = 0;
					}
					
					break;
				};
		default: break;
	}
	
	/*if(true_data_info.status == 2) {
		NeedSec = true_data_info.time;
		printk(KERN_INFO "NeedSec = %d\n", NeedSec);
		
		for(i=0; i<8; i++) {
			LD[i] = true_data_info.LeftRange[i];
			RD[i] = true_data_info.RghtRange[i];
		}
	} */

}

void file_close(struct file* file) {
    filp_close(file, NULL);
}

static ssize_t my_read_file(struct file *file, char __user *userbuf,
                                size_t count, loff_t *ppos)
{
/*	int i;
	for(i=0; i<=11; i++)
		truedata->timespk[i][3968] = 0; */
	
	copy_to_user(userbuf, truedata, sizeof(dataspk));
	printk(KERN_INFO "/dev/simple read");

	return count;
} 

static ssize_t my_write_file(struct file *file, const char __user *buf,
                                size_t count, loff_t *ppos)
{
	printk(KERN_INFO "%d", count);
	if(count > sizeof(dataspk)) {
		return -EINVAL;
		printk(KERN_INFO "Write error!\n");
	}
	copy_from_user(truedata, buf, sizeof(dataspk));
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
//	data[ARRAY_EL-1]++;
	 
	if (TimerIntrpt == 3600 || die == 2) {
		printk(KERN_INFO "The end\n");
		die = 1;		/* keep intrp_routine from queueing itself */
		true_data_info.status = 0;
    }
	 
	if (die == 0)
		queue_delayed_work(my_workqueue, &Task, TIMER_FREQ-1);
		
//	printk(KERN_INFO "timer = %d", TimerIntrpt);
	
//	printk(KERN_INFO "e100 = %d e1950 = %d", truedata->energyspk[0][100], truedata->energyspk[0][1950]); 
	
//	printk(KERN_INFO "0x0804 = %02x 0x4 = %02x ", (short)baseptr[0x0804], baseptr[0x4]);
	
//	printk(KERN_INFO "timerintr = %d 0x0804 = %04x 0x4 = %04x 0x1 = %04x\n", TimerIntrpt, (short)baseptr[0x0804], (short)baseptr[0x4], (short)baseptr[0x1]);

//	printk(KERN_INFO "0x1 = %d, 0x4 = %d, 0x804 = %d, 0x1c00 = %d, 0x2000 = %d", (char)baseptr[0x0001], (char)baseptr[0x0004], (short)baseptr[0x0804], baseptr[0x1c00], baseptr[0x2000]);
}

int init_module()
{
	int i, j;
	int result;
	/* Char DEV */
    dev_t dev = MKDEV(simple_major, 0);

	free_irq(IRQ_NUM, NULL);
	enable_irq(IRQ_NUM);
	printk(KERN_INFO "IRQ Enabled\n");
	
	truedata = (dataspk *)kmalloc(sizeof(dataspk), GFP_KERNEL);
/*	truedata->timespk = (int **)kmalloc(12*sizeof(int *), GFP_KERNEL);
	truedata->energyspk = (int **)kmalloc(4*sizeof(int *), GFP_KERNEL);
	int i, j;
	for(i=0; i<=11; i++)
		truedata->timespk[i] = (int *)kmalloc(MAX_CH*sizeof(int), GFP_KERNEL);
	for(i=0; i<=3; i++)
		truedata->energyspk[i] = (int *)kmalloc(MAX_CH*sizeof(int), GFP_KERNEL);
		
	for(i=0; i<=MAX_CH-1; i++) {
		for(j=0; j<=3; j++)
			truedata->energyspk[j][i] = 1;
		for(j=0; j<=11; j++)
			truedata->timespk[j][i] = 1;
	} */
	
/*	int i;
	truedata = (dataspk *)kmalloc(sizeof(dataspk), GFP_KERNEL);
	truedata->timespk = (int **)kmalloc(sizeof(int **)*12, GFP_KERNEL);
	for(i=0; i<=11; i++)
		truedata->timespk[i] = (int *)kmalloc(sizeof(int)*MAX_CH, GFP_KERNEL);
	truedata->energyspk = (int **)kmalloc(sizeof(int **)*4, GFP_KERNEL);
	for(i=0; i<=3; i++)
		truedata->energyspk[i] = (int *)kmalloc(sizeof(int)*MAX_CH, GFP_KERNEL);
	*/
/*	int i, j;
	for(i=0; i<=MAX_CH-1; i++) {
		for(j=0; j<=3; j++)
			truedata->energyspk[j][i] = 0;
		for(j=0; j<=11; j++)
			truedata->timespk[j][i] = 0;
	}*/
	
	for(j=0; j<=3; j++)
		for(i=0; i<=MAX_CH-1; i++)
			truedata->energyspk[j][i] = 0;
	for(j=0; j<=11; j++)
		for(i=0; i<=MAX_CH-1; i++)
			truedata->timespk[j][i] = 0;
	
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
	
	printk(KERN_INFO "0x0001=%d 0x0004=%d 0x0804=%d 0x0c04=%d 0x1004=%d 0x1804=%d 0x1c00=%d 0x0404=%d 0x0005=%d",\
	(short)baseptr[0x0001], (short)baseptr[0x0004], (short)baseptr[0x0804], \
	(short)baseptr[0x0c04], (short)baseptr[0x1004], (short)baseptr[0x1804], \
	(short)baseptr[0x1c00], (short)baseptr[0x0404], (short)baseptr[0x0005]);
	
//	printk(KERN_INFO "0x0804 = %02x ", (short)baseptr[0x0804]);
   
 	if (request_irq(IRQ_NUM, jinr_interrupt, 0x00000020, "my_drv", NULL))
		printk(KERN_INFO "cannot register IRQ %d\n", (int)IRQ_NUM);
   
   /* QUEUE */
   	my_workqueue = create_workqueue(MY_WORK_QUEUE_NAME);
	queue_delayed_work(my_workqueue, &Task, 0);
 
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
	
	test_file = filp_open("/root/job/EBE_driver/test_file", O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if(test_file == NULL)
		printk(KERN_INFO "Open error\n");
	true_data_info.status = 2;
	
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
	
	file_close(test_file);
} 

MODULE_LICENSE("GPL");
