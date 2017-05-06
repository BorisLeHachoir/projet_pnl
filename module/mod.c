#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
/* required for various structures related to files liked fops. */
#include <linux/semaphore.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/version.h>
#include <linux/uaccess.h>

#include "ioctl_basics.h"

static int Major;
static struct wordqueue_struct *my_wq;
int open(struct inode *inode, struct file *filp)
{
	printk (KERN_INFO "Inside open \n");
	return 0;
}

int release(struct inode *inode, struct file *filp)
{
	printk (KERN_INFO "Inside close \n");
	return 0;
}

static void kill_function( struct work_struct *wk)
{
 struct kill_work * work = (struct kill_work *)wk;

	pr_info("[KILL_FUNCTION] signal: %d pid: %d\n", work->signal, work->pid);

	kfree( (void *)wk);

	return;
}

static void wait_function( struct work_struct *wk)
{
	int i;
	struct wait_work * work = (struct wait_work *)wk;

	pr_info("[WAIT_FUNCTION] size: %d ", work->size);
	for(i = 0; i < work->size; i++)
	 pr_info("| pids[%d]: %d ",i, work->pids[i]);
								
	pr_info("\n");

	kfree( (void *)wk);

	return;
}
static void modinfo_function( struct work_struct *wk)
{
 struct modinfo_work * work = (struct modinfo_work *)wk;
	
 pr_info("[MODINFO_FUNCTION] name: %s\n", work->name);

	kfree( (void *)wk);

	return;
}

long ioctl_funcs(struct file *filp, unsigned int cmd, unsigned long arg)
{

	struct mesg_kill mesg_kill;
	struct mesg_wait mesg_wait;
	struct mesg_modinfo mesg_mod;
 
 struct kill_work *kill_work;
 struct wait_work *wait_work;
 struct modinfo_work *modinfo_work;

	int i;

	switch (cmd) {

	case IOCTL_KILL:
		pr_info("ASKED KILL");
		copy_from_user(&mesg_kill, (char *)arg, sizeof(struct mesg_kill));
		pr_info("recv from user: kill %d %d %c\n",
		mesg_kill.signal, mesg_kill.pid, mesg_kill.async ? '&':' ');

  kill_work = (struct kill_work *)kmalloc(sizeof(struct kill_work), GFP_KERNEL);
  if(kill_work){
  
   INIT_WORK( (struct work_struct *)kill_work, kill_function);
  
   kill_work->signal = mesg_kill.signal;
   kill_work->pid = mesg_kill.pid;

   if(! queue_work( func_wq, (struct work_struct *) kill_work)){
    
     pr_info("work was already on a queue\n");
     kfree ( (void *)kill_work);
     return -1;
   }
  }
  else{
   pr_info("kmalloc failed\n");
   return -1;
  }
   
		break;

	case IOCTL_WAIT:
		printk (KERN_INFO "Asked WAIT");
		copy_from_user(&mesg_wait, (char *)arg, sizeof(struct mesg_wait));
		pr_info("recv from user: wait ");
		for (i = 0 ; i < mesg_wait.size ; ++i) {
			pr_info("%d ", mesg_wait.pids[i]);
		}

		pr_info("%c\n", mesg_wait.async ? '&':' ');

  wait_work = (struct wait_work *)kmalloc(sizeof(struct wait_work), GFP_KERNEL);
  if(wait_work){
  
   INIT_WORK( (struct wait_struct *)wait_work, wait_function);
  
   wait_work->size = mesg_wait.size;
   for (i = 0; i < mesg_wait.size ; ++i) 
    wait_work->pids[i] = mesg_wait.pids[i];

   if(! queue_work( func_wq, (struct work_struct *) wait_work)){
    
     pr_info("work was already on a queue\n");
     kfree ( (void *)wait_work);
     return -1;
   }
  }
  else{
   pr_info("kmalloc failed\n");
   return -1;
  }
 

		break;

	case IOCTL_MEMINFO:
		printk (KERN_INFO "Asked MEMINFO");
		copy_from_user(&i, (char *)arg, sizeof(int));
		pr_info("recv from user: meminfo %c\n", i ? '&':' ');
		break;

	case IOCTL_MODINFO:
		printk (KERN_INFO "Asked MODINFO");
		copy_from_user(&mesg_mod, (char *)arg, sizeof(struct mesg_modinfo));
		pr_info("recv from user: modinfo %s %c\n",
		mesg_mod.name, mesg_mod.async ? '&':' ');

  modinfo_work = (struct modinfo_work *)kmalloc(sizeof(struct modinfo_work), GFP_KERNEL);
  if(modinfo_work){
  
   INIT_WORK( (struct work_struct *)modinfo_work, modinfo_function);
  
   modinfo_work->name = strncpy( modinfo_work->name, mesg_mod.name, (size_t)BUFF_SIZE); 

   if(! queue_work( func_wq, (struct work_struct *) modinfo_work)){
    
     pr_info("work was already on a queue\n");
     kfree ( (void *)modinfo_work);
     return -1;
   }
  }
  else{
   pr_info("kmalloc failed\n");
   return -1;
  }

		break;
	}

	return 0;
}

struct file_operations fops = {
	open: open,
	unlocked_ioctl : ioctl_funcs,
	release : release
};


struct cdev *kernel_cdev;


int char_arr_init (void)
{
	int ret;
	dev_t dev_no, dev;

	kernel_cdev = cdev_alloc();
	kernel_cdev->ops = &fops;
	kernel_cdev->owner = THIS_MODULE;
	printk (" Inside init module\n");
	ret = alloc_chrdev_region(&dev_no, 0, 1, "char_arr_dev");

	if (ret < 0) {
		printk ("Major number allocation is failed\n");
		return ret;
	}

	Major = MAJOR(dev_no);
	dev = MKDEV(Major, 0);
	printk (" The major number for your device is %d\n", Major);
	printk (" usage: sudo mknod /dev/temp c %d 0\n", Major);
	ret = cdev_add(kernel_cdev, dev, 1);
 
	if (ret < 0) {
		printk (KERN_INFO "Unable to allocate cdev");
		return ret;
	}

 func_wq = create_workqueue("function_queue");

 
	return 0;
}

void char_arr_cleanup(void)
{
	printk (KERN_INFO " Inside cleanup_module\n");
	cdev_del(kernel_cdev);
	unregister_chrdev_region(Major, 1);

 flush_workqueue( func_wq );
 destroy_workqueue( func_wq );

 return;
}
MODULE_LICENSE("GPL");
module_init(char_arr_init);
module_exit(char_arr_cleanup);
