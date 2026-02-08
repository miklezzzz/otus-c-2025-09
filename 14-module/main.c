#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/ioctl.h>

#define DEVICE_NAME "ring_buffer"
#define CLASS_NAME "ring_buffer"
#define IOCTL_MAGIC 'N'
#define IOCTL_WIPE_RB _IO(IOCTL_MAGIC, 0)
#define DEFAULT_SIZE 16

MODULE_LICENSE("GPL");
static dev_t dev_num;
static struct cdev rb_cdev;
static struct class* rb_class;
static struct device* rb_device;
static DEFINE_MUTEX(device_mutex);

// def ring buffer struct
typedef struct ring_buffer {
	char *buffer;
	size_t size;
	int head;
	int tail;
	int count;
	struct mutex lock;
} ring_buffer;

static ring_buffer* rb;

// open operation
static int rb_open(struct inode* inode, struct file* file) {
	if (!mutex_trylock(&device_mutex)) {
		printk(KERN_ERR "Device is busy\n");
		return -EBUSY;
	}
	
	return 0;
}

// release operation
static int rb_release(struct inode* inode, struct file* file) {
	mutex_unlock(&device_mutex);
	
	return 0;
}

// read operation
static ssize_t rb_read(struct file* file, char __user* user_buf, size_t count, loff_t* offset) {
	if (mutex_lock_interruptible(&rb->lock)) {
	       	return -ERESTARTSYS;
	}

	int to_read = (count < rb->count) ? count : rb->count;
	for (int i = 0; i < to_read; i++) {
		if (copy_to_user(user_buf + i, &rb->buffer[rb->tail], 1)) {
			mutex_unlock(&rb->lock);
			return -EFAULT;
		}

		rb->tail = (rb->tail + 1) % rb->size;
		rb->count--;
	}

	mutex_unlock(&rb->lock);
	*offset += to_read;

	return to_read;
}

// write operation
static ssize_t rb_write(struct file* file, const char __user* user_buf, size_t count, loff_t* offset) {
	if (mutex_lock_interruptible(&rb->lock)) {
	       	return -ERESTARTSYS;
	}

	char k_byte;

	for (int i = 0; i < count; i++) {
		if (copy_from_user(&k_byte, user_buf + i, 1)) {
			mutex_unlock(&rb->lock);
			return -EFAULT;
		}

		rb->buffer[rb->head] = k_byte;
		rb->head = (rb->head + 1) % rb->size;

		if (rb->count < rb->size) {
			rb->count++;
		} else {
			rb->tail = (rb->tail + 1) % rb->size;
		}
	}

	mutex_unlock(&rb->lock);
	*offset += count;

	return count;
}

// ioctl - lazily wipes the ring buffer
static long rb_ioctl(struct file* file, unsigned int cmd, unsigned long arg) {
	if (cmd != IOCTL_WIPE_RB) {
		return -EINVAL;
	}

	if (mutex_lock_interruptible(&rb->lock)) {
		return -ERESTARTSYS;
	}

	rb->head = rb->tail = rb->count = 0;
	mutex_unlock(&rb->lock);
	printk(KERN_WARNING "Ring buffer was wiped out\n");

	return 0;
}

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = rb_open,
	.release = rb_release,
	.read = rb_read,
	.write = rb_write,
	.unlocked_ioctl = rb_ioctl,
};

// ring buffer init
static int init_rb(int size) {
	rb = kmalloc(sizeof(struct ring_buffer), GFP_KERNEL);
	rb->head = 0;
	rb->tail = 0;
	rb->count = 0;
	rb->size = size;
	rb->buffer = kmalloc(rb->size, GFP_KERNEL);
	if (!rb->buffer) {
        	kfree(rb);
        	return -ENOMEM;
	}

	mutex_init(&rb->lock);

	return 0;
}

// ring buffer clean up
static void cleanup_rb(void) {
	mutex_unlock(&rb->lock);
	kfree(rb);
}

// get ring buffer current size via sysfs
static ssize_t buffer_size_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct ring_buffer *rb = dev_get_drvdata(dev);
	return sprintf(buf, "%zu\n", rb->size);
}

// udpdate ring buffer size via sysfs
static ssize_t buffer_size_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count) {
	struct ring_buffer *rb = dev_get_drvdata(dev);
	size_t new_size;
	char *new_buf, *old_buf;

	unsigned long new_size_val;
	if (kstrtoul(buf, 10, &new_size_val)) {
		return -EINVAL;
	}
	new_size = (size_t)new_size_val;

	if (new_size < 1) {
		return -EINVAL;
	}

	if (mutex_lock_interruptible(&rb->lock)) {
		return -ERESTARTSYS;
	}

	new_buf = kzalloc(new_size, GFP_KERNEL);
	if (!new_buf) {
		mutex_unlock(&rb->lock);
		return -ENOMEM;
	}

	old_buf = rb->buffer;
	rb->buffer = new_buf;
	rb->size = new_size;
	rb->head = rb->tail = rb->count = 0;

	kfree(old_buf);
	mutex_unlock(&rb->lock);

	return count;
}

static DEVICE_ATTR_RW(buffer_size);

static int __init chardev_init(void) {
	int ret;
	if ((ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME)) < 0) {
		printk(KERN_ERR "Failed to allocate char dev region\n");
		return ret;
	}

	cdev_init(&rb_cdev, &fops);
	rb_cdev.owner = THIS_MODULE;
	if ((ret = cdev_add(&rb_cdev, dev_num, 1)) < 0) {
		unregister_chrdev_region(dev_num, 1);
		return ret;
	}

	rb_class = class_create(CLASS_NAME);
	if (IS_ERR(rb_class)) {
		cdev_del(&rb_cdev);
		unregister_chrdev_region(dev_num, 1);
		return PTR_ERR(rb_class);
	}

	rb_device = device_create(rb_class, NULL, dev_num, NULL, DEVICE_NAME);
	if (IS_ERR(rb_device)) {
		class_destroy(rb_class);
		cdev_del(&rb_cdev);
		unregister_chrdev_region(dev_num, 1);
		return PTR_ERR(rb_device);
	}

	ret = init_rb(DEFAULT_SIZE);
	if (ret != 0) {
		return ret;
	}

	dev_set_drvdata(rb_device, rb);

	if (device_create_file(rb_device, &dev_attr_buffer_size)) {
		pr_err("Failed to create sysfs file\n");
	}

	return 0;
}

static void __exit chardev_exit(void) {
	device_destroy(rb_class, dev_num);
	class_destroy(rb_class);
	cdev_del(&rb_cdev);
	unregister_chrdev_region(dev_num, 1);
	cleanup_rb();
}

module_init(chardev_init);
module_exit(chardev_exit);
