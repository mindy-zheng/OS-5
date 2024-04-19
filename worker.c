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

#define SH_KEY 89918991
#define PERMS 0644
unsigned int shm_clock[2] = {0, 0};

typedef struct msgbuffer { 
	long mtype;                                                                        int resourceAction;     // 0 means request, 1 means release
    int resourceID;         // R0, R1, R2, etc..
    pid_t childPID;         // PID of child process that wants to release or                            request resources
} msgbuffer;
                                                                                   msgbuffer buf;
int msqid;

int main(int argc, char **argv) { 








	return 0; 
} 



