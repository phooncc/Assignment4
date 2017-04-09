#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>

int lcd;
#define SCULL_IOC_MAGIC 'k'
#define SCULL_HELLO _IO(SCULL_IOC_MAGIC, 1)
#define SCULL_DEV_MSG _IOW(SCULL_IOC_MAGIC, 2, int)
#define SCULL_DEV_MSG_LEN _IOW(SCULL_IOC_MAGIC, 3, int)
#define SCULL_DEV_MSG_TO_USER _IOR(SCULL_IOC_MAGIC, 4, long)
#define SCULL_DEV_MSG_BI _IOWR(SCULL_IOC_MAGIC, 5, long)

void test()
{
	int k, i, sum;
	char s[3];

	memset(s, '2', sizeof(s));
	printf("Test begin!\n");

	k = write(lcd, s, sizeof(s));
	printf("written %d\n", k);

	k = ioctl(lcd, SCULL_HELLO);
	printf("result = %d\n", k);

	char* user_msg_to_pass = "Phoon Chee Choung";
	char* user_msg = NULL;
	user_msg = malloc(strlen(user_msg_to_pass));
	k = ioctl(lcd, SCULL_DEV_MSG_LEN, strlen(user_msg_to_pass));
	k = ioctl(lcd, SCULL_DEV_MSG, user_msg_to_pass);
	k = ioctl(lcd, SCULL_DEV_MSG_TO_USER, user_msg);
	printf("The returned message: %s\n", user_msg);

	k = ioctl(lcd, SCULL_DEV_MSG_LEN, strlen(user_msg_to_pass));
	k = ioctl(lcd, SCULL_DEV_MSG_BI, user_msg);
	printf("The returned message: %s\n", user_msg); 	
}
int main(int argc, char **argv)
{
	lcd = open("/dev/4MB", O_RDWR);
	if(lcd == -1){
		perror("unable to open 4MB");
		exit(EXIT_FAILURE);
	}
	test();
	close(lcd);
	return 0;
}