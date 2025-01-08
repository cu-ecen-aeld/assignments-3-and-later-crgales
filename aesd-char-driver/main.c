/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations

#include "aesd-circular-buffer.h"
#include "aesdchar.h"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Chuck Gales"); /** DONE: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    struct aesd_dev *dev;

    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    // No specific release functionality needed for now
    // This function is called when the file is closed
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{

    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */
    struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry *entry;
    ssize_t entry_offset = 0;
    ssize_t bytes_copied = 0;
    ssize_t buffers_read = 0;

    PDEBUG("1read %zu bytes with offset %lld",count,*f_pos);

    if (mutex_lock_interruptible(&dev->lock)) {
        PDEBUG("Can't get mutex");
        return -ERESTARTSYS;
    }

    PDEBUG("2read %zu bytes with offset %lld",count,*f_pos);

    if (count == 0) {
        mutex_unlock(&dev->lock);
        return 0;
    }

    PDEBUG("3read %zu bytes with offset %lld",count,*f_pos);

    // Find the buffer entry corresponding to the file position
    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->buffer, *f_pos, &entry_offset);

    // If the entry is NULL, it means that the file position is not available in the buffer
    if (entry) {
        while ((bytes_copied < count) && (buffers_read++ < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) && entry && entry->size) {
            PDEBUG("So far read %zu bytes and %zu buffers",bytes_copied, buffers_read);
            if (copy_to_user(buf + bytes_copied, entry->buffptr + entry_offset, entry->size - entry_offset)) {
                mutex_unlock(&dev->lock);
                return -EFAULT;
            }
            bytes_copied += (entry->size - entry_offset);
            entry_offset = 0;

            entry = aesd_circular_buffer_get_next_entry(&dev->buffer, entry);
        }
    } else {
        bytes_copied = 0;
    }

    PDEBUG("read finished with %zu bytes", bytes_copied);

    mutex_unlock(&dev->lock);
    return bytes_copied;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */
    struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry *entry;
    ssize_t bytes_not_copied;
    char *tmp_data;

    if (mutex_lock_interruptible(&dev->lock)) {
        return -ERESTARTSYS;
    }

    if (count == 0) {
        mutex_unlock(&dev->lock);
        return 0;
    }

    // If there is a new buffer entry, it means that the previous write didn't end in a new line
    // and the buffer entry was not added to the circular buffer, so we need to append the new data
    if (dev->new_entry) {
        PDEBUG("Appending to previous entry");
        // Allocate memory for the new buffer entry which will contain the previous data and the new data
        tmp_data = kmalloc(dev->new_entry->size + count, GFP_KERNEL);
        if (!tmp_data) {
            mutex_unlock(&dev->lock);
            return -ENOMEM;
        }
        // Copy the previous data to the new buffer entry
        memcpy(tmp_data, dev->new_entry->buffptr, dev->new_entry->size);

        // And free the memory allocated for the previous buffer entry
        kfree(dev->new_entry->buffptr);

        // Copy the new data to the end of the new data buffer
        bytes_not_copied = copy_from_user(tmp_data + dev->new_entry->size, buf, count);

        // If some bytes were not copied, free the buffer and return an error 
        if (bytes_not_copied) {
            kfree(tmp_data);
            mutex_unlock(&dev->lock);
            return -EFAULT;
        }

        dev->new_entry->size += count;
        dev->new_entry->buffptr = tmp_data;
    } else {
        PDEBUG("Creating new entry");

        // Allocate memory for the new buffer entry
        dev->new_entry = kmalloc(sizeof(struct aesd_buffer_entry), GFP_KERNEL);
        if (!dev->new_entry) {
            mutex_unlock(&dev->lock);
            return -ENOMEM;
        }

        PDEBUG("Creating new entry buffer with size %zu", count);
        dev->new_entry->buffptr = kmalloc(count, GFP_KERNEL); // Allocate memory for the buffer
        if (!dev->new_entry->buffptr) {
            kfree(dev->new_entry);
            dev->new_entry = NULL;
            mutex_unlock(&dev->lock);
            return -ENOMEM;
        }

        PDEBUG("Copying data to new entry from user space");
        bytes_not_copied = copy_from_user(dev->new_entry->buffptr, buf, count);
        if (bytes_not_copied) {
            kfree(dev->new_entry->buffptr);
            kfree(dev->new_entry);
            dev->new_entry = NULL;
            mutex_unlock(&dev->lock);
            return -EFAULT;
        }
        dev->new_entry->size = count;
    }

    PDEBUG("Checking if the last character is a new line");

    // We now have a new buffer entry, so we need to add it to the circular buffer if the last character is a new line
    if (dev->new_entry->buffptr[dev->new_entry->size - 1] == '\n') {
        PDEBUG("Moving new entry to circular buffer");
        aesd_circular_buffer_add_entry(&dev->buffer, dev->new_entry);
        dev->new_entry = NULL;
    }

    mutex_unlock(&dev->lock);
    retval = count;

    return retval;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}

int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;

    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");

    aesd_major = MAJOR(dev);

    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }

    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    mutex_init(&aesd_device.lock);
    aesd_circular_buffer_init(&aesd_device.buffer);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }

    return result;

}

void aesd_cleanup_module(void)
{
    uint8_t index;
    struct aesd_buffer_entry *entry;
    
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * DONE: cleanup AESD specific poritions here as necessary
     */

    // Free memory allocated for circular buffer entries
    AESD_CIRCULAR_BUFFER_FOREACH(entry,&aesd_device.buffer,index) {
        kfree(entry->buffptr);
    }

    mutex_destroy(&aesd_device.lock);

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
