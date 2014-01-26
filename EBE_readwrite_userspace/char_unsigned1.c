#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFF_SIZE	512

int write_testfile()
{
	int i, j;
	FILE *pFile;
	unsigned char *buffer;
	static unsigned char M[4096];
	
	pFile = fopen ( "../../EbE/123" , "w" );
	if (pFile==NULL) {fputs ("File error\n", stderr); return (1);}
	
	buffer = (unsigned char *) malloc (sizeof(unsigned char)*BUFF_SIZE);
	if (buffer == NULL) {fputs ("Memory error\n", stderr); return (2);}
	
	for(i=0; i<4096; i++)
		M[i] = 255*i/4095;
		
	char *str_to_write = (char *)malloc(32*512*sizeof(char));
	char *str_temp = (char *)malloc(32*sizeof(char));
		
	sprintf(str_temp, "%u %u %u %u %u %u %u %u", M[0], M[1], M[2], M[3], M[4], M[5], M[6], M[7]);
	strcpy(str_to_write, str_temp);
	
	for(j=1; j<512; j++) {
	//	printf("%s", str_to_write);
	//	if(M[8*j+0] == M[8*j+1] && M[8*j+0] == 0) printk(KERN_INFO "Warning! all m=0 i=%d\n", j); 
		sprintf(str_temp, "%u %u %u %u %u %u %u %u", M[8*j+0], M[8*j+1], M[8*j+2], M[8*j+3], M[8*j+4], M[8*j+5], M[8*j+6], M[8*j+7]);
		strcat(str_to_write, str_temp);
	}
	
	fwrite (M, sizeof(unsigned char), 4096, pFile);
	
	// terminate
	fclose (pFile);
	free (buffer);
	free(str_to_write);
	free(str_temp);
}

int read_testfile()
{
	FILE *pFile;
	long lSize;
	unsigned char *buffer;
	size_t result;
	int i;
	
	pFile = fopen ( "/home/das/job/EBE_driver/r_wr", "r" );
	if (pFile==NULL) {fputs ("File error\n", stderr); return (1);}
	
	fseek (pFile, 0, SEEK_END);
	lSize = ftell (pFile);
	rewind (pFile);
	
	buffer = (unsigned char *) malloc (sizeof(unsigned char)*lSize);
	if (buffer == NULL) {fputs ("Memory error\n", stderr); return (2);}

	// copy the file into the buffer:
	result = fread (buffer, 1, lSize, pFile);
	if (result != lSize) {fputs ("Reading error\n", stderr); return (3);}

	/* the whole file is now loaded in the memory buffer. */

	for(i=0; i<=lSize-1; i++)
		printf("%d - %u\n", i, buffer[i]);
	printf("lSize = %ld, buff_size = %d", lSize, sizeof(buffer));
		
	//printf("%s%s%s lsize=%ld\n", buffer, buffer+32, buffer+64, lSize);
	//	if(i % 16 == 0) printf("\n");
	//}

	// terminate
	fclose (pFile);
	free (buffer);
}

int main(int argc, char **argv)
{
//	write_testfile();
	read_testfile();
	
	return 0;
}
