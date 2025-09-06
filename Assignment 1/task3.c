#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

void add_process_to_file() {
    FILE *file = fopen("p_count.txt", "a");
    if (file != NULL) {
        fprintf(file, "1\n"); 
        fclose(file);
    }
}

int main() {
    pid_t original_parent_pid = getpid();
    pid_t a, b, c;
    
    FILE *file = fopen("p_count.txt", "w");
    if (file != NULL) {
        fclose(file);
    }

    a = fork();
    if (a == 0) {
        add_process_to_file(); 
        if (getpid() % 2 == 1) {
            pid_t extra_child = fork();
            if (extra_child == 0) {
                add_process_to_file();
                exit(0);
            } else if (extra_child > 0) {
                wait(NULL);
            }
        }
        exit(0); 
    } else if (a > 0) {
        add_process_to_file();
    }

    b = fork();
    if (b == 0) {
       add_process_to_file(); 
        if (getpid() % 2 == 1) {
            pid_t extra_child = fork();
            if (extra_child == 0) {
                add_process_to_file();
                exit(0);
            } else if (extra_child > 0) {
                wait(NULL);  
            }
        }
        exit(0);  
    } else if (b > 0) {
        add_process_to_file();
    }
    
    c = fork();
    if (c == 0) {
        add_process_to_file(); 
        if (getpid() % 2 == 1) {
            pid_t extra_child = fork();
            if (extra_child == 0) {
                add_process_to_file();
                exit(0);
            } else if (extra_child > 0) {
                wait(NULL);  
            }
        }
        exit(0);  
    } else if (c > 0) {
        add_process_to_file();
    }
    
    if (getpid() == original_parent_pid) {
        
        int status;
        while (wait(&status) > 0) {
        }
       
        int total_processes = 1;  
        
        file = fopen("p_count.txt", "r");
        if (file != NULL) {
            char line[100];
            int file_count = 0;
            while (fgets(line, sizeof(line), file) != NULL) {
                file_count++;
            }
            total_processes += file_count;
            fclose(file); 
        }
        
        printf("Total processes created (including parent): %d\n", total_processes);
        
    } else {
        exit(0);
    }
    
    return 0;
}