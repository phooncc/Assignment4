#include <linux/module.h> 
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/ioctl.h>
#define size 4194304//4194304//8,16,32, ... . . . .. 2^22 is the max, I can only max: 2^17
#define MAJOR_NUMBER 61
#define SCULL_IOC_MAGIC 'k'
#define SCULL_HELLO _IO(SCULL_IOC_MAGIC, 1)
#define SCULL_DEV_MSG _IOW(SCULL_IOC_MAGIC, 2, int)
#define SCULL_DEV_MSG_LEN _IOW(SCULL_IOC_MAGIC, 3, int)
#define SCULL_DEV_MSG_TO_USER _IOR(SCULL_IOC_MAGIC, 4, long)
#define SCULL_DEV_MSG_BI _IOWR(SCULL_IOC_MAGIC, 5, long)
#define SCULL_IOC_MAXNR 5

/* forward declaration */
int onebyte_open(struct inode *inode, struct file *filep);
int onebyte_release(struct inode *inode, struct file *filep);
ssize_t onebyte_read(struct file *filep, char *buf, size_t
count, loff_t *f_pos);
ssize_t onebyte_write(struct file *filep, const char *buf,
size_t count, loff_t *f_pos);
static void onebyte_exit(void);
static loff_t onebyte_lseek(struct file *filep, loff_t offset, int whence);
long ioctl_example(struct file *filp, unsigned int cmd, unsigned long arg);

/* definition of file_operation structure */
struct file_operations onebyte_fops = {
	read:onebyte_read,
	write:onebyte_write,
	open:onebyte_open,
	release: onebyte_release,
	llseek: onebyte_lseek,
	unlocked_ioctl: ioctl_example
};

char* onebyte_data = NULL;
char* dev_msg = NULL;
int overflowed_byte = 0;
int dev_msg_len = 0;

long ioctl_example(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0, tmp, retval = 0;
	if((_IOC_TYPE(cmd) != SCULL_IOC_MAGIC)||(_IOC_NR(cmd) > SCULL_IOC_MAXNR))return -ENOTTY;
	if(_IOC_DIR(cmd)&_IOC_READ)err = !access_ok(VERIFY_WRITE, (void __user*)arg, _IOC_SIZE(cmd));
	if(_IOC_DIR(cmd)&_IOC_WRITE)err = !access_ok(VERIFY_READ, (void __user*)arg, _IOC_SIZE(cmd));
	if(err)return -EFAULT;
	switch(cmd){
		case SCULL_HELLO:
			printk("Hello!\n");
			break;
		case SCULL_DEV_MSG:
			dev_msg = kmalloc(dev_msg_len, GFP_KERNEL);
			long dest_address = dev_msg;
			long source_address = arg;
			retval = copy_from_user(dest_address, source_address, dev_msg_len);
			//*(dest_address+len)='\0';
			printk("%s", dest_address);
			break;
		case SCULL_DEV_MSG_LEN:
			dev_msg_len = arg;
			printk("message length: %d", dev_msg_len);
			break;
		case SCULL_DEV_MSG_TO_USER:
			retval = copy_to_user(arg, dev_msg, dev_msg_len);
			break;
		case SCULL_DEV_MSG_BI:
			dev_msg = kmalloc(dev_msg_len, GFP_KERNEL);
			retval = copy_from_user(dev_msg, arg, dev_msg_len);
			retval = copy_to_user(arg, dev_msg, dev_msg_len);
			*dev_msg = 'H';
			printk("%s",dev_msg);
			break;
		default:
			return -ENOTTY;
	}
	return retval;
}

static loff_t onebyte_lseek(struct file *filep, loff_t offset, int whence)
{
	loff_t new_pos = 0;
	switch(whence)
	{
		case 0: new_pos = offset;//SEEK_SET
				break;
		case 1: new_pos = filep->f_pos + offset;//SEEK_CURRENT
				break;
		case 2: new_pos = size - offset;//SEEK_END --> [0] [1] [2] [3] --> 3
				break;
	}
	if(new_pos > size) new_pos = size;
	if(new_pos < 0) new_pos = 0;
	filep->f_pos = new_pos;
	return new_pos;
}

int onebyte_open(struct inode *inode, struct file *filep)
{
	return 0; // always successful
}
int onebyte_release(struct inode *inode, struct file *filep)
{
	return 0; // always successful
}
ssize_t onebyte_read(struct file *filep, char *buf, size_t count, loff_t *f_pos)
{
	//if(*f_pos == 0){
		//copy_to_user(buf, onebyte_data, 1);
	printk("---------------Reading------------");
	printk("count: %d",(count));
	printk("*f_pos: %d",*f_pos);
	int len;
	if(*f_pos + count - 1 <= size - 1)len = count;
	else len = count - (*f_pos + count -1 - size + 1);
	printk("len to be copy : %d", len);
	if(*f_pos < size){
		long dest_address = buf;
		long source_address = onebyte_data + *f_pos;
		copy_to_user(dest_address, source_address, len);//Copy byte by byte//copy_to_user(to,from,n)
		*f_pos = *f_pos + len;
		int k = 0, i =0;
		while(1){
			if(*(onebyte_data + i) == NULL)k++;
			i++;
			if(i == size)break;
		}
		printk("Number of NULL: %d",(k));
		printk("Number of bytes written to buffer: %d", size - k);
		return len;
	}
	else if(*f_pos >= size){
		return 0;
	}
	return 0;
}
ssize_t onebyte_write(struct file *filep, const char *buf, size_t count, loff_t *f_pos)
{
	//Here we need to recognise that if the device must read full 4MB, this function will execute again and again
	//Say if 4MB is the kmalloc size and buffer size is 13kB, the number of times executing this functeion will be
	//307.69. 307 times of buffer len and one last time with buffer len of 0.6923 of 13 kB.
	//Here we compare the using the actual (kmalloc size - *f_pos) and actual buffer length.
	// First case: |------------------|-----------------------------------------|
	//             ^(*f_pos)          ^(buf_size)                               ^(max_kmalloc)
	// Second case:|---------------------------------------------------------|--|-------------------|
	printk("---------------Writing------------");
	printk("count: %d",(count));
	printk("*f_pos: %d",*f_pos);
	//For example if the input contains 8 bytes -> count = 8//Another example, input is 1MB, count -> 131072 (max)
	//Important thing is that *f_pos must not be more than fsize (allocated memory) or else will be writing to unauthorized region
	//Therefore, I need to check if the current *f_pos + count will exceed fsize or not. Remember *f_pos start from 0, fsize does not!
	//It will be easier to compare through positions instead of fsize
	int len = count;
	int maxfpos = size - 1;
	int new_max_written_fos_pos = *f_pos + len - 1;
	if(new_max_written_fos_pos <= maxfpos){
		//do nth
	}
	else{ //I need to adjust the len//
		len = len - (new_max_written_fos_pos - maxfpos);
	}
	if(*f_pos < size){
		long source_address = buf;
		long dest_address = onebyte_data + *f_pos;
		copy_from_user(dest_address, source_address, len);//copy_from_user(to,from,n)//len-1 to exclude "wwerwre\r" -> '\r'
		*f_pos = *f_pos + len;
		int k = 0, i =0;
		while(1){
			if(*(onebyte_data + i) == NULL)k++;
			i++;
			if(i == size)break;
		}
		printk("Number of NULL: %d",(k));
		printk("Number of bytes written to file: %d", size - k);
		return len;
	}
	else if(*f_pos == size){
		if(count == 0) return 0;
		overflowed_byte += count;
		printk("Number of bytes overflowed: %d",overflowed_byte);
		return count;
	}
}
static int onebyte_init(void)
{
	int result;
	// register the device
	result = register_chrdev(MAJOR_NUMBER, "onebyte", &onebyte_fops);
	if (result < 0) {
		return result;
	}
	// allocate one byte of memory for storage
	// kmalloc is just like malloc, the second parameter is// the type of memory to be allocated.
	// To release the memory allocated by kmalloc, use kfree.
	kfree(onebyte_data);
	onebyte_data = kmalloc(size, GFP_KERNEL);
	printk("I got %zu bytes of memory",ksize(onebyte_data));
	int i = 0;
	while(i<size){
		*(onebyte_data + i) = NULL;
		i++;
	}
	printk("Reset %d bytes of memory to NULL",i);
	//memset(onebyte_data, NULL, size);
	if (!onebyte_data) {
		onebyte_exit();
		return -ENOMEM;
	}
	// initialize the value to be X
	*onebyte_data = 'X';
	printk(KERN_ALERT "This is a onebyte device module\n");
	return 0;
}
static void onebyte_exit(void)
{
	// if the pointer is pointing to something
	if (onebyte_data) {
		// free the memory and assign the pointer to NULL
		kfree(onebyte_data);
		//memset(onebyte_data, NULL, size);
		onebyte_data = NULL;
	}
	// unregister the device
	unregister_chrdev(MAJOR_NUMBER, "onebyte");
	printk(KERN_ALERT "Onebyte device module is unloaded\n");
}
MODULE_LICENSE("GPL");
module_init(onebyte_init);
module_exit(onebyte_exit);

