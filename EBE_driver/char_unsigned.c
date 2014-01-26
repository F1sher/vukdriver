#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
	FILE * pFile;
	long lSize;
	char * buffer;
	size_t result;
	int i;
	
	pFile = fopen ( "/root/job/EBE_driver/test_file" , "r" );
	if (pFile==NULL) {fputs ("File error\n", stderr); exit (1);}
	
	fseek (pFile, 0, SEEK_END);
	lSize = ftell (pFile);
	rewind (pFile);
	
	buffer = (char*) malloc (sizeof(char)*lSize);
	if (buffer == NULL) {fputs ("Memory error\n",stderr); exit (2);}

	// copy the file into the buffer:
	result = fread (buffer, 1, lSize, pFile);
	if (result != lSize) {fputs ("Reading error\n", stderr); exit (3);}

	/* the whole file is now loaded in the memory buffer. */

	for(i=0; i<=lSize-2; i+=2)
		printf("%d %d\n", 127+buffer[i]-'0', 127+buffer[i+1]-'0');

	// terminate
	fclose (pFile);
	free (buffer);
}
