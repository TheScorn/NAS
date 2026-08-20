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