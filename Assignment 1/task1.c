#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    int filehandle;
    char input[256];
    
    if (argc != 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        exit(1);
    }
    
    filehandle = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (filehandle < 0) {
        perror("File open failed");
        exit(1);
    }
    
    printf("Enter strings (enter '-1' to stop):\n");
    
    while (1) {
        printf("> ");
        fflush(stdout);
        
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        
        // Remove newline character if present
        size_t len = strlen(input);
        if (len > 0 && input[len-1] == '\n') {
            input[len-1] = '\0';
            len--;
        }
        
        if (strcmp(input, "-1") == 0) {
            break;
        }
        
        write(filehandle, input, len);
        write(filehandle, "\n", 1);
    }
    
    close(filehandle);
}