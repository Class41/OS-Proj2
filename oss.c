//Vasyl Onufriyev
//3-1-19
//OS Spring 2019
//Launches n many children in s bursts

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

int* cPids; //list of pids
int numpids; //count of pids
int ipcid; //inter proccess shared memory
Shared* data; //shared data struct
char* filen; //filename
char* outfilename; //output filename


struct Row { //row from the file/input
	int seconds;
	int nanoseconds;
	int arg;
	int flag;
};

struct Row rows[500]; //accept up to x rows
int rowcount = -1; //max rows stored
int timerinc; //inc timer

int parsefile(FILE* in) //reads in input file and parses input
{
	fscanf(in, "%i", &timerinc); //get iteration count
	fgetc(in); //eat newline

	char line[1000];

	printf("%s: PARENT: BEGIN FILE PARSE\n", filen);
	while (!feof(in)) //keep reading until end of line
	{
		rowcount++;
		if (rowcount > 500)
		{
			printf("%s: PARENT: TOO MANY LINES IN FILE. MAX 500", filen);
			return 1;
		}

		char* success = fgets(line, 1000, in);

		char* value = strtok(line, " "); // split numbers by spaces

		int fieldcount = -1;
		while (value != NULL) //check if token exists
		{
			fieldcount++;
			if (fieldcount <= 2) //make sure the amount provided matches actual
			{
				switch (fieldcount)
				{
				case 0:
					rows[rowcount].seconds = atoi(value);
					break;
				case 1:
					rows[rowcount].nanoseconds = atoi(value);
					break;
				case 2:
					rows[rowcount].arg = atoi(value);
					break;
				}
			}
			else
			{
				printf("\n%s: Error: Expected 3 values on line %i, got more.\n", filen, rowcount + 1);
				exit(1); //exit with error
			}

			value = strtok(NULL, " "); //get next token
		}

		if(fieldcount < 2 && success != NULL) //when < 3 elements on a line
		{
			printf("\n%s: Error: Expected 3 values on line %i got less.\n", filen, rowcount + 1);
			exit(1);
		}

	}

	int i;
	for (i = 0; i < rowcount; i++) //output parse data
	{
		printf("%s: PARENT: PARSED: sec: %i, nano %i, offset %i\n", filen, rows[i].seconds, rows[i].nanoseconds, rows[i].arg);
		fflush(stdout);
	}
}

int userready(int* tracker) //loops through users and determines what users are ready to be launched
{
	int x;
	for (x = 0; x < rowcount; x++) //seeks ready users by comparing clock to launch start time. Ignores launched rows.
	{
		if (rows[x].seconds <= data->seconds && rows[x].nanoseconds <= data->nanoseconds && rows[x].flag != 1337)
		{
			*tracker = x; //returns position of ready struct
			return 1;
		}
	}
	return 0;
}


void timerhandler(int sig) //2 second kill timer
{
	handler(sig);
}

int setupinterrupt() //setup interrupt handling
{
	struct sigaction act;
	act.sa_handler = timerhandler;
	act.sa_flags = 0;
	return (sigemptyset(&act.sa_mask) || sigaction(SIGPROF, &act, NULL));
}

int setuptimer() //setup timer handling
{
	struct itimerval value;
	value.it_interval.tv_sec = 2;
	value.it_interval.tv_usec = 0;
	value.it_value = value.it_interval;
	return (setitimer(ITIMER_PROF, &value, NULL));
}

void handler(int signal) //handle ctrl-c and timer hit
{
	printf("%s: Kill Signal Caught. Killing children and terminating...", filen);
	fflush(stdout);

	int i;
	for (i = 0; i < numpids; i++)
	{
		if (cPids[i] > 0) //kill all pids just in case
		{
			kill(cPids[i], SIGTERM);
		}
	}

	FILE* out = fopen(outfilename, "a"); //write clock to file
	fprintf(out, "%s: PARENT: CLOCK: Seconds: %i ns: %i\n", filen, data->seconds, data->nanoseconds);
	fclose(out);


	free(cPids); //free used memory
	shmctl(ipcid, IPC_RMID, NULL); //free shared mem


	kill(getpid(), SIGTERM); //kill self
}

void AddTime(int* seconds, int* nano, int amount)
{
	int newnano = *nano + amount; 

	while (newnano >= 1000000000) //nano = 10^9, so keep dividing until we get to something less and increment seconds
	{
		newnano -= 1000000000;
		(*seconds)++;
	}
	*nano = newnano;
}

void DoFork(int value, char* output) //do fun fork stuff here. I know, very useful comment.
{
	char* convert[15];
	sprintf(convert, "%i", value); //convert int to char in the most inneficient way possible 
	char* forkarg[] = {
			"./user",
			convert,
			output,
			NULL
	}; //null terminated parameter array of chars

	execv(forkarg[0], forkarg); //exec
	printf("Exec failed! Aborting."); //all is lost. we couldn't fork. Blast.
	handler(1);
}

int checkPIDs(int* pids, int count) //deprecated. used to check if any pids were still alive. Think of this as a piece of history.
{
	int i;
	for (i = 0; i < count; i++)
	{
		if (pids[i] > 0)
			return 1;
	}
	return 0;
}

void DoSharedWork(char* filename, int childMax, int childConcurMax, FILE* input, char* output) //This is where the magic happens. Forking, and execs be here
{
	outfilename = output; //global outpt filename
	numpids = childMax;  //global pid count
	key_t shmkey = ftok("shmshare", 765); //shared mem key

	if (shmkey == -1) //check if the input file exists
	{
		printf("\n%s: ", filename);
		fflush(stdout);
		perror("Error: Ftok failed");
		return;
	}

	ipcid = shmget(shmkey, sizeof(Shared), 0600 | IPC_CREAT); //get shared mem

	if (ipcid == -1) //check if the input file exists
	{
		printf("\n%s: ", filename);
		fflush(stdout);
		perror("Error: failed to get shared memory");
		return;
	}

	data = (Shared*)shmat(ipcid, (void*)0, 0); //attach to shared mem

	if (data == (void*)-1) //check if the input file exists
	{
		printf("\n%s: ", filename);
		fflush(stdout);
		perror("Error: Failed to attach to shared memory");
		return;
	}

	data->seconds = 0; //give the computer amnesia. Gotta reset shared mem.
	data->nanoseconds = 0; //make sure it has amnesia for sure. Gotta reset shared mem.

	cPids = calloc(childMax, sizeof(int)); //dynamically allocate a array of pids

	int status; //keeps track of status of waited pids
	signal(SIGQUIT, handler); //Pull keycombos
	signal(SIGINT, handler); //pull keycombos
	int i; //generic iterator. I call him bob.
	int remainingExecs = childMax; 
	int activeExecs = 0; //how many execs are going right now
	int exitcount = 0; //how many exits we got
	int cPidsPos = 0; //wher we are in the cpid array
	FILE* o = fopen(output, "a"); //open the output file

	while (1) {
		//printf("Parent Timer: %i %i\n", data->seconds, data->nanoseconds);
		AddTime(&(data->seconds), &(data->nanoseconds), timerinc);

		pid_t pid; //pid temp
		int usertracker = -1; //updated by userready to the position of ready struct to be launched
		if (activeExecs < childConcurMax && remainingExecs > 0 && userready(&usertracker) > 0)
		{
			pid = fork(); //the mircle of proccess creation

			if (pid < 0) //...or maybe not proccess creation if this executes
			{
				perror("Failed to fork, exiting");
				handler(1);
			}

			remainingExecs--; //we have less execs now since we launched successfully
			if (pid == 0)
			{
				DoFork(rows[usertracker].arg, output); //do the fork thing with exec followup
			}
			
			fprintf(o, "%s: PARENT: STARTING CHILD %i AT TIME SEC: %i NANO: %i\n", filen, pid, data->seconds, data->nanoseconds); //we are parent. We have made child at this time
			rows[usertracker].flag = 1337; //set usertracker to already executed so it is ignored
			cPids[cPidsPos] = pid; //add pid to pidlist
			cPidsPos++; //increment pid list
			activeExecs++; //increment active execs
		}

		if ((pid = waitpid((pid_t)-1, &status, WNOHANG)) > 0) //if a PID is returned
		{
			if (WIFEXITED(status))
			{
				//printf("\n%s: PARENT: EXIT: PID: %i, CODE: %i, SEC: %i, NANO %i", filen, pid, WEXITSTATUS(status), data->seconds, data->nanoseconds);
				if (WEXITSTATUS(status) == 21) //21 is my custom return val
				{
					exitcount++;
					activeExecs--;
					fprintf(o, "%s: CHILD PID: %i: RIP. fun while it lasted: %i sec %i nano.\n", filen, pid, data->seconds, data->nanoseconds);
				}
			}
		}

		if (exitcount == childMax && remainingExecs == 0) //only get out of loop if we run out of execs or we have maxed out child count
			break;
	}

	printf("((REMAINING: %i)))\n", remainingExecs);

	fprintf(o, "%s: PARENT: CLOCK: Seconds: %i ns: %i\n", filename, data->seconds, data->nanoseconds); //final clock reading and write
	fflush(o);
	fclose(o);

	free(cPids); //free up memory
	shmdt(data); //detatch from shared mem
	shmctl(ipcid, IPC_RMID, NULL); //clear shared mem
	exit(0);
}

int main(int argc, char** argv)
{
	if (setupinterrupt() == -1) //handler for SIGPROF failed
	{
		perror("Failed to setup handler for SIGPROF");
		return 1;
	}
	if (setuptimer() == -1) //timer failed
	{
		perror("Failed to setup ITIMER_PROF interval timer");
		return 1;
	}

	filen = argv[0]; //shorthand for filename
	int optionItem; 
	int childMax = 4; //default
	int childConcurMax = 2; //default

	char* inputName = malloc(50 * sizeof(char) + 1); //store filenames
	char* outputName = malloc(50 * sizeof(char) + 1); //create space for output default

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

	parsefile(input); //read file contents

	if(childMax > rowcount) //if we asked for more children than the file can supply
	{
		printf("%s: Not enough lines in file for max count: %i\n", filen, childMax);
		exit(-1);	
	}		
		
	DoSharedWork(argv[0], childMax, childConcurMax, input, outputName); //do fork/exec fun stuff

	return 0;
}
