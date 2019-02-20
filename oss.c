#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include "shared.h"


void DoFork(int value)
{
	char* convert[15];
	sprintf(convert, "%i", value);
	char* forkarg[]={
			"./user",
			convert,
			NULL
			};

	execv(forkarg[0], forkarg);
	printf("Exec failed! Aborting.");
	exit(0);
}

void DoSharedWork(char* filename, int childMax, int childConcurMax, FILE* input, FILE* output)
{
	key_t shmkey = ftok("shmshare", 695);

	if (shmkey == -1) //check if the input file exists
	{
			printf("\n%s: ", filename);
			fflush(stdout);
			perror("Error: Ftok failed");
			return;
	}

	int ipcid = shmget(shmkey, sizeof(Shared), 0600|IPC_CREAT);

	if (ipcid == -1) //check if the input file exists
	{
			printf("\n%s: ", filename);
			fflush(stdout);
			perror("Error: failed to get shared memory");
			return;
	}
	
	Shared* data = (Shared*)shmat(ipcid, (void*)0, 0);

	if (data == (void*)-1) //check if the input file exists
	{
			printf("\n%s: ", filename);
			fflush(stdout);
			perror("Error: Failed to attach to shared memory");
			return;
	}

	data->seconds = 0;
	data->nanoseconds = 0;

	int pids[childConcurMax];
 


	int pid = fork();

	if(pid == -1)
	{
		printf("\n%s: ", filename);
		fflush(stdout);
		perror("Error: Failed to fork");
		return;
	}
	else if(pid > 0) //TODO: parent
	{
		time_t terminator = time(NULL) + 2;
		int status;

		while(time(NULL) < terminator)
		{		
			waitpid(pid, &status, WNOHANG);	
		}
	
		kill(pid, SIGTERM);
	}
	else //TODO: child
	{
	   DoFork(1000000001);
	}
	
	shmdt(data);
	exit(0);
}

int main(int argc, char** argv)
{
	int optionItem;
	int childMax = 4;
	int childConcurMax = 2;
	
	char* inputName = malloc(50 * sizeof(char) + 1); //store filenames
	char* outputName = malloc(50 * sizeof(char) + 1);

	strcpy(inputName, "input.txt"); //put initial strings into the malloc
	strcpy(outputName, "output.txt");
	while ((optionItem = getopt(argc, argv, "hi:o:n:s:")) != -1) //read option list
	{
			switch (optionItem)
			{
			case 'h': //show help menu
					printf("\t%s Help Menu\n\
		\t-h : show help dialog \n\
		\t-i [filename] : input filename. default: input.dat\n\
		\t-n [count] : max children to be created. default: 4\n\
		\t-s [count] : max concurrent children to be run. default: 2\n\
		\t-o [filename] : output filename. default: output.dat\n", argv[0]);
					return;
			case 'i': //change input
					strcpy(inputName, optarg);
					printf("\n%s: Info: set input file to: %s", argv[0], optarg);
					break;
			case 'o': //change output
					strcpy(outputName, optarg);
					printf("\n%s: Info: set output file to: %s", argv[0], optarg);
					break;
			case 's': //max child at once
					childConcurMax = atoi(optarg);
					printf("\n%s: Info: set max concurrent children to: %s", argv[0], optarg);
					break;
			case 'n': //total # of children
					childMax = atoi(optarg);
					printf("\n%s: Info: set max children to: %s", argv[0], optarg);
					break;
			case '?': //an error has occoured reading arguments
					printf("\n%s: Error: Invalid Argument or Arguments missing. Use -h to see usage.", argv[0]);
					return;
			}
	}

	printf("\n%s: Info: input file: %s, output file: %s, max total: %i, max concurrent: %i", argv[0], inputName, outputName, childMax, childConcurMax);

	FILE* input = fopen(inputName, "r"); //open input/output files specified
	FILE* output = fopen(outputName, "wr");

	if (input == NULL) //check if the input file exists
	{
			printf("\n%s: ", argv[0]);
			fflush(stdout);
			perror("Error: Failed to open input file");
			return;
	}

	DoSharedWork(argv[0], childMax, childConcurMax, input, output);
	
	return 0;
}
