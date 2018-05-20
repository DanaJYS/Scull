#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>	//printk()
#include <linux/slab.h>		//kmalloc()
#include <linux/fs.h>		//everything...
#include <linux/errno.h>	//error codes
#include <linux/types.h>	//size_t
#include <linux/fcntl.h>	//O_ACCMODE
#include <linux/cdev.h>		//
//#include <asm/system.h>		
#include <asm/uaccess.h>	//copy_*_user
#include "Scull.h"

//���豸��
int scull_major = SCULL_MAJOR;
//���豸��
int scull_minor = 0;
//���������豸�������
int scull_nr_devs = SCULL_NR_DEVS;
//���Ӵ�С
int scull_quantum = SCULL_QUANTUM;
//���Ӽ���С
int scull_qset1 = SCULL_QSET;

module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_nr_devs, int, S_IRUGO);
module_param(scull_quantum, int, S_IRUGO);
module_param(scull_qset1, int, S_IRUGO);

struct scull_dev *scull_devices;

/*
*�ͷ��������������򵥱����б����ͷ������ֵ��κ����Ӻ����Ӽ���
*��scull_open���ļ�Ϊд����ʱ���á�
*�����������ʱ��������ź�����
*/
int scull_trim(struct scull_dev *dev)
{
	struct scull_qset *next, *dptr;
	
	//���Ӽ���С
	int qset = dev->qset;
	int i = 0;

	for(dptr = dev->data; dptr; dptr = next)
	{
		if(dptr->data)
		{
			for(i = 0; i < qset; i++)
				kfree(dptr->data[i]);
			
			kfree(dptr->data);
			dptr->data = NULL;
		}

		next = dptr->next;
		kfree(dptr);
	}

	dev->size = 0;
	dev->quantum = scull_quantum;
	dev->qset = scull_qset1;
	dev->data = NULL;
	return 0;
}

/*
*open and close
*/
int scull_open(struct inode *inode, struct file *filp)
{
	struct scull_dev *dev;	//device informaton
	dev = container_of(inode->i_cdev, struct scull_dev, cdev);
	filp->private_data = dev;

	//�ļ���ֻ��ģʽ��ʱ���ض�Ϊ0
	if((filp->f_flags & O_ACCMODE) == O_WRONLY)
	{
		if(down_interruptible(&dev->sem))
			return -ERESTARTSYS;
		scull_trim(dev);
		up(&dev->sem);
	}

	return 0;
}

int scull_release(struct inode *inode, struct file * filp)
{
	return 0;
}

struct scull_qset *scull_follow(struct scull_dev *dev, int n)
{
	//��һ�����Ӽ�ָ��
	struct scull_qset *qs = dev->data;

	//�����ǰ�豸��û�����Ӽ�������ʾ�����һ�����Ӽ�
	if(!qs)
	{
		qs = dev->data = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
		if(qs == NULL)
		{
			printk(KERN_INFO "scull_follow: Malloc dev->data fail\n");
			return NULL;
		}
		memset(qs, 0, sizeof(struct scull_qset));
	}

	//������ǰ�豸�����Ӽ�����n�������Ӽ���uct���������µ�
	while(n--)
	{
		if(!qs->next)
		{
			qs->next = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
			if(!qs->next)
			{
				printk(KERN_INFO "scull_follow: Malloc qs->next fail\n");
				return NULL;
			}
			
			memset(qs->next, 0, sizeof(struct scull_qset));
		}

		qs = qs->next;
	}

	return qs;
}

ssize_t scull_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct scull_dev *dev = filp->private_data;
	struct scull_qset *dptr;
	
	//���ӣ����Ӽ���С
	int quantum = dev->quantum, qset = dev->qset;
	//һ�����Ӽ����ֽ���
	int itemsize = quantum * qset;
	int item, s_pos, q_pos, rest;
	ssize_t retval = -1;

	if(down_interruptible(&dev->sem))
		return -ERESTARTSYS;
	//Ҫ����λ�ó�������������
	if(*f_pos >= dev->size)
	{
		printk(KERN_INFO "scull_read: *f_pos >= dev->size, *f_pos = %d dev->size = %lu\n", (int)*f_pos, dev->size);
		goto out;
	}
	//Ҫ����count������size���ض�count
	if(*f_pos + count > dev->size)
		count = dev->size - *f_pos;

	//�����ӡ����Ӽ��ж�λ��дλ�ã��ڼ������Ӽ����еĵڼ������ӣ��������е�ƫ��
	//�ڼ������Ӽ�
	item = (long)*f_pos / itemsize;
	//�����Ӽ��е�ƫ��
	rest = (long)*f_pos % itemsize;
	//�ڼ������ӣ��������е�ƫ��
	s_pos = rest / quantum; q_pos = rest % quantum;
	
	//��ȡҪ��ȡ�����Ӽ���ָ��
	dptr = scull_follow(dev, item);
	//��ȡ������
	if(dptr == NULL || !dptr->data || !dptr->data[s_pos])
	{
		printk(KERN_INFO "scull_read: dptr==NULL || dptr->data==NULL || dptr->data[%d]==NULL", s_pos);
		goto out;
	}

	//ֻ��һ�������ж������count������ǰ���ӣ��ض�count
	if(count > quantum - q_pos)
		count = quantum - q_pos;
	
	//��Ҫ��ȡλ�õ����ݸ���count�ֽڵ��û��ռ�buf��
	if(copy_to_user(buf, dptr->data[s_pos] + q_pos, count))
	{
		retval = -EFAULT;
		printk(KERN_INFO "scull_read: copy_to_user function fail\n");
		goto out;
	}

	*f_pos += count;
	retval = count;

out:
	up(&dev->sem);
	return retval;
}

ssize_t scull_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	struct scull_dev *dev = filp->private_data;
	struct scull_qset *dptr;

	//���ӡ����Ӽ���С
	int quantum = dev->quantum, qset = dev->qset;
	//һ�����Ӽ����ֽ���
	int itemsize = dev->quantum * qset;
	int item, s_pos, q_pos, rest;
	ssize_t retval = -1;
	if(down_interruptible(&dev->sem))
		return -ERESTARTSYS;
	
	//�ڼ������Ӽ�
	item = (long)*f_pos / itemsize;
	//�����Ӽ��е�ƫ��
	rest = (long)*f_pos % itemsize;
	//�ڸ����Ӽ��еĵڼ������ӣ��������е�ƫ��
	s_pos = rest / quantum; q_pos = rest % quantum;
	//���ظ����Ӽ���ָ��
	dptr = scull_follow(dev, item);
	if(dptr == NULL)
	{
		printk(KERN_INFO "scull_write: Cann't get correct qset pointer\n");
		goto out;
	}
	//��������Ӽ����ݼ�����ΪNULL�� ������һ�����ڴ�
	if(!dptr->data)
	{
		dptr->data = kmalloc(qset * sizeof(char*), GFP_KERNEL);
		if(!dptr->data)
		{
			printk(KERN_INFO "scull_write: Malloc dptr->data fail\n");
			goto out;
		}
		memset(dptr->data, 0, qset * sizeof(char*));
	}

	//�����s_pos��������NULL�� ������һ�����ڴ�
	if(!dptr->data[s_pos])
	{
		dptr->data[s_pos] = kmalloc(quantum, GFP_KERNEL);
		if(!dptr->data[s_pos])
		{
			printk(KERN_INFO "scull_write: Malloc dptr->data[%d] fail\n", s_pos);
			goto out;
		}
	}

	//ֻ��һ��������д�����count������ǰ���Ӿͽض�
	if(count > quantum - q_pos)
		count = quantum - q_pos;
	//���û��ռ俽�����ݵ��ں˿ռ䣬ʧ�ܷ���û�п������ֽ������ɹ�����0
	if(copy_from_user(dptr->data[s_pos] + q_pos, buf, count))
	{
		retval = -EFAULT;
		printk(KERN_INFO "scull_write: copy_from_user function fail\n");
		goto out;
	}

	*f_pos += count;
	retval = count;
	//�����ֽ�����
	if(dev->size < *f_pos)
		dev->size = *f_pos;

out:
	up(&dev->sem);
	return retval;
}

loff_t scull_llseek(struct file *filp, loff_t off, int whence)
{
	struct scull_dev *dev = filp->private_data;
	loff_t newpos = -1;

	switch(whence)
	{
		case 0:		//SEEK_SET
		newpos = off;
		break;

		case 1:		//SEEK_CUR
		newpos = filp->f_pos + off;
		break;

		case 2:		//SEEK_END
		newpos = dev->size + off;
		break;

		default:
			printk(KERN_INFO "scull_llseek: Invalid whence\n");
			return -EINVAL;
	}

	if(newpos < 0)
	{
		printk(KERN_INFO "scull_llseek: newpos < 0, newpos = %d\n", (int)newpos);
		return -EINVAL;
	}

	filp->f_pos = newpos;
	return newpos;
}

struct file_operations scull_fops = 
{
	.owner = THIS_MODULE,
	.read = scull_read,
	.write = scull_write,
	.llseek = scull_llseek,
	.open = scull_open,
	.release = scull_release,
};

void scull_cleanup_module(void)
{
	int i;
	//�����豸�źϳ�һ��dev_t�ṹ�����豸��
	dev_t devno = MKDEV(scull_major, scull_minor);
	if(scull_devices)
	{
		//���ͷ�ÿ���豸��������
		for(i = 0; i < scull_nr_devs; i++)
		{
			//�ͷ�������
			scull_trim(scull_devices + i);
			//�Ƴ�cdev
			cdev_del(&scull_devices[i].cdev);
		}
		//�ͷ�scull_devices����
		kfree(scull_devices);
	}
	
	unregister_chrdev_region(devno, scull_nr_devs);
}

//����char_dev�ṹ
static void scull_setup_cdev(struct scull_dev *dev, int index)
{
	int err, devno = MKDEV(scull_major, scull_minor + index);

	cdev_init(&dev->cdev, &scull_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &scull_fops;
	//����ַ��豸dev->cdev,������Ч
	err = cdev_add(&dev->cdev, devno, 1);
	if(err)
		printk(KERN_NOTICE "Error %d adding scull %d", err, index);
}

int scull_init_module(void)
{
	int result, i;
	dev_t dev = 0;

	//�����豸�ţ����ڼ���ʱû��ָ�����豸�žͶ�̬����
	if(scull_major)
	{
		dev = MKDEV(scull_major, scull_minor);
		result = register_chrdev_region(dev, scull_nr_devs, "Scull");
	}
	else
	{
		result = alloc_chrdev_region(&dev, scull_minor, scull_nr_devs, "Scull");
		scull_major = MAJOR(dev);
	}

	if(result < 0)
	{
		printk(KERN_WARNING "scull: can't get major %d\n", scull_major);
		return result;
	}

	//��scull_dev���������ڴ�
	scull_devices = kmalloc(scull_nr_devs * sizeof(struct scull_dev), GFP_KERNEL);
	if(!scull_devices)
	{
		result = -ENOMEM;
		printk(KERN_INFO "malloc scull_devices fail\n");
		goto fail;
	}

	memset(scull_devices, 0, scull_nr_devs * sizeof(struct scull_dev));

	for(i = 0; i < scull_nr_devs; i++)
	{
		scull_devices[i].quantum = scull_quantum;
		scull_devices[i].qset = scull_qset1;

		printk(KERN_INFO "scull_quantum = %d, scull_qset1 = %d\n", scull_quantum, scull_qset1);
		
		//��ʼ�������������ź���sem��Ϊ1
		sema_init(&scull_devices[i].sem, 1);
		//����char_dev�ṹ
		scull_setup_cdev(&scull_devices[i], i);
	}

	return 0;
fail:
	scull_cleanup_module();
	return result;
}

module_init(scull_init_module);
module_exit(scull_cleanup_module);

MODULE_AUTHOR("JinYinShuai");
MODULE_LICENSE("GPL");
