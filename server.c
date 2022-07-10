#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "server.h"

#define LENGTH_NAME 31
#define LENGTH_MSG 101
#define PORT 8080

int server_sockfd = 0, client_sockfd = 0;
ClientList *root, *now;

//Function that sends to all other clients in that room
void send_to_all_clients(ClientList *np, char tmp_buffer[]) {
    ClientList *tmp = root->link;

    while (tmp != NULL) {
        if(np->data != tmp->data && np->room_num == tmp->room_num) {
            printf("Sent to sockfd %d: \"%s\" \n", tmp->data, tmp_buffer);
            send(tmp->data, tmp_buffer, LENGTH_MSG, 0);
        }
        tmp = tmp->link;
    }
}

void broadcast() {

}

// Function that handles all the clients entering or leaving the chat room
void client_handler(void *p_client)
{
	int leave_flag = 0;
    char room[3] = {};
    char nickname[LENGTH_NAME] = {};
    char recv_buffer[LENGTH_MSG] = {};
    char send_buffer[LENGTH_MSG] = {};
    ClientList *np = (ClientList *)p_client;

    //Naming
    if (recv(np->data, nickname, LENGTH_NAME, 0) <= 0 || strlen(nickname) < 2 || strlen(nickname) >= LENGTH_NAME - 1) {
        printf("%s didn't input name.\n", np->ip);
        leave_flag = 1;
    } else {
        strncpy(np->name, nickname, LENGTH_NAME);
        recv(np->data, room, sizeof(int), 0);
        np->room_num = atoi(room);
        printf("%s(%s)(%d) joined chatroom: %d.\n", np->name, np->ip, np->data, np->room_num);
        sprintf(send_buffer, "%s joined the chatroom.", np->name);
        send_to_all_clients(np, send_buffer);
    }
    
    //Conversation
    while(1) {
        if (leave_flag) {
            break;
        }

        int recieve = recv(np->data, recv_buffer, LENGTH_MSG, 0);
        if (strcmp(recv_buffer, "/changeroom") == 0) {
            recv(np->data, room, sizeof(room), 0);
            np->room_num = atoi(room);
            sprintf(send_buffer, "%s joined room %d", np->name, np->room_num);
        } 
        else if (recieve == 0 || strcmp(recv_buffer, "/exit") == 0) {
            printf("%s(IP: %s)(Sockfd: %d) left chatroom: %d.\n", np->name, np->ip, np->data, np->room_num);
            sprintf(send_buffer, "%s(IP: %s) left the chat.", np->name, np->ip);
            leave_flag = 1;
        } 
        else if (recieve > 0) {
            if (strlen(recv_buffer) == 0) {
                continue;
            }
            sprintf(send_buffer, "%s: %s", np->name, recv_buffer);
        }
        else {
            printf("ERROR\n");
            leave_flag = 1;
        }
        send_to_all_clients(np,  send_buffer);

    }

    //Removing a node
    close(np->data);
    if(np == now) { //last node
        now = np->prev;
        now->link = NULL;
    } else { //middle node
        np->prev->link = np->link;
        np->link->prev = np->prev;
    }
    free(np);
}

int main()
{
    //Socket init and validation
	server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sockfd == -1) {
        printf("Failed to create socket.");
        exit(EXIT_FAILURE);
    } else {
        printf("Socket created successfully...\n");
    }

    //Socket informtion
    struct sockaddr_in server_info, client_info;
    int s_addrlen = sizeof(server_info);
    int c_addrlen = sizeof(client_info);
    memset(&server_info, 0, s_addrlen);
    memset(&client_info, 0, c_addrlen);
    server_info.sin_family = PF_INET;
    server_info.sin_addr.s_addr = INADDR_ANY;
    server_info.sin_port = htons(PORT);

    //Bind validation
    if (bind(server_sockfd, (struct sockaddr *)&server_info, s_addrlen) < 0) {
        printf("ERROR: Binding\n");
    } else {
        printf("Socket bound successfully...\n");
    }

    //Connection validation
    if (listen(server_sockfd, 5)) {
        printf("ERROR: Listening\n");
    } else { 
        printf("Socket listening successfully...\n");
    }

    //Create client list
    root = newNode(server_sockfd, inet_ntoa(server_info.sin_addr));
    now = root;

    while (1) {
        client_sockfd = accept(server_sockfd, (struct sockaddr*)&client_info, (socklen_t*)&c_addrlen);

        //Append client to client list
        ClientList *c = newNode(client_sockfd, inet_ntoa(client_info.sin_addr));
        c->prev = now;
        now->link = c;
        now = c;

        pthread_t id;
        if(pthread_create(&id, NULL, (void *)client_handler, (void *)c) != 0) {
            perror("Pthread error\n");
            exit(EXIT_FAILURE);
        }
    }
    return 0;
}
