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
	pid_t childPID; 		// PID of child process that wants to release or 							request resources 
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
	
	// setting up signal handlers 
	if(setupinterrupt() == -1){
        perror("Failed to set up handler for SIGPROF");
        return 1;
    }
    if(setupitimer() == -1){
        perror("Failed to set up the ITIMER_PROF interval timer");
        return 1;
    }
	
	int opt; 
	const char opstr[] = ":hn:s:i:f:"; 
	int proc = 0; 	// [-n]: total number of processes 
	int simul = 0; 	// [s]: max number of child processes that can simultaneously run 
	int interval = 1; // [-i]: interval in ms to launch children 
	char *filename = NULL; // initialize filename 

	// ./oss [-h] [-n proc] [-s simul] [-i intervalInMsToLaunchChildren] [-f logfile] 
	while ((opt = getopt(argc, argv, opstr)) != -1) {
		switch(opt) { 
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
			case 'f': 
				filename = optarg; 
				break; 
			default: 
				help(); 
				exit(EXIT_FAILURE);
		} 
	}
	
	if (filename == NULL) { 
		printf("Did not read filename \n"); 
		help(); 
		exit(EXIT_FAILURE); 
	} 

	// Create logfile for outputting information 
	FILE *fptr = fopen(filename, "w"); 
	if (fptr == NULL) { 
		perror("Error in file creation");	
		exit(EXIT_FAILURE);
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
		exit(1); 
	}
	
	shm_ptr = (unsigned*)shmat(shm_id, NULL, 0); 
	if (shm_ptr == NULL) { 
		perror("Unable to connect to shared memory segment\n");
		exit(1); 
	} 
	memcpy(shm_ptr, shm_clock, sizeof(unsigned) * 2); 

	// Set up message queues! 
	key_t msgkey; 
	system("touch msgq.txt"); 

	// Generating key for message queue
	if ((msgkey = ftok("msgq.txt", 1)) == -1) { 
		perror("ftok"); 
		exit(1); 
	} 
	
	// Creating message queue 	
	if ((msqid = msgget(msgkey, 0666 | IPC_CREAT)) == -1) { 
		perror("msgget in parent"); 
		exit(1); 
	} 
	printf("Message queue sucessfully set up!\n"); 	
	
	
	return 0; 

} 


static void myhandler(int s){
	// variables for signal handler
	int *seconds, *nanoseconds;
	int sh_sec, sh_nano;
    printf("SIGNAL RECIEVED--- TERMINATION\n");
    for(int i = 0; i < 20; i++){
        if(processTable[i].occupied == 1){
           	kill(processTable[i].pid, SIGTERM);
       	}
   	}
 	shmdt(seconds);
   	shmdt(nanoseconds);
   	shmctl(sh_sec, IPC_RMID, NULL); 
   	shmctl(sh_nano, IPC_RMID, NULL);
   	exit(1);
}

static int setupinterrupt(void){
    struct sigaction act;
   	act.sa_handler = myhandler;
   	act.sa_flags = 0;
   	return(sigemptyset(&act.sa_mask) || sigaction(SIGINT, &act, NULL) || sigaction(SIGPROF, &act, NULL));
}

static int setupitimer(void){
    struct itimerval value;
   	value.it_interval.tv_sec = 60;
   	value.it_interval.tv_usec = 0;
   	value.it_value = value.it_interval;
   	return (setitimer(ITIMER_PROF, &value, NULL));
}


