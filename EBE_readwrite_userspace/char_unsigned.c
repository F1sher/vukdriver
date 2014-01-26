#include <stdio.h>
#include <stdlib.h>

#define BUFF_SIZE	10

int write_testfile()
{
	int i;
	FILE *pFile;
	unsigned char *buffer;
	
	pFile = fopen ( "/home/das/job/EBE/testfile" , "w" );
	if (pFile==NULL) {fputs ("File error\n", stderr); return (1);}
	
	buffer = (unsigned char *) malloc (sizeof(unsigned char)*BUFF_SIZE);
	if (buffer == NULL) {fputs ("Memory error\n", stderr); return (2);}
	
	for(i=0; i<BUFF_SIZE; i++)
		buffer[i] = 255;
	
	fwrite ((char *)buffer, sizeof(char), BUFF_SIZE*sizeof(char), pFile);
	
	// terminate
	fclose (pFile);
	free (buffer);
}

int read_testfile()
{
	FILE *pFile;
	long lSize;
	char *buffer = malloc(32*sizeof(char));
	size_t result;
	size_t len = 0;
	int i=0, j;
	int x[8];
	
	pFile = fopen ( "../../EbE/test_file", "r" );
	if (pFile==NULL) {fputs ("File error\n", stderr); return (1);}
	
	fseek (pFile, 0, SEEK_END);
	lSize = ftell (pFile);
	rewind (pFile);
	
	while(getline(&buffer, &len, pFile) != -1) {
		//sscanf(buffer, "%d %d %d %d %d %d %d %d", &x[0], &x[1], &x[2], &x[3], &x[4], &x[5], &x[6], &x[7]);
		//for(i=0; i<8; i++)
	//		printf("%d ", x[i]);
		printf("%s", buffer);
	}

	/*for(i=0; i<=lSize/(512*32)-1; i++) 
		printf("%s%s", buffer, buffer+512*32*i);
	printf("lSize = %ld, buff_size = %ld", lSize, sizeof(buffer));*/
		
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
