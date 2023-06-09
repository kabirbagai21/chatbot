#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "util.h"

int client_socket = -1;
char username[MAX_NAME_LEN + 1];
char inbuf[BUFLEN + 1];
char outbuf[MAX_MSG_LEN + 1];
bool redirect = false;

int handle_stdin() {
    if(redirect){
	if (fgets(outbuf, MAX_MSG_LEN + 1, stdin) == NULL) {
            if (feof(stdin)) {
		close(client_socket);
                exit(EXIT_SUCCESS);
            } else {
                fprintf(stderr, "Error: Failed to read message from file. %s.\n",
                        strerror(errno));
                return EXIT_FAILURE;
            }
       }
       else if(strlen(outbuf) >= MAX_MSG_LEN){
	    fprintf(stderr, "Sorry, limit your message to 1 line of at most %d characters.\n\n", MAX_MSG_LEN);
            close(client_socket);
	    exit(EXIT_FAILURE);
       }
    }
    else{
        if(fgets(outbuf, MAX_MSG_LEN + 1, stdin) == NULL){
       	    if(feof(stdin)){
	        close(client_socket);
		exit(EXIT_SUCCESS);
	    }

	    else if(ferror(stdin)){
	        fprintf(stderr, "Error: Failed to read from stdin. %s.\n", strerror(errno));
	        return EXIT_FAILURE;
            }
        }

	else if(strlen(outbuf) >= MAX_MSG_LEN){
            fprintf(stderr, "Sorry, limit your message to 1 line of at most %d characters.\n", MAX_MSG_LEN);
	    int c;
            while ((c = getchar()) != '\n' && c != EOF) {}
	    return EXIT_SUCCESS;
	}
    }
    char *ptr = strchr(outbuf, '\n');
    
    if (ptr != NULL) {
        *ptr = '\0';
    }
    
    if (send(client_socket, outbuf, strlen(outbuf) + 1, 0) < 0) {
        fprintf(stderr, "Error: Failed to send message to server. %s.\n",
                strerror(errno));
         return EXIT_FAILURE;

    }
    if (strcmp(outbuf, "bye") == 0) {
        printf("Goodbye\n");
	close(client_socket);
	exit(EXIT_SUCCESS);
    }

    memset(outbuf, 0, sizeof(outbuf)); 

    return EXIT_SUCCESS;
    
}

int handle_client_socket() {

    int bytes_recvd, total_bytes = 0;
    inbuf[0] = '\0';
    
    while (total_bytes < MAX_MSG_LEN) {
        bytes_recvd = recv(client_socket, &inbuf[total_bytes], 1, 0);
        
	if (bytes_recvd < 0 && errno != EINTR) {
            printf("Warning: Failed to receive incoming message.");
            return EXIT_FAILURE;
        }
        else if (bytes_recvd == 0) {
            fprintf(stderr, "\nConnection to server has been lost.\n");
            close(client_socket);
	    exit(EXIT_FAILURE);
        }
        else {
            total_bytes++;
            if (inbuf[total_bytes-1] == '\0') {
                break;
            }
        }
    }

    if (strcmp(inbuf, "bye") == 0) {
        printf("\nServer initiated shutdown.\n");
        close(client_socket);
	exit(EXIT_SUCCESS);
    }

    printf("%s\n", inbuf);
    
    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server IP> <port number>\n", argv[0]);
        return EXIT_FAILURE;
    }

    if(!isatty(STDIN_FILENO)){
	  redirect = true;
    }

    int port, bytes_recvd, retval = EXIT_SUCCESS, opt = 1, user_len = 0;
    if (!parse_int(argv[2], &port, "port number")) {
        return EXIT_FAILURE;
    }
    
    if (port < 1024 || port > 65535) {
        fprintf(stderr, "Error: port must be in range [1024, 65535].\n");
        return EXIT_FAILURE;
    }
    
    struct sockaddr_in serv_addr;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    memset(&serv_addr, 0, addrlen);

    int ip_conversion = inet_pton(AF_INET, argv[1], &serv_addr.sin_addr);
    
    if (ip_conversion == 0) {
        fprintf(stderr, "Error: Invalid IP address '%s'.\n", argv[1]);
        return EXIT_FAILURE;
    } else if (ip_conversion < 0) {
        fprintf(stderr, "Error: Failed to convert IP address. %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }
    
    char username_cpy[MAX_NAME_LEN * 5];
    
    while(user_len <= 1 || user_len > MAX_NAME_LEN + 1){
        
	printf("Please enter a username: ");
	if(fgets(username_cpy, MAX_NAME_LEN * 5, stdin) == NULL){
            if(feof(stdin)){
                retval = EXIT_SUCCESS;
                goto EXIT;
            }

            else if(ferror(stdin)){
                fprintf(stderr, "Error: Failed to read from stdin. %s.\n", strerror(errno));
                retval = EXIT_FAILURE;
		goto EXIT;
            }
        }
	
        user_len = strlen(username_cpy);

        if(user_len > MAX_NAME_LEN + 1){
            fprintf(stderr, "Sorry, limit your username to %d characters.\n", MAX_NAME_LEN);
	    fflush(stdin);
	    continue;
	}
	else if(user_len <= 1){
              printf("Username cannot be empty.\n");
	      continue;
        }
    }
    
    char *ptr = strchr(username_cpy, '\n');
    if (ptr != NULL) {
        *ptr = '\0';
    }
    strncpy(username, username_cpy, MAX_NAME_LEN);
    
    printf("Hello, %s. Let's try to connect to the server.\n", username);
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Error: Failed to create socket. %s.\n",
                strerror(errno));
        retval = EXIT_FAILURE;
	goto EXIT;
        
    }

    if (setsockopt(client_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)) != 0) {
    	fprintf(stderr, "Error: Failed to set socket options. %s.\n",strerror(errno));
	retval = EXIT_FAILURE;
	goto EXIT;
    }

    if (connect(client_socket, (struct sockaddr *)&serv_addr, addrlen) < 0) {
        fprintf(stderr, "Error: Failed to connect to server. %s.\n",
                strerror(errno));
        retval = EXIT_FAILURE;
        goto EXIT;
    }

    if ((bytes_recvd = recv(client_socket, inbuf, BUFLEN + 1, 0)) < 0) {
        fprintf(stderr, "Error: Failed to receive message from server. %s.\n",
                strerror(errno));
        retval = EXIT_FAILURE;
        goto EXIT;
    }
    
    printf("\n%s\n\n", inbuf);
    
     if (send(client_socket, username, strlen(username) + 1, 0) < 0) {
        fprintf(stderr, "Error: Failed to send message to server. %s.\n",
                strerror(errno));
        retval = EXIT_FAILURE;
        goto EXIT;
    }

    fd_set sockset;
    int max_socket;

    while(true){
        FD_ZERO(&sockset);
        FD_SET(client_socket, &sockset);
	FD_SET(STDIN_FILENO, &sockset);
        max_socket = client_socket;
	outbuf[0] = '\0';
        
        if (select(max_socket + 1, &sockset, NULL, NULL, NULL) < 0
                && errno != EINTR) {
            fprintf(stderr, "Error: select() failed. %s.\n", strerror(errno));
            retval = EXIT_FAILURE;
            goto EXIT;
        }

        if (FD_ISSET(STDIN_FILENO, &sockset)) {
            if (handle_stdin() == EXIT_FAILURE) {
                retval = EXIT_FAILURE;
                goto EXIT;
            }
        }
      
        if (FD_ISSET(client_socket, &sockset)) {
            if (handle_client_socket() == EXIT_FAILURE) {
                retval = EXIT_FAILURE;
                goto EXIT;
            }
        }
    }


    EXIT:
    if (fcntl(client_socket, F_GETFD) != -1) {
        close(client_socket);
    }
    return retval;

}
