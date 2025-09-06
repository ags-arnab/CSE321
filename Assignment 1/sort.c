#include <stdio.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("No numbers provided to sort\n");
        return 0;
    }
    
    int n = argc - 1;
    int arr[100];
    
    for (int i = 0; i < n; i++) {
        int num = 0;
        int j = 0;
        int sign = 1;
        
        if (argv[i + 1][0] == '-') {
            sign = -1;
            j = 1;
        }
        
        while (argv[i + 1][j] != '\0') {
            num = num * 10 + (argv[i + 1][j] - '0');
            j++;
        }
        arr[i] = num * sign;
    }
    
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (arr[j] < arr[j + 1]) {
                int temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
    
    printf("Sorted array (descending): ");
    for (int i = 0; i < n; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");
    
    return 0;
}