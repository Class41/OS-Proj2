#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <errno.h>
#include "shared.h"
#include <signal.h>
#include <sys/time.h>

int* cPids;
int numpids;
int ipcid;
Shared* data;
char* filen;
char* outfilename;


struct Row {
	int seconds;
	int nanoseconds;
	int arg;
};

struct Row rows[50];
int rowcount;


int parsefile(FILE* in)
{
	int timerinc;
	fscanf(in, "%i", &timerinc);
	fgetc(in);

	char line[1000];

	int linecount = 0;
	while (!feof(in))
	{
		linecount++;
		if (linecount >= 50)
		{
			printf("%s: PARENT: TOO MANY LINES IN FILE. MAX 50", filen);
			return 1;
		}

		char* success = fgets(line, 1000, in);

		char* value = strtok(line, " "); // split numbers by spaces

		int fieldcount = 0;
		while (value != NULL) //check if token exists
		{
			if (fieldcount <= 2) //make sure the amount provided matches actual
			{
				switch (fieldcount)
				{
				case 0:
					rows[linecount].seconds = atoi(value);
					break;
				case 1:
					rows[linecount].nanoseconds = atoi(value);
					break;
				case 2:
					rows[linecount].arg = atoi(value);
					break;
				}
			}
			else
			{
				printf("\n%s: Error: Expected 3 values on line %i, got more.\n", filen, linecount + 1);
				exit(1); //exit with error
			}

			fieldcount++;
			value = strtok(NULL, " "); //get next token
		}
		
		if(!(rows[linecount].seconds && rows[linecount].nanoseconds && rows[linecount].arg) && success != NULL)
		{
			printf("\n%s: Error: Expected 3 values on line %i got less.\n", filen, linecount + 1);
			exit(1);
		}	
	
	}

}


void timerhandler(int sig)
{
	handler(sig);
}

int setupinterrupt()
{
	struct sigaction act;
	act.sa_handler = timerhandler;
	act.sa_flags = 0;
	return (sigemptyset(&act.sa_mask) || sigaction(SIGPROF, &act, NULL));
}

int setuptimer()
{
	struct itimerval value;
	value.it_interval.tv_sec = 2;
	value.it_interval.tv_usec = 0;
	value.it_value = value.it_interval;
	return (setitimer(ITIMER_PROF, &value, NULL));
}

void handler(int signal)
{
	printf("%s: Kill Signal Caught. Killing children and terminating...", filen);
	fflush(stdout);

	int i;
	for (i = 0; i < numpids; i++)
	{
		if (cPids[i] > 0)
		{
			kill(cPids[i], SIGTERM);
		}
	}

	FILE* out = fopen(outfilename, "a");
	fprintf(out, "%s: PARENT: CLOCK: Seconds: %i ns: %i\n", filen, data->seconds, data->nanoseconds);
	fclose(out);


	free(cPids);
	shmctl(ipcid, IPC_RMID, NULL);


	kill(getpid(), SIGTERM);
}

void AddTime(int* seconds, int* nano, int amount)
{
	int newnano = *nano + amount;

	while (newnano >= 1000000000)
	{
		newnano -= 1000000000;
		(*seconds)++;
	}
	*nano = newnano;
}

void DoFork(int value, char* output)
{
	char* convert[15];
	sprintf(convert, "%i", value);
	char* forkarg[] = {
			"./user",
			convert,
			output,
			NULL
	};

	execv(forkarg[0], forkarg);
	printf("Exec failed! Aborting.");
	exit(0);
}

int checkPIDs(int* pids, int count)
{
	int i;
	for (i = 0; i < count; i++)
	{
		if (pids[i] > 0)
			return 1;
	}
	return 0;
}

void DoSharedWork(char* filename, int childMax, int childConcurMax, FILE* input, char* output)
{
	filen = filename;
	outfilename = output;

	key_t shmkey = ftok("shmshare", 695);

	if (shmkey == -1) //check if the input file exists
	{
		printf("\n%s: ", filename);
		fflush(stdout);
		perror("Error: Ftok failed");
		return;
	}

	ipcid = shmget(shmkey, sizeof(Shared), 0600 | IPC_CREAT);

	if (ipcid == -1) //check if the input file exists
	{
		printf("\n%s: ", filename);
		fflush(stdout);
		perror("Error: failed to get shared memory");
		return;
	}

	data = (Shared*)shmat(ipcid, (void*)0, 0);

	if (data == (void*)-1) //check if the input file exists
	{
		printf("\n%s: ", filename);
		fflush(stdout);
		perror("Error: Failed to attach to shared memory");
		return;
	}

	data->seconds = 0;
	data->nanoseconds = 0;

	cPids = calloc(childConcurMax, sizeof(int));

	int status;
	signal(SIGQUIT, handler);
	signal(SIGINT, handler);
	int i;
	int remainingExecs = childMax;
	int exitcount = 0;

	while (exitcount < childMax) {
		AddTime(&(data->seconds), &(data->nanoseconds), 20000);

		for (i = 0; i < childConcurMax; i++)
		{
			if (cPids[i] <= 0 && remainingExecs > 0)
			{
				remainingExecs--;
				cPids[i] = fork();

				if (cPids[i] < 0)
				{
					printf("\n%s: ", filename);
					fflush(stdout);
					perror("Error: Failed to fork");
					return;
				}
				else if (cPids[i] == 0) //if child
				{
					DoFork(100000, output);
				}
			}

			if (cPids[i] > 0) //TODO: parent
			{
				if (childMax - exitcount > 1)
					waitpid(cPids[i], &status, WNOHANG);
				else
					waitpid(cPids[i], &status, 0);

				if (WIFEXITED(status))
				{
					if (WEXITSTATUS(status) == 21)
					{
						cPids[i] = 0;
						exitcount++;
					}
				}
			}
		}
	}

	printf("((REMAINING: %i)))\n", remainingExecs);

	while (checkPIDs(cPids, childConcurMax))
	{
		wait(NULL);
	}

	FILE* out = fopen(output, "a");
	fprintf(out, "%s: PARENT: CLOCK: Seconds: %i ns: %i\n", filename, data->seconds, data->nanoseconds);
	fclose(out);

	shmdt(data);
	shmctl(ipcid, IPC_RMID, NULL);
	exit(0);
}

int main(int argc, char** argv)
{
	if (setupinterrupt() == -1)
	{
		perror("Failed to setup handler for SIGPROF");
		return 1;
	}
	if (setuptimer() == -1)
	{
		perror("Failed to setup ITIMER_PROF interval timer");
		return 1;
	}

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
		\t-s [count] : max concurrent children to be run. default: 2, < 20\n\
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

			if (childConcurMax > 20)
				childConcurMax = 20;
			if (childConcurMax > childMax)
				childConcurMax = childMax;

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

	printf("\n%s: Info: input file: %s, output file: %s, max total: %i, max concurrent: %i\n\n", argv[0], inputName, outputName, childMax, childConcurMax);
	fflush(stdout);

	FILE* input = fopen(inputName, "r"); //open input/output files specified
	FILE* output = fopen(outputName, "wr");

	fprintf(output, "***Begin Of File***\n\n");
	fclose(output);

	if (input == NULL) //check if the input file exists
	{
		printf("\n%s: ", argv[0]);
		fflush(stdout);
		perror("Error: Failed to open input file");
		return;
	}

	parsefile(input);
	DoSharedWork(argv[0], childMax, childConcurMax, input, outputName);

	return 0;
}
