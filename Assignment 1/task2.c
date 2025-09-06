#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main()
{
    pid_t child, grandchild;
    
    child = fork();
    
    if (child == 0) {
        grandchild = fork();
        
        if (grandchild == 0) {
            printf("I am grandchild\n");
        } else {
            int status;
            wait(&status);
            printf("I am child\n");
        }
    } else {
        int status;
        wait(&status);
        printf("I am parent\n");
    }
}