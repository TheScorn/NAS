#include "NAS.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

void* NAS_handle(void* arg) {

    struct handle_args_struct* args = arg;
    int client_fd = args->client_fd;
    bool verbose = args->verbose_init;

    free(args);


    ////////////////////////////////////////////////////
    char request_type = 0;

    //Typy requestów
    //0 NONE - nierozpoznany request
    //1 lista plików
    //2 get file
    //3 put file
    //4 Test con

    //jak zrobić listowanie plików jeśli uznamy że foldery są legalne.

    

    int buffer_size = DEFAULT_BUFFER_SIZE;
    char *buffer = (char*)malloc(buffer_size * sizeof(char));

    ssize_t bytes_received = recv(client_fd, buffer, buffer_size, 0);

    if(bytes_received == 0) {
        free(buffer);
        close(client_fd);
        return NULL;
    }

    
    //czy typ 4
    if(strcasecmp(buffer, "TEST") == 0) {
        request_type = 4;
    }

    
    ////////////////////////////////////////////////////////////////////////
    //TEST handle
    if(request_type == 4) {
        char response[] = "ACK";
        size_t response_len = strlen(response);

        size_t total = 0;
        while(total < response_len) {
            ssize_t n = send(client_fd, response + total, response_len - total, 0);
            if(n <= 0) {
                fprintf(stderr, "0 bytes sent. Breaking.\n");
                break;
            }
            total += n;
        }

        free(buffer);
        close(client_fd);
        return NULL;
    }


    else {
        if(verbose) {
            fprintf(stderr, "Unknown request, no data sent.\n");
        }
        close(client_fd);
        free(buffer);
        return NULL;
    }



    close(client_fd);
    return NULL;

}
