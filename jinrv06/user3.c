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

/*
typedef struct {
	int **timespk;
	int **energyspk;
} dataspk;
*/

typedef struct {
	int timespk[12][MAX_CH];
	int energyspk[4][MAX_CH];
} dataspk;

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
	int i, j;
	int fd_in, fd_out[16];
	char name[7];
	char ItoA[3];
	
	fd_in = open(SIMPLE_DEVICE, O_RDWR);
	if (fd_in < 0) {
        perror("error in device");
		return -1;
	}
	
	dataspk *truedata = (dataspk *)malloc(sizeof(dataspk));
	if(truedata == NULL) {
		perror("error malloc 1");
		return-1;
	}
/*	
	truedata->timespk = (int **)malloc(12*sizeof(int *));
	if(truedata->timespk == NULL) {
		perror("error malloc 2");
		return-1;
	}
	
	truedata->energyspk = (int **)malloc(4*sizeof(int *));
	if(truedata->energyspk == NULL) {
		perror("error malloc 3");
		return-1;
	}
	
	for(i=0; i<=11; i++) {
		truedata->timespk[i] = (int *)malloc(MAX_CH*sizeof(int));
		if(truedata->timespk[i] == NULL) {
			perror("error malloc 4 ");
			return-1;
		}
	}
	for(i=0; i<4; i++) {
		truedata->energyspk[i] = (int *)malloc(MAX_CH*sizeof(int));
		if(truedata->energyspk[i] == NULL) {
			perror("error malloc 5");
			return-1;
		}
	}
	printf("1\n");
*/	
	read(fd_in, truedata, sizeof(dataspk));
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
		
		write(fd_out[i], truedata->timespk[i], MAX_CH*sizeof(int));
		close(fd_out[i]);
		
		if(i==3) for(j=0; j<=MAX_CH-1; j++) printf("%d - %d\n", j, truedata->timespk[i][j]);
	}
	
	for(i=0; i<=3; i++) {
		strcpy(name, folder2spk);
		strcat(name, "BUFKA");
		itoa(i+1, ItoA);
		strcat(name, ItoA);
		strcat(name, ".SPK"); 
		fd_out[i] = open(name, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR);
		
		if (fd_out[i] < 0) {
			perror("error in device");
			exit(1);
		}
		lseek(fd_out[i], 512, 0);
		
		write(fd_out[i], truedata->energyspk[i], MAX_CH*sizeof(int));
		close(fd_out[i]);
	}
	
	printf("All ok!!!\n");
	
	return 0;
}
