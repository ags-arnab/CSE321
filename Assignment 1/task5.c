#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

int main()
{
    int counter;
    pid_t child;
    
    child = fork();
    
    if (child == 0) {
        printf("2. Child process ID: %d\n", getpid());
        fflush(stdout);
        
        for (counter = 0; counter < 3; counter++) {
            pid_t grandchild = fork();
            if (grandchild == 0) {
                printf("%d. Grand Child process ID: %d\n", counter + 3, getpid());
                fflush(stdout);
                exit(0);
            } else {
                int status;
                wait(&status);
            }
        }
        exit(0);
    } else {
        printf("1. Parent process ID: %d\n", getpid());
        fflush(stdout);
        int status;
        wait(&status);
    }
    return 0;
}