// Name: Mindy Zheng
// Date: 4/15/2024 
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h> 
#include <math.h>
#include <signal.h> 
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <sys/wait.h>
#include <time.h> 
#include <stdbool.h> 
#include <sys/msg.h>
#include <errno.h>
#include <stdarg.h> 

void help(); 

// Key for shared memory
#define SH_KEY 89918991
#define PERMS 0644
unsigned int shm_clock[2] = {0, 0}; // Creating simulated clock in shared memory

// Defined variables 
#define HALF_NANO 5000000000	// Condition for printing the process table; half a nano
#define ONE_SEC 1000000000	// 1,000,000,000 (billion) nanoseconds is equal to 1 second
#define NANO_INCR 100000	// 0.1 millisecond 
int total_launched = 0, total_terminated = 0, second_passed = 0, half_passed = 0;
unsigned long long launch_passed = 0;  

// PCB Structure 
struct PCB { 
	int occupied; 		// Either true or false	
	pid_t pid; 			// Process ID of current assigned child 
	int startSeconds; 	// Start seconds when it was forked
	int startNano; 		// Start nano when it was forked
	int awaitingResponse; 	// Process waiting response 
}; 
struct PCB processTable[18]; 

// Message Queue Structure 
typedef struct msgbuffer { 
	long mtype; 
	int resourceAction; 	// 0 means request, 1 means release
	int resourceID; 		// R0, R1, R2, etc.. 
	pid_t targetPID; 		// PID of child process that wants to release or 							request resources 
} msgbuffer;

msgbuffer buf; 
int msqid; 
unsigned shm_id;
unsigned *shm_ptr; 


// Resource and allocation tables 
int allocatedMatrix[10][18]; 
int requestMatrix[10][18]; 
int allResources[10]; 

static void myhandler(int);
static int setupinterrupt(void);
static int setupitimer(void);

// Limit logfile from reaching more than 10k lines 
int lfprintf(FILE *stream,const char *format, ... ) {
    static int lineCount = 0;
    lineCount++;

    if (lineCount > 10000)
        return 1;

    va_list args;
    va_start(args, format);
    vfprintf(stream,format, args);
    va_end(args);

    return 0;
}

void incrementClock() {
	int incrementNano = NANO_INCR;
	shm_clock[1] += incrementNano;  

	if (shm_clock[1] >= ONE_SEC) { 
		unsigned incrementSec = shm_clock[1] / ONE_SEC; 
		shm_clock[0] += incrementSec; 
		shm_clock[1] %= ONE_SEC;
	}
	
	memcpy(shm_ptr, shm_clock, sizeof(unsigned int) * 2); 
}


int main(int argc, char **argv) { 
	srand(time(NULL) + getpid()); 
	signal(SIGINT, terminate); 
	signal(SIGALARM, terminate); 
	alarm(5); 
	
	int opt; 
	const char opstr[] = "f:hn:s:i:"; 
	int proc = 0; 	// [-n]: total number of processes 
	int simul = 0; 	// [s]: max number of child processes that can simultaneously run 
	int interval = 1; // [-i]: interval in ms to launch children 
	char *filename = NULL; // initialize filename 

	// ./oss [-h] [-n proc] [-s simul] [-i intervalInMsToLaunchChildren] [-f logfile] 
	while ((opt = getopt(argc, argv, opstr)) != -1) {
		switch(opt) {
			case 'f': { 
				char *opened_file = optarg; 
				FILE fptr = fopen(opened_file, "r"); 
				if (file) { 
					filename = opened_file; 
					fclose(file); 
				} else { 
					printf("File doesn't exist - ERROR\n"); 
					exit(1); 
				} 
				break;
			}  
			case 'h': 
				help(); 
				break; 
			case 'n': 
				proc = atoi(optarg); 
				if (proc > 18) { 
					printf("The max number of processes is 18\n"); 
					exit(1); 
				} 
				break; 
			case 's': 
				simul = atoi(optarg); 
				if (simul > 18) {
                    printf("The max number of simultaneous processes is 18\n");
                    exit(1);
                }
			case 'i': 
				interval = atoi(optarg); 
				break; 
			default: 
				help(); 
				exit(EXIT_FAILURE);
		} 
	}
	
	// Initializing PCB table: 
	for (int i = 0; i < 18; i++) { 
		processTable[i].occupied = 0;
		processTable[i].pid = 0; 
		processTable[i].startSeconds = 0; 
		processTable[i].startNano = 0; 
		processTable[i].awaitingResponse = 0; 
	} 

	// Set up Resource Matrix
	for (int i = 0; i < 10; i++) { 
		for (int j = 0; j < 18; j++) { 
			allocatedMatrix[i][j] = 0; 
			requestMatrix[i][j] = 0; 
		}
	} 
	
	for (int i = 0; i < 10; i++) { 
		allResources[i] = 0; 
	} 

	// Set up shared memory channels! 
	shm_id = shmget(SH_KEY, sizeof(unsigned) * 2, 0777 | IPC_CREAT); 
	if (shm_id == -1) { 
		fprintf(stderr, "Shared memory get failed in seconds\n");
		terminate(); 
	}
	
	shm_ptr = (unsigned*)shmat(shm_id, NULL, 0); 
	if (shm_ptr == NULL) { 
		perror("Unable to connect to shared memory segment\n");
		terminate(); 
	} 
	memcpy(shm_ptr, shm_clock, sizeof(unsigned) * 2); 

	// Set up message queues! 
	key_t msgkey; 
	system("touch msgq.txt"); 

	// Generating key for message queue
	if ((msgkey = ftok("msgq.txt", 1)) == -1) { 
		perror("ftok");
		terminate();  
	} 
	
	// Creating message queue 	
	if ((msqid = msgget(msgkey, 0666 | IPC_CREAT)) == -1) { 
		perror("msgget in parent");
		terminate();  
	} 
	printf("Message queue sucessfully set up!\n"); 	

	while (total_terminated != proc) { 
		launched_passed += NANO_INCR; 
		incrementClock(); 
	
		// Calculate if we should launch child 
		if (launch_passed >= interval || total_launched == 0) { 
			if (total_launched < proc && total_launched < simul + total_terminated)	{ 
				pid_t pid = fork(); 
				if (pid == 0) { 
					char *args[] = {"./worker", NULL}; 
					execvp(args[0], args); 
				} else {
					processTable[total_launched].pid = pid; 
					processTable[total_launched].occupied = 1;
					processTable[total_launched].awaitingResponse = 0; 
					processTable[total_launched].startSeconds = shm_clock[0]; 
					processTable[total_launched].startNano = shm_clock[1]; 
				} 
				total_launched += 1; 
			} 

			launch_passed = 0; 
		} 

		// Check if any processes have terminated 
		for (int i = 0; i < total_launched; i++) { 
			int status; 
			pid_t childPid = processTable[i].pid; 
			pid_t result = waitpid(childPid, &status, WNOHANG); 
			if (result > 0) {
				FILE* fptr = fopen(filename, "a+"0); 
				if (fptr == NULL) { 
					perror("Error opening file"); 
					exit(1); 
				} 
 
				char *detection_message = "\nMaster detected process P%d termianted\n"; 
				lfprintf(fptr, detection_message, i); 
				printf(detection_messsage, i); 

				// Release Resources if child has terminated 
				lfprintf(fptr, "Releasing Resources: "); 
				printf("Releasing Resources: "); 

				for (int t = 0; t < 10; t++) { 
					if (allocatedMatrix[t][i] != 0) { 
						allResources[t] -= allocatedMatrix[t][i]; 
						lfprintf(fptr, "R%d: %d ", t, allocatedMatrix[t][i]); 
						printf("R%d: %d ", t, allocatedMatrix[t][i]); 
					} 
					requestMatrix[t][i] = 0; 
					allocatedMatrix[t][i] = 0; 
				} 
				lprintf(fptr, "\n"); 
				printf("\n"); 
				processTable[i].occupied = 0; 
				processTable[i].awaitingResponse = 0; 
				total_terminated += 1; 
				fclose(fptr); 
			}
		} 
		
		if (proc == total_terminated) { 
			terminate(); 
		} 

		// Check message from children 
		msgbuffer msg; 
		if (msgrcv(msqid, &msg, sizeof(msgbuffer), getpid(), IPC_NOWAIT) == -1) { 
			if (errno == ENOMSG) { 
				printf("Got no message, so maybe do nothing\n"); 
			} else { 
				printf("Got an error message from msgrcv\n"); 
				perror("msgrcv"); 
				terminate(); 
			} 
		} else {
			// printf("Recived %d from worker\n",message.data);
			int targetPID = -1; 
			pid_t messenger = msg.targetPID; 
			
			for (int i = 0; i < proc; i++) { 
				if (processTable[i].pid == messenger) { 
					targetPID = i; 
				} 
			} 

			// Check the content of the msg 
			int sendmsg = 0; 
			if (msg.resourceAction == 1) { 
				FILE* fptr = fopen(filename, "a+"); 
				if (fptr == NULL) { 
					perror("Error opening and appending to file\n"); 
					terminate(); 
				} 
			
				char *acknowledged_message = "Master has acknowledged Process P%d releasing R%d at time %u:%u\n\n"; 
				lfprintf(fptr, acknowledged_message, targetPID, msg, shm_clock[0], shm_clock[1]); 
				printf(acknowledged_message, targetPID, msg, shm_clock[0], shm_clock[1]); 

				// Child is releasing instance of resource
				allResources[
		}


	return 0; 

} 



void terminate() {
    // Terminate all processes, clean msg queue and remove shared memory
    kill(0, SIGTERM);
    msgctl(msqid, IPC_RMID, NULL);
    shmdt(shm_ptr);
    shmctl(shm_id, IPC_RMID, NULL);
    exit(0);
}
