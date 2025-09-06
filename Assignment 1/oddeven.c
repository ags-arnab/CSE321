#include <stdio.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("No numbers provided to check\n");
        return 0;
    }
    
    printf("Odd/Even status:\n");
    
    for (int i = 1; i < argc; i++) {
        int num = 0;
        int j = 0;
        int sign = 1;
        
        if (argv[i][0] == '-') {
            sign = -1;
            j = 1;
        }
        
        while (argv[i][j] != '\0') {
            num = num * 10 + (argv[i][j] - '0');
            j++;
        }
        num = num * sign;
        
        if (num % 2 == 0) {
            printf("%d is even\n", num);
        } else {
            printf("%d is odd\n", num);
        }
    }
    
    return 0;
}