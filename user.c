#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "shared.h"

Target AddTime(int seconds, int nanoseconds, int amt)
{
	int newnano = nanoseconds + amt;

	while(newnano >= 1000000000)
	{
		newnano -= 1000000000;
		seconds++;
	}
	
	Target* targ = (Target*) calloc(1, sizeof(Target));
	targ->seconds = seconds;
	targ->nanoseconds = newnano;
	return *targ;	
}

int main(int argc, char** argv)
{
	char* filename = argv[0];

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

	Target targtime = AddTime(data->seconds, data->nanoseconds, atoi(argv[1]));
	
	printf("%s: Argument got: %i, SHARED(%i %i), added seconds: %i, added nanoseconds: %i\n", argv[0], atoi(argv[1]), data->seconds, data->nanoseconds, targtime.seconds, targtime.nanoseconds);
	fflush(stdout);

	while(data->seconds < targtime.seconds && data->nanoseconds < targtime.nanoseconds);
	
	FILE* output = fopen(argv[2], "a");
	fprintf(output, "%s: CHILD PID: %i, PPID: %i: RIP. Time of death: %i sec %i nano. Was fun while it lasted.\n", filename, getpid(), getppid(), data->seconds, data->nanoseconds);
	fflush(output);
	fclose(output);
	
	shmdt(data);
	printf("%s: PID: %i EXIT\n", filename, getpid());
	fflush(stdout);

	exit(21);	
	return 0;
}
