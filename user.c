//Vasyl Onufriyev
//3-11-19
//Spring OS 
//Worker for oss, waits for x time then dies based on shared clock

#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "shared.h"

Target AddTime(int seconds, int nanoseconds, int amt) //add time function
{
	int newnano = nanoseconds + amt;

	while(newnano >= 1000000000)
	{
		newnano -= 1000000000;
		seconds++;
	}
	
	Target* targ = (Target*) calloc(1, sizeof(Target)); //create target object
	targ->seconds = seconds;
	targ->nanoseconds = newnano;
	return *targ;	
}

int main(int argc, char** argv)
{
	char* filename = argv[0]; //filename shorthand

	key_t shmkey = ftok("shmshare", 765); //get shared key

	if (shmkey == -1) //check if the input file exists
	{
			printf("\n%s: ", filename);
			fflush(stdout);
			perror("Error: Ftok failed");
			return;
	}

	int ipcid = shmget(shmkey, sizeof(Shared), 0600|IPC_CREAT); //get shared mem

	if (ipcid == -1) //check if the input file exists
	{
			printf("\n%s: ", filename);
			fflush(stdout);
			perror("Error: failed to get shared memory");
			return;
	}
	
	Shared* data = (Shared*)shmat(ipcid, (void*)0, 0); //attached to shared mem

	if (data == (void*)-1) //check if the input file exists
	{
			printf("\n%s: ", filename);
			fflush(stdout);
			perror("Error: Failed to attach to shared memory");
			return;
	}

	int sharedTimeCurrentSec = data->seconds, sharedTimeCurrentNs = data->nanoseconds; //capture current time in shared mem
	Target targtime = AddTime(sharedTimeCurrentSec, sharedTimeCurrentNs, atoi(argv[1])); //calculate death time
	
	printf("%s: Argument got: %i, SHARED(%i %i), added seconds: %i, added nanoseconds: %i\n", argv[0], atoi(argv[1]), sharedTimeCurrentSec, sharedTimeCurrentNs, targtime.seconds, targtime.nanoseconds);
	fflush(stdout);


	while((data->seconds - targtime.seconds) < 0); //spinlock 1
	while((data->nanoseconds - targtime.nanoseconds) < 0);	 //spinlock 2

	printf("%s: PID: %i EXIT AT: (%i %i) \n", filename, getpid(), data->seconds, data->nanoseconds);
	fflush(stdout);

	shmdt(data); //detatch from sharedmem
	_Exit(21);	
}
