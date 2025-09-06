#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>

#define NUM_STUDENTS 10
#define NUM_CHAIRS 3

sem_t *chairs;
sem_t *students;
sem_t *st_ready;
sem_t *done;
pthread_mutex_t waiting_lock;
pthread_mutex_t served_lock;
pthread_mutex_t file_lock;

int waiting = 0;
int served = 0;
int finished = 0;
FILE *output_fp;

void* student(void* arg) {
    int id = *(int*)arg;
    
    if (sem_trywait(chairs) == 0) {
        pthread_mutex_lock(&waiting_lock);
        waiting++;
        printf("Student %d started waiting for consultation\n", id);
        pthread_mutex_lock(&file_lock);
        fprintf(output_fp, "Student %d started waiting for consultation\n", id);
        pthread_mutex_unlock(&file_lock);
        pthread_mutex_unlock(&waiting_lock);
        
        sem_post(students);
        sem_wait(st_ready);
        
        pthread_mutex_lock(&waiting_lock);
        waiting--;
        printf("A waiting student started getting consultation\n");
        printf("Number of students now waiting: %d\n", waiting);
        printf("ST giving consultation\n");
        printf("Student %d is getting consultation\n", id);
        pthread_mutex_lock(&file_lock);
        fprintf(output_fp, "A waiting student started getting consultation\n");
        fprintf(output_fp, "Number of students now waiting: %d\n", waiting);
        fprintf(output_fp, "ST giving consultation\n");
        fprintf(output_fp, "Student %d is getting consultation\n", id);
        pthread_mutex_unlock(&file_lock);
        pthread_mutex_unlock(&waiting_lock);
        
        sem_post(chairs);
        sleep(1 + rand() % 2);
        
        pthread_mutex_lock(&served_lock);
        finished++;
        printf("Student %d finished getting consultation and left\n", id);
        served++;
        printf("Number of served students: %d\n", served);
        pthread_mutex_lock(&file_lock);
        fprintf(output_fp, "Student %d finished getting consultation and left\n", id);
        fprintf(output_fp, "Number of served students: %d\n", served);
        pthread_mutex_unlock(&file_lock);
        pthread_mutex_unlock(&served_lock);
        
        sem_post(done);
        
    } else {
        pthread_mutex_lock(&served_lock);
        finished++;
        printf("No chairs remaining in lobby. Student %d Leaving.....\n", id);
        printf("Student %d finished getting consultation and left\n", id);
        served++;
        printf("Number of served students: %d\n", served);
        pthread_mutex_lock(&file_lock);
        fprintf(output_fp, "No chairs remaining in lobby. Student %d Leaving.....\n", id);
        fprintf(output_fp, "Student %d finished getting consultation and left\n", id);
        fprintf(output_fp, "Number of served students: %d\n", served);
        pthread_mutex_unlock(&file_lock);
        pthread_mutex_unlock(&served_lock);
    }
    
    return NULL;
}

void* st(void* arg) {
    while (finished < NUM_STUDENTS) {
        sem_wait(students);
        
        pthread_mutex_lock(&waiting_lock);
        int serve = (waiting > 0);
        pthread_mutex_unlock(&waiting_lock);
        
        if (serve) {
            sem_post(st_ready);
            sem_wait(done);
        }
    }
    
    return NULL;
}

int main() {
    pthread_t student_t[NUM_STUDENTS];
    pthread_t st_t;
    int ids[NUM_STUDENTS];
    
    srand(time(NULL));
    
    output_fp = fopen("task2_output.txt", "w");
    if (output_fp == NULL) {
        printf("Error opening output file!\n");
        return 1;
    }
    
    sem_unlink("/chairs");
    sem_unlink("/students");
    sem_unlink("/st_ready");
    sem_unlink("/done");
    
    chairs = sem_open("/chairs", O_CREAT, 0644, NUM_CHAIRS);
    students = sem_open("/students", O_CREAT, 0644, 0);
    st_ready = sem_open("/st_ready", O_CREAT, 0644, 0);
    done = sem_open("/done", O_CREAT, 0644, 0);
    
    if (chairs == SEM_FAILED || students == SEM_FAILED || 
        st_ready == SEM_FAILED || done == SEM_FAILED) {
        perror("sem_open failed");
        return 1;
    }
    
    pthread_mutex_init(&waiting_lock, NULL);
    pthread_mutex_init(&served_lock, NULL);
    pthread_mutex_init(&file_lock, NULL);
    
    pthread_create(&st_t, NULL, st, NULL);
    
    for (int i = 0; i < NUM_STUDENTS; i++) {
        ids[i] = i;
        pthread_create(&student_t[i], NULL, student, &ids[i]);
        usleep(300000 + rand() % 700000);
    }
    
    for (int i = 0; i < NUM_STUDENTS; i++) {
        pthread_join(student_t[i], NULL);
    }
    
    pthread_cancel(st_t);
    pthread_join(st_t, NULL);
    
    fclose(output_fp);
    
    sem_close(chairs);
    sem_close(students);
    sem_close(st_ready);
    sem_close(done);
    sem_unlink("/chairs");
    sem_unlink("/students");
    sem_unlink("/st_ready");
    sem_unlink("/done");
    
    pthread_mutex_destroy(&waiting_lock);
    pthread_mutex_destroy(&served_lock);
    pthread_mutex_destroy(&file_lock);
    
    return 0;
}