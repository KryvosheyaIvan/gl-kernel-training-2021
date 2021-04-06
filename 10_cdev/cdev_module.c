// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause

#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>

MODULE_AUTHOR("Ivan Kryvosheia");
MODULE_DESCRIPTION("Character device module for Linux Kernel ProCamp");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION("1.0");

//#define DB

#define DEV_NAME		"gl_chat"
#define BASE_MINOR		0
#define NUM_DEVICES		1
#define CLASS			"gl_class"
#define BUFF_DEFAULT_SIZE	1024
#define BUFF_PARAM_PERM		0644

unsigned int buff_size = BUFF_DEFAULT_SIZE;
module_param(buff_size, uint, BUFF_PARAM_PERM);
MODULE_PARM_DESC(buff_size, "Messages max size in bytes");


static int init_cdev(void);
static void deinit_cdev(void);

static int devc_open(struct inode *inode, struct file *filp);
static int devc_release(struct inode *inode, struct file *file);
static long devc_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static ssize_t devc_read(struct file *file, char __user *buf, size_t count,
	loff_t *offset);
static ssize_t devc_write(struct file *file, const char __user *buf,
	size_t count, loff_t *offset);

struct cdev_module_data {
	struct cdev cdev;
	uint8_t *data;
};

static dev_t my_dev;

/* cdev is here + module specific data */
static struct cdev_module_data cdev_specific_data;

/* holds major nu,ber of char dev */
static int dev_major;

static struct class *gl_class;

static const struct file_operations mychardev_fops = {
	.owner		= THIS_MODULE,
	.open		= devc_open,
	.release	= devc_release,
	.unlocked_ioctl	= devc_ioctl,
	.read		= devc_read,
	.write		= devc_write
};

static int devc_open(struct inode *inode, struct file *filp)
{
	unsigned int maj = imajor(inode);
	unsigned int min = iminor(inode);
	struct cdev_module_data *pcdev_st;

	pcdev_st = container_of(inode->i_cdev, struct cdev_module_data, cdev);

	if (maj != dev_major || min < 0) {
		pr_err("major num err\n");
		return -ENODEV; /* No such device */
	}

	/* prepare the buffer if the device is opened for the first time */
	if (pcdev_st->data == NULL) {
		pcdev_st->data = kzalloc(buff_size, GFP_KERNEL);
		if (pcdev_st->data == NULL) {
			#ifdef DB
			pr_err("Open: memory allocation failed\n");
			#endif
			return -ENOMEM;
		}
	}

	/* set this pinter to our data (to use it in another callbacks */
	filp->private_data = pcdev_st->data;

	/* offset is zeroed at every open */

	#ifdef DB
	pr_info("devc: Device open\n");
	#endif

	return 0;
}


static int devc_release(struct inode *inode, struct file *file)
{
	#ifdef DB
	pr_info("devc: Device close\n");
	#endif

	return 0;
}

static long devc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	#ifdef DB
	pr_info("devc: Device ioctl\n");
	#endif

	return 0;
}

static ssize_t devc_read(struct file *file, char __user *buf, size_t count,
	loff_t *offset)
{
	bool not_sent;
	int buf_cont_len;

	/* its for discovering caveats */
	/* pr_info("devc: Device read. Bytes to read: %u\n", count); */
	/* pr_info("offset: %lld\n", *offset); */
	#ifdef DB
	pr_info("devc: Device read\n");
	#endif

	buf_cont_len = strlen(file->private_data);
	if (buf_cont_len == 0)
		return 0;

	pr_info("%s", (char *)file->private_data);

	/* offset plays role only if writing is in several fun. calls */

	/* Copy the data from kernel to the user-space buffer */
	not_sent = copy_to_user(buf, file->private_data, buf_cont_len);
	if (not_sent) {
		pr_err("not sent\n");
		return -EFAULT;
	}

	/* zeroing buffer */
	memset(file->private_data, 0, buf_cont_len);

	return buf_cont_len;
}

static ssize_t devc_write(struct file *file, const char __user *buf,
	size_t count, loff_t *offset)
{
	int copy;

	/* its for discovering caveats */
	/* pr_info("devc: Device write. count: %u, offset: %lld\n", count, */
	/*          *offset); */
	#ifdef DB
	pr_info("devc: Device write\n");
	#endif

	if (count > BUFF_DEFAULT_SIZE)
		copy = BUFF_DEFAULT_SIZE;
	else
		copy = count;

	*offset = strlen(file->private_data);

	if ((*offset + copy) > BUFF_DEFAULT_SIZE) {
		pr_err("buffer volume: %lld / %d\n", *offset,
		       BUFF_DEFAULT_SIZE);

		/* always return count to prevent cycling */
		return count;
	}

	if (copy_from_user(file->private_data + *offset, buf, copy) != 0)
		return copy;

	/* update buffer offset */
	*offset = copy + *offset;

	#ifdef DB
	pr_info("upd offset: %lld\n", *offset);
	pr_info("wr: %s", (char *)file->private_data);
	#endif

	return count;
}

static int init_cdev(void)
{
	int rc;
	//int i;

	rc = alloc_chrdev_region(&my_dev, BASE_MINOR, NUM_DEVICES, DEV_NAME);
	if (rc) {
		pr_err("Failed to allocate region\n");
		goto init_cdev_err;
	}

	/* Let's create our device's class, visible in /sys/class */
	gl_class = class_create(THIS_MODULE, CLASS);

	/* get dev major number */
	dev_major = MAJOR(my_dev);

	/* initialize a cdev structure */
	cdev_init(&cdev_specific_data.cdev, &mychardev_fops);

	cdev_specific_data.cdev.owner = THIS_MODULE;

	/* Now make the device live for the users to access */
	cdev_add(&cdev_specific_data.cdev, MKDEV(dev_major, 0), 1);

	/* create device node /dev/gl_class-x where "x" is "i", */
	/* equal to the Minor number */
	device_create(gl_class,
			NULL,			/* no parent */
			MKDEV(dev_major, 0),
			NULL,			/* no additional data */
			DEV_NAME"-%d", 0);	/*gl_chat-x */

	/* ok */
	return 0;

init_cdev_err:

	return -1;
}

static void deinit_cdev(void)
{
	int i;

	pr_info("Deiniting character device...\n");

	for (i = 0; i < NUM_DEVICES; i++)
		device_destroy(gl_class, MKDEV(dev_major, i));

	class_unregister(gl_class);
	class_destroy(gl_class);

	unregister_chrdev_region(MKDEV(dev_major, 0), NUM_DEVICES);
}

static int __init cdev_module_init(void)
{
	int rc;

	/* check input from user */
	if (buff_size != BUFF_DEFAULT_SIZE) {
		if (buff_size < BUFF_DEFAULT_SIZE) {
			pr_err("Buffer size must be greater than 1k bytes\n");
			return -1;
		}
	}

	rc = init_cdev();
	if (rc != 0) {
		pr_err("Failed to init character device\n");
		return -1;
	}

	pr_info("ProCamp CDEV Module inserted\n");

	return 0;
}

static void __exit cdev_module_exit(void)
{
	pr_info("ProCamp CDEV Module removed\n");

	kzfree(cdev_specific_data.data);

	deinit_cdev();
}

module_init(cdev_module_init);
module_exit(cdev_module_exit);
