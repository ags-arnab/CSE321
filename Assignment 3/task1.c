#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/wait.h>

struct shared{
    char sel[100];
    int b;
};

int main(){
    int sm_id;
    void *s_m;
    struct shared *shared_data;
    int fd[2];
    pid_t p_id;
    char selection;
    char pipe_msg[100];
    
    sm_id = shmget((key_t)101, sizeof(struct shared), 0666|IPC_CREAT);
    if(sm_id == -1){
        perror("shmget");
        exit(1);
    }
    
    s_m = shmat(sm_id, NULL, 0);
    if(s_m == (void *)-1){
        perror("shmat");
        exit(1);
    }
    shared_data = (struct shared *)s_m;
    
    if(pipe(fd) == -1){
        perror("pipe");
        exit(1);
    }
    
    printf("Provide Your Input From Given Options:\n");
    printf("1. Type a to Add Money\n");
    printf("2. Type w to Withdraw Money\n");
    printf("3. Type c to Check Balance\n");
    scanf(" %c", &selection);
    
    shared_data->sel[0] = selection;
    shared_data->sel[1] = '\0';
    shared_data->b = 1000;  
    
    printf("Your selection: %c\n", selection);
    fflush(stdout);
    
    p_id = fork();
    
    if(p_id < 0){
        perror("fork");
        exit(1);
    }
    else if(p_id == 0){
        close(fd[0]); 
        
        char user_selection = shared_data->sel[0];
        int amount;
        
        if(user_selection == 'a'){
            printf("Enter amount to be added:\n");
            scanf("%d", &amount);
            
            if(amount > 0){
                shared_data->b += amount;
                printf("Balance added successfully\n");
                printf("Updated balance after addition:\n%d\n", shared_data->b);
            }
            else{
                printf("Adding failed, Invalid amount\n");
            }
        }
        else if(user_selection == 'w'){
            printf("Enter amount to be withdrawn:\n");
            scanf("%d", &amount);
            
            if(amount > 0 && amount <= shared_data->b){
                shared_data->b -= amount;
                printf("Balance withdrawn successfully\n");
                printf("Updated balance after withdrawal:\n%d\n", shared_data->b);
            }
            else{
                printf("Withdrawal failed, Invalid amount\n");
            }
        }
        else if(user_selection == 'c'){
            printf("Your current balance is:\n%d\n", shared_data->b);
        }
        else{
            printf("Invalid selection\n");
        }
        
        strcpy(pipe_msg, "Thank you for using");
        write(fd[1], pipe_msg, strlen(pipe_msg) + 1);
        close(fd[1]);
        
        exit(0);
    }
    else{
        close(fd[1]); 
        
        wait(NULL);
        
        read(fd[0], pipe_msg, sizeof(pipe_msg));
        printf("%s\n", pipe_msg);
        close(fd[0]);
        
        shmctl(sm_id, IPC_RMID, NULL);
    }
    
    return 0;
}