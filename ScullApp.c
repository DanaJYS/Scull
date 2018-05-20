#include <stdio.h>
#include <stdlib.h>
#include <linux/fcntl.h>

#define BUF_SIZE		256

int GetBufSize(const char *buf)
{
	if(buf == NULL)
		return 0;

	int count = 0;
	while(*buf++ != '\0')
		count++;

	return count;
}

char TransCharToLow(char ch)
{
	if(ch >= 'A' && ch <= 'Z')
		return ch + ('a' - 'A');
	else if(ch >= 'a' && ch <= 'z')
		return ch;
	else
		return '\0';
}

int main()
{
	int fd;
	char buf[BUF_SIZE];
	char buf_read[BUF_SIZE];
	char devName[30];

	int pos = 0;
	int bSize = 0;
	char ch;
	char th;

	int chose = 1;

	printf("Please enter the device name: ");
	scanf("%[^\n]", devName);
	getchar();

	if((fd=open(devName, O_RDWR)) == -1)
	{
		printf("open scull wrong!\n");
		return 0;
	}
	else
		printf("open scull success!\n");

	printf("Please enter a chose: ");
	while(chose)
	{
		scanf("%c", &ch);
		//printf("%c", ch);
		getchar();
		ch = TransCharToLow(ch);
		if(ch < 'a' || ch > 'z')
		{
			printf("You enter a wrong chose\n");
			printf("Please renter: ");
			continue;
		}
		
		switch(ch)
		{
			case 'w':
				printf("Please enter a string:\n");
				scanf("%[^\n]", buf);
				getchar();
				bSize = GetBufSize(buf);
				
				printf("Please enter a position: ");
				scanf("%d", &pos);
				getchar();
				llseek(fd, pos, SEEK_SET);
				
				printf("You will write string %s to Scull, string length is %d\n", buf, bSize);
				if (write(fd, buf, bSize+1, &pos) > 0)
					printf("Write success\nPlease enter a chose: ");
				else
					printf("Write failed\nPlease renter: ");
				break;

			case 'r':
				printf("Please enter a position: ");
				scanf("%d", &pos);
				getchar();
				llseek(fd, pos, SEEK_SET);
				
				if(read(fd, buf_read, BUF_SIZE, &pos) > 0)
				{
					printf("The string read frome Scull is: %s\n", buf_read);
					printf("Please enter a chose: ");
				}
				else
					printf("Read failed\nPlease renter: ");
				break;

			case 'q':
				chose = 0;
				printf("You will quit the programe\n");
				break;

			default:
				printf("You enter a wrong chose\n");
				printf("Please renter: ");
				break;
				
		}
	}

	return 0;
}
