#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#define SIMPLE_DEVICE "/dev/simple"
#define MAX_NLH_MSG	5
#define NETLINK_NITRO 17
#define MAX_CH 4096

struct truedata {
	int timespk[12][MAX_CH];
	int energyspk[4][MAX_CH];
};

static char *folder2spk = "./spk/";

void itoa(int n, char s[])
{
	if ((int) (n/10) != 0) {
		s[0] = (n/10) + '0';
		s[1] = n-10 + '0';
		s[2] = '\0';
	}
	else {
		s[0] = n + '0';
		s[1] = '\0';
	}
}

int main()
{
	int i;
	int fd_in, fd_out[16];
	char name[7];
	char ItoA[3];
	
	fd_in = open(SIMPLE_DEVICE, O_RDWR);
	if (fd_in < 0) {
        perror("error in device");
        return -1;
	}
	
	struct truedata spectras;
	read(fd_in, &spectras, sizeof(struct truedata));
	close(fd_in);
	
	for(i=0; i<=11; i++) {
		strcpy(name, folder2spk);
		strcat(name, "TIME");
		itoa(i+1, ItoA);
		strcat(name, ItoA);
		strcat(name, ".SPK");
		fd_out[i] = open(name, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR);
		
		if (fd_out[i] < 0) {
			perror("error in device");
			exit(1);
		}
		lseek(fd_out[i], 512, 0);
		
		write(fd_out[i], spectras.timespk[i], MAX_CH*sizeof(int));
		close(fd_out[i]);
	}
	
	for(i=12; i<=15; i++) {
		strcpy(name, folder2spk);
		strcat(name, "BUFKA");
		itoa(i-11, ItoA);
		strcat(name, ItoA);
		strcat(name, ".SPK");
		fd_out[i] = open(name, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR);
		
		if (fd_out[i] < 0) {
			perror("error in device");
			exit(1);
		}
		lseek(fd_out[i], 512, 0);
		
		write(fd_out[i], spectras.energyspk[i-12], MAX_CH*sizeof(int));
		close(fd_out[i]);
	}
	
	printf("All ok!!!\n");
	
	return 0;
}
