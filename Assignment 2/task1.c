#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

typedef struct {
    int n;
    int* fib;
    int done;
} FibData;

typedef struct {
    FibData* fibdata;
    int* search;
    int* result;
    int count;
} SearchData;

void* fib_thread(void* arg) {
    FibData* data = (FibData*)arg;
    int n = data->n;
    
    data->fib = (int*)malloc((n + 1) * sizeof(int));
    if (data->fib == NULL) {
        printf("Memory allocation failed!\n");
        data->done = 0;
        return NULL;
    }
    
    if (n >= 0) data->fib[0] = 0;
    if (n >= 1) data->fib[1] = 1;
    
    for (int i = 2; i <= n; i++) {
        data->fib[i] = data->fib[i-1] + data->fib[i-2];
    }
    
    data->done = 1;
    return NULL;
}

void* search_thread(void* arg) {
    SearchData* sdata = (SearchData*)arg;
    FibData* fibdata = sdata->fibdata;
    
    while (!fibdata->done) {
        usleep(1000);
    }
    
    for (int i = 0; i < sdata->count; i++) {
        int idx = sdata->search[i];
        if (idx >= 0 && idx <= fibdata->n) {
            sdata->result[i] = fibdata->fib[idx];
        } else {
            sdata->result[i] = -1;
        }
    }
    
    return NULL;
}

int main() {
    int n, count;
    FILE *fp;
    
    printf("Enter the term of fibonacci sequence:\n");
    scanf("%d", &n);
    
    printf("How many numbers you are willing to search?:\n");
    scanf("%d", &count);
    
    FibData fibdata = {n, NULL, 0};
    
    int* search = (int*)malloc(count * sizeof(int));
    int* result = (int*)malloc(count * sizeof(int));
    
    if (search == NULL || result == NULL) {
        printf("Memory allocation failed!\n");
        return 1;
    }
    
    for (int i = 0; i < count; i++) {
        printf("Enter search %d:\n", i + 1);
        scanf("%d", &search[i]);
    }
    
    SearchData sdata = {&fibdata, search, result, count};
    
    pthread_t fib_t, search_t;
    
    if (pthread_create(&fib_t, NULL, fib_thread, &fibdata) != 0) {
        printf("Error creating fibonacci thread!\n");
        return 1;
    }
    
    if (pthread_create(&search_t, NULL, search_thread, &sdata) != 0) {
        printf("Error creating search thread!\n");
        return 1;
    }
    
    pthread_join(fib_t, NULL);
    pthread_join(search_t, NULL);
    
    fp = fopen("task1_output.txt", "w");
    if (fp == NULL) {
        printf("Error opening output file!\n");
        return 1;
    }
    
    for (int i = 0; i <= n; i++) {
        printf("a[%d] = %d\n", i, fibdata.fib[i]);
        fprintf(fp, "a[%d] = %d\n", i, fibdata.fib[i]);
    }
    
    for (int i = 0; i < count; i++) {
        printf("result of search #%d = %d\n", i + 1, result[i]);
        fprintf(fp, "result of search #%d = %d\n", i + 1, result[i]);
    }
    
    fclose(fp);
    
    free(fibdata.fib);
    free(search);
    free(result);
    
    return 0;
}