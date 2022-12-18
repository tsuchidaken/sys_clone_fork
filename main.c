#include <stdio.h>
#include <pthread.h>
#include <sys/resource.h>
#include <sys/wait.h>
       
#include <sys/syscall.h>
#include <unistd.h>
#define _GNU_SOURCE
          
void *thread_fn(void* arg){
    // get tid
    int tid = syscall(SYS_gettid);                                                                                                   
    // Show the tid of thread
    printf("I am the thread %d in child %d\n", tid, getpid());
    return NULL;
}        
        
int main(){
         
    pthread_t pt;
    pid_t pid;
    int waitstatus;
    // Use fork api to call clone function      
    if( (pid = fork()) != 0 ) {
        // Wait for the child process to finish
        waitpid(pid, &waitstatus, 0);
    } else {
        // print pid
        printf("I am the child %d\n", getpid());
        // Create a thread
        pthread_create(&pt, NULL, &thread_fn, NULL);
        pthread_join(pt, NULL);
    }
          
 }        