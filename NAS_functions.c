#include "NAS.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>


/**
 * @brief Function encapsulates prefix determination routine
 * 
 * @param client_fd int describing socket on which connection is established
 * 
 * @param prefix_numerical address of unsigned long long where the message lenght should be stored.
 * 
 * @returns 0 if execution successful, -1 if client closed connection,
 * -2 if recv error occured, -3 if conversion failed.
 * 
 */
int get_prefix(int client_fd, unsigned long long* prefix_numerical) {
    char* prefix = (char*)malloc(sizeof(char) * DEFAULT_PREFIX_SIZE + 1);
    size_t recieved = 0;
    while(recieved < DEFAULT_PREFIX_SIZE) {
        ssize_t n = recv(client_fd, prefix + recieved, DEFAULT_PREFIX_SIZE - recieved, 0);

        if(n == 0) {
            free(prefix);
            close(client_fd);
            return -1;
        }
        else if(n < 0) {
            fprintf(stderr, "Recv error occured in prefix determination!\n");
            free(prefix);
            close(client_fd);
            return -2;
        }
        recieved += n;

    }

    *(prefix + DEFAULT_PREFIX_SIZE) = '\0';

    int conversion_status = determine_length(prefix_numerical, prefix);
    free(prefix);

    if(conversion_status != 0) {
        close(client_fd);
        return -3;
    }

    return 0;
}

/**
 * @brief Function encapsulates sending routine
 * 
 * Function calculates message len, then sends it using while.
 * Note that function never closes the client_fd socket nor does it free message.
 * 
 * @param message null terminated string containing prefixed message
 * 
 * @returns 0 if execution successful, -1 if error occured during send.
 */
int send_routine(int client_fd, char* message, size_t message_len) {

    size_t total = 0;
    while(total < message_len) {
        ssize_t n = send(client_fd, message + total, message_len - total, 0);
        if(n <= 0) {
            return -1;
        }
        total += n;

    }

}

int send_ACCEPT(int sockD) {
    char accept_message[] = "0000000000000006ACCEPT";
    size_t message_len = strlen(accept_message);

    size_t total = 0;
    while(total < message_len) {
        ssize_t n = send(sockD, accept_message + total, message_len - total, 0);
        if(n <= 0) {
            fprintf(stderr, "Error occured during send in ACCEPT.\n");
            return -1;
        }
        total += n;
    }
    return 0;

}

int send_REFUSE(int sockD) {
    char refuse_message[] = "0000000000000006REFUSE";
    size_t message_len = strlen(refuse_message);

    size_t total = 0;
    while(total < message_len) {
        ssize_t n = send(sockD, refuse_message + total, message_len - total, 0);
        if(n <= 0) {
            fprintf(stderr, "Error occured during send in REFUSE.\n");
            return -1;
        }
        total += n;
    }
    return 0;


}

int send_ACK(int client_fd) {
    char ack_message[] = "0000000000000003ACK";
    size_t response_len = strlen(ack_message);

    int send_status = send_routine(client_fd, ack_message, response_len);
    if(send_status == -1) {
        fprintf(stderr, "Function send_ACK; 0 bytes sent. Closing connection.\n");
        close(client_fd);
        return -1;
    }
    return 0;
}