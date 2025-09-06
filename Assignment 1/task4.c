#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main() {
    int array[] = {45, 23, 67, 12, 89, 34, 78, 56};
    int n = sizeof(array) / sizeof(array[0]);
    
    printf("Original array: ");
    for (int i = 0; i < n; i++) {
        printf("%d ", array[i]);
    }
    printf("\n\n");
    
    pid_t pid;
    printf("Creating child process...\n");
    pid = fork();
    
    if (pid < 0) {
        printf("Fork failed\n");
        return 1;
    }
    else if (pid == 0) {
        printf("Child process (PID: %d): Sorting the array in descending order...\n", getpid());
        
        for (int i = 0; i < n - 1; i++) {
            for (int j = 0; j < n - i - 1; j++) {
                if (array[j] < array[j + 1]) {
                    int temp = array[j];
                    array[j] = array[j + 1];
                    array[j + 1] = temp;
                }
            }
        }
        
        printf("Sorted array (descending): ");
        for (int i = 0; i < n; i++) {
            printf("%d ", array[i]);
        }
        printf("\n");
        printf("Child process completed sorting.\n");
        
        return 0;
    }
    else {
        printf("Parent process (PID: %d): Waiting for child process to complete...\n", getpid());
        int status;
        wait(&status);
        
        printf("Parent process: Child process completed. Now checking odd/even status...\n");
        printf("Odd/Even status for original array:\n");
        
        for (int i = 0; i < n; i++) {
            if (array[i] % 2 == 0) {
                printf("%d is even\n", array[i]);
            } else {
                printf("%d is odd\n", array[i]);
            }
        }
        printf("Parent process completed.\n");
    }
    
    return 0;
}