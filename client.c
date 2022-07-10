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
#include "string.h"

#define LENGTH_NAME 31
#define LENGTH_MSG 101
#define PORT 8080
#define MAX_ROOM_STRING_LEN 3

volatile sig_atomic_t flag = 0;
int sockfd =  0;
char nickname[LENGTH_NAME] = {};
char room[MAX_ROOM_STRING_LEN] = {};
char password[] = {"password"};

void catch(int sig) {
	flag = 1;
}

//Function to print out ">"
void str_overwrite_stdout() {
    printf("\r%s", "> ");
    fflush(stdout);
}

//Function that removes the "\n" from strings 
void str_trim_lf (char* arr, int length) {
    int i;
    for (i = 0; i < length; i++) { // trim \n
        if (arr[i] == '\n') {
            arr[i] = '\0';
            break;
        }
    }
}

//Function that handles revieved messages from the server or other clients
void recv_message() {
	char rcv_message[LENGTH_MSG] = {};

	while(1) {
		int recieve = recv(sockfd, rcv_message, LENGTH_MSG, 0);

		if (recieve > 0) {
			printf("\r%s\n", rcv_message);
            str_overwrite_stdout();
		} else if (recieve == 0) {
			break;
		}
	}
}

//Function that handles sending messages from the client
void send_msg() {
	char message[LENGTH_MSG] = {};
	char pass_entry[sizeof(password)] = {};
	int confirmation = 0;

	while(1) {

		str_overwrite_stdout();
		while (fgets(message, LENGTH_MSG, stdin) != NULL) {
			str_trim_lf(message, LENGTH_MSG);
			if (strlen(message) == 0) {
				str_overwrite_stdout();
			} else {
				break;	
			}
		}
		
		if (strcmp(message, "/exit") == 0) {
			break;
		}
		else if (strcmp(message, "/changeroom") == 0) { //Room change
			send(sockfd, message, LENGTH_MSG, 0);
			do { //Validation
				printf("Please enter room number (1 - 99)\n");
				fgets(room, MAX_ROOM_STRING_LEN, stdin);
			}while (atoi(room) < 1 || atoi(room) > 99); 

			memset(message, 0, LENGTH_MSG);
			strcpy(message, room);
			str_trim_lf(message, LENGTH_MSG);
			printf("Welcome to room %d\n", atoi(room)); 
		}
		else if (strcmp(message, "/admin") == 0) { //Ask for admin rights + validation
			send(sockfd, message, LENGTH_MSG, 0);
			printf("Please enter admin password\n");
			fgets(pass_entry, sizeof(password), stdin);
			if (strcmp(pass_entry, password) == 0) {		
				confirmation = 1;		
				send(sockfd, (void*)&confirmation, sizeof(int), 0);
				printf("Welcome Admin\n");
				continue;
			}
			else {
				printf("Wrong Password\n");
				continue;
			}
		}
		else if (strcmp(message, "/broadcast") == 0 && confirmation == 1) { //Broadcast
			send(sockfd, message, LENGTH_MSG, 0);
			if (confirmation == 1) {
				printf("Enter broadcast message:\n");
				fgets(message, LENGTH_MSG, stdin);
				str_trim_lf(message, LENGTH_MSG);
				send(sockfd, message, LENGTH_MSG, 0);
				continue;
			}
		}
		else if (strcmp(message, "/broadcast") == 0 && confirmation == 0) {
			printf("Not within regular user rights\n");
			continue;
		}
		
		send(sockfd, message, LENGTH_MSG, 0);
	}
	catch(2);
}

int main() {
	signal(SIGINT, catch);

	printf("Please enter your name\n");
	fgets(nickname, LENGTH_NAME, stdin);

	//Naming validation
	if (strlen(nickname) < 2 || strlen(nickname) > LENGTH_NAME - 1) {
		printf("Name must be more than 2 and less than 30 characters\n");
		exit(EXIT_FAILURE);
	} else {
		str_trim_lf(nickname, LENGTH_NAME);
	}

	//Room validation
	do {
		printf("Please enter room number (1 - 99)\n");
		fgets(room, MAX_ROOM_STRING_LEN, stdin);
	}while (atoi(room) < 1 || atoi(room) > 99);

	//Create socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		printf("Failed to create socket\n");
		exit(EXIT_FAILURE);
	}

	//Socket information
	struct sockaddr_in server_info, client_info;
    	int s_addrlen = sizeof(server_info);
    	int c_addrlen = sizeof(client_info);
    	memset(&server_info, 0, s_addrlen);
    	memset(&client_info, 0, c_addrlen);
    	server_info.sin_family = PF_INET;
    	server_info.sin_addr.s_addr = inet_addr("127.0.0.1");
    	server_info.sin_port = htons(PORT);

	//Connect to server
	int err = connect(sockfd, (struct sockaddr *)&server_info, s_addrlen);
	if (err == -1) {
		printf("Socket failed to connect\n");
		exit(EXIT_FAILURE);
	}

	getpeername(sockfd, (struct sockaddr*) &server_info, (socklen_t*) &s_addrlen);
	printf("Connected to server: %s:%d\nWelcome to room %s\n", inet_ntoa(server_info.sin_addr), ntohs(server_info.sin_port), room);

	//Sends both the name and the room number back to the server
	send(sockfd, nickname, LENGTH_NAME, 0);
	send(sockfd, room, sizeof(room), 0);

	pthread_t send_msg_thread;
	if(pthread_create(&send_msg_thread, NULL, (void*)send_msg, NULL) != 0) {
		printf("Pthread creation error\n");
		exit(EXIT_FAILURE);
	}

	pthread_t recv_msg_thread;
    	if (pthread_create(&recv_msg_thread, NULL, (void *) recv_message, NULL) != 0) {
        	printf ("Create pthread error!\n");
        	exit(EXIT_FAILURE);
    	}

	//Runs until client leaves then prints out "Bye"
	while (1) {
        if(flag) {
            printf("\nBye\n");
            break;
        }
    }

	close(sockfd);

	return 0;
}
