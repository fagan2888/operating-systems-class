#include <linux/init.h>
#include <linux/module.h>

#include <linux/fs.h>
#include <asm/uaccess.h>
#define BUFFER_SIZE 1024

static char device_buffer[BUFFER_SIZE];

ssize_t simple_char_driver_read (struct file *pfile, char __user *buffer, size_t length, loff_t *offset)
{
	/* *buffer is the userspace buffer to where you are writing the data you want to be read from the device file*/
	/*  length is the length of the userspace buffer*/
	/*  current position of the opened file*/
	/* copy_to_user function. source is device_buffer (the buffer defined at the start of the code) and destination is the userspace 		buffer *buffer */

	
	// don't read past the bounds of buffer	
	int bytesLeft = BUFFER_SIZE - *offset;
	int bytesToRead;
	if (bytesLeft > length)
		bytesToRead = length;
	else
		bytesToRead = bytesLeft;

	if (bytesToRead == 0)
		return 0;
	
	// copy_to_user returns number of unsuccessfully read bytes
	int bytesRead = bytesToRead - copy_to_user(buffer, device_buffer + *offset, bytesToRead);
	printk(KERN_INFO "String '%.*s' read from buffer at offset %d. Read %d bytes.\n", bytesRead, device_buffer + *offset, *offset, bytesRead);

	if (*offset + bytesRead == BUFFER_SIZE)
		printk(KERN_INFO "Note: End of buffer reached.");

	*offset += bytesRead;
	return bytesRead;
}



ssize_t simple_char_driver_write (struct file *pfile, const char __user *buffer, size_t length, loff_t *offset)
{
	/* *buffer is the userspace buffer where you are writing the data you want to be written in the device file*/
	/*  length is the length of the userspace buffer*/
	/*  current position of the opened file*/
	/* copy_from_user function. destination is device_buffer (the buffer defined at the start of the code) and source is the userspace 		buffer *buffer */

	// don't overfill buffer
	int bytesLeft = BUFFER_SIZE - *offset;
	int bytesToWrite;
	if (bytesLeft > length)
		bytesToWrite = length;
	else
		bytesToWrite = bytesLeft;
	
	if (bytesToWrite == 0)
		return 0;


	// copy_from_user returns number of unsuccessfully written bytes
	int bytesWritten = bytesToWrite - copy_from_user(device_buffer + *offset, buffer, bytesToWrite);
	printk(KERN_INFO "String '%.*s' written into buffer at offset %d. Wrote %d bytes.\n", bytesWritten, device_buffer + *offset, *offset, bytesWritten);

	if (*offset + bytesWritten == BUFFER_SIZE)
		printk(KERN_INFO "Note: End of buffer reached.");
	
	// reset all bytes after this to 0 so echo "" > simple_file can reset buffer
	int i;
	for (i = *offset + bytesWritten; i < BUFFER_SIZE; i++)
		device_buffer[i] = 0;

	*offset += bytesWritten;
	return bytesWritten;
}


int simple_char_driver_open (struct inode *pinode, struct file *pfile)
{
	/* print to the log file that the device is opened and also print the number of times this device has been opened until now*/
	static unsigned int num_opens = 0;
	num_opens++;

	printk(KERN_INFO "The file has been opened. It has now been opened %d times.", num_opens);
	return 0;
}


int simple_char_driver_close (struct inode *pinode, struct file *pfile)
{
	/* print to the log file that the device is closed and also print the number of times this device has been closed until now*/
	static unsigned int num_closes = 0;
	num_closes++;

	printk(KERN_INFO "The file has been closed. It has now been closed %d times.", num_closes);
	return 0;
}

struct file_operations simple_char_driver_file_operations = {
	.owner   = THIS_MODULE,
	.open = &simple_char_driver_open,
	.release = &simple_char_driver_close,
	.read = &simple_char_driver_read,
	.write = &simple_char_driver_write
};

static int simple_char_driver_init(void)
{
	/* print to the log file that the init function is called.*/
	printk(KERN_INFO "The init function has been called.");

	/* register the device */
	register_chrdev(240, "simple character driver", &simple_char_driver_file_operations); 
	
	return 0;
}

static int simple_char_driver_exit(void)
{
	/* print to the log file that the exit function is called.*/
	printk(KERN_INFO "The exit function has been called.");
	
	/* unregister  the device using the register_chrdev() function. */
	unregister_chrdev(240, "simple character driver");

	return 0;
}

/* add module_init and module_exit to point to the corresponding init and exit function*/
module_init(simple_char_driver_init);
module_exit(simple_char_driver_exit);
