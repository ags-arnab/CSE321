#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <sys/wait.h>

struct message{
    long int type;
    char txt[100];
};

int main(){
    int msg_id;
    pid_t otp_gen_pid, mail_pid;
    struct message message;
    char workspace[100];
    
    msg_id = msgget((key_t)123, 0666|IPC_CREAT);
    if(msg_id == -1){
        perror("msgget");
        exit(1);
    }
    
    printf("Please enter the workspace name:\n");
    scanf("%s", workspace);
    
    if(strcmp(workspace, "cse321") != 0){
        printf("Invalid workspace name\n");
        msgctl(msg_id, IPC_RMID, 0);
        exit(0);
    }
    
    message.type = 1;
    strcpy(message.txt, workspace);
    if(msgsnd(msg_id, (void *)&message, sizeof(message.txt), 0) == -1){
        perror("msgsnd");
        exit(1);
    }
    printf("Workspace name sent to otp generator from log in: %s\n", workspace);
    fflush(stdout);
    
    otp_gen_pid = fork();
    
    if(otp_gen_pid < 0){
        perror("fork");
        exit(1);
    }
    else if(otp_gen_pid == 0){
        struct message recv_msg, send_msg;
        char otp[100];
        
        if(msgrcv(msg_id, (void *)&recv_msg, sizeof(recv_msg.txt), 1, 0) == -1){
            perror("msgrcv");
            exit(1);
        }
        printf("OTP generator received workspace name from log in: %s\n", recv_msg.txt);
        
        pid_t my_pid = getpid();
        sprintf(otp, "%d", my_pid);
        
        send_msg.type = 2;
        strcpy(send_msg.txt, otp);
        if(msgsnd(msg_id, (void *)&send_msg, sizeof(send_msg.txt), 0) == -1){
            perror("msgsnd");
            exit(1);
        }
        printf("OTP sent to log in from OTP generator: %s\n", otp);
        
        send_msg.type = 3;
        strcpy(send_msg.txt, otp);
        if(msgsnd(msg_id, (void *)&send_msg, sizeof(send_msg.txt), 0) == -1){
            perror("msgsnd");
            exit(1);
        }
        printf("OTP sent to mail from OTP generator: %s\n", otp);
        fflush(stdout);
        
        mail_pid = fork();
        
        if(mail_pid < 0){
            perror("fork");
            exit(1);
        }
        else if(mail_pid == 0){
            struct message mail_recv_msg, mail_send_msg;
            
            if(msgrcv(msg_id, (void *)&mail_recv_msg, sizeof(mail_recv_msg.txt), 3, 0) == -1){
                perror("msgrcv");
                exit(1);
            }
            printf("Mail received OTP from OTP generator: %s\n", mail_recv_msg.txt);
            
            mail_send_msg.type = 4;
            strcpy(mail_send_msg.txt, mail_recv_msg.txt);
            if(msgsnd(msg_id, (void *)&mail_send_msg, sizeof(mail_send_msg.txt), 0) == -1){
                perror("msgsnd");
                exit(1);
            }
            printf("OTP sent to log in from mail: %s\n", mail_send_msg.txt);
            fflush(stdout);
            
            exit(0);
        }
        else{
            wait(NULL);
            exit(0);
        }
    }
    else{
        struct message otp_msg, mail_msg;
        
        wait(NULL);
        
        if(msgrcv(msg_id, (void *)&otp_msg, sizeof(otp_msg.txt), 2, 0) == -1){
            perror("msgrcv");
            exit(1);
        }
        printf("Log in received OTP from OTP generator: %s\n", otp_msg.txt);
        
        if(msgrcv(msg_id, (void *)&mail_msg, sizeof(mail_msg.txt), 4, 0) == -1){
            perror("msgrcv");
            exit(1);
        }
        printf("Log in received OTP from mail: %s\n", mail_msg.txt);
        
        if(strcmp(otp_msg.txt, mail_msg.txt) == 0){
            printf("OTP Verified\n");
        }
        else{
            printf("OTP Incorrect\n");
        }
        
        msgctl(msg_id, IPC_RMID, 0);
    }
    
    return 0;
}