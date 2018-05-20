#ifndef _SCULL_
#define _SCULL_

#include <linux/cdev.h>
#include <linux/semaphore.h>

#define SCULL_MAJOR			260
#define SCULL_NR_DEVS		2
#define SCULL_QUANTUM 		256
#define SCULL_QSET 			20

struct scull_qset {
char 				**data;
struct scull_qset		*next;
};

struct scull_dev {
struct scull_qset *data;
int				quantum;
int				qset;
unsigned long		size;
unsigned int		access_key;
struct semaphore		sem;
struct cdev		cdev;
};

#endif
