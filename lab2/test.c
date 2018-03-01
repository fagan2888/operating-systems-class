#include <stdio.h>
#include <unistd.h>

#include <sys/stat.h>
#include <fcntl.h>

// maintains two offsets, one for read and one for write
off_t readOffset = 0;
off_t writeOffset = 0;

int main(int argc, char **argv)
{
	// check if filename is supplied
	if (argc != 2) {
		printf("test program requires exactly one command-line option: the name of the device file in /dev\n");
		return 0;
	}
	char *filename = argv[1];
	
	// open file for reading and writing. will begin at beginning of file
	int fd = open(filename, O_RDWR);
	if (fd == -1) {
		printf("Failed to open file.");
		return 0;
	}
	
	printf("Opened file %s.\n", filename);
	
	// begin read/write loop
	printf("Entering read/write loop. Use 'w' to write to file, 'r' to read from file, and 'q' to quit.\n");
	
	printf("Enter desired command: ");
	char c = getchar();
	
	while (c != 'q') {
		if (c == 'w') {
			char whatToWrite[1024];
			printf("Enter string you would like to write: ");
			scanf("%s", whatToWrite);
			
			int strSize = 0;
			while (whatToWrite[strSize] != '\0')
				strSize++;

			int bytesWritten = pwrite(fd, whatToWrite, strSize, writeOffset);
			writeOffset += bytesWritten;

			printf("Write command issued.\n");
		}
		else if (c == 'r') {
			int numBytes;
			printf("Enter the maximum number of bytes you would like to read: ");
			scanf("%d", &numBytes);
			
			char readData[numBytes];
			for (int i = 0; i < numBytes; i++)
				readData[i] = 0;
			
			lseek(fd, readOffset, SEEK_SET);

			int bytesRead = pread(fd, readData, numBytes, readOffset);
			readOffset += bytesRead;

			printf("Read data string: '%.*s'.\n", numBytes, readData);
		}
		else {
			printf("Incorrect input. Please enter 'w', 'r', or 'q'.\n");
		}

		// clear input buffer (from http://stackoverflow.com/questions/7898215/how-to-clear-input-buffer-in-c)
		while ((c = getchar()) != '\n' && c != EOF) { }
		
		printf("Enter desired command: ");
		c = getchar();
	}
	
	printf("Quitting. Goodbye!\n");
	close(fd);
	
	return 0;
}
