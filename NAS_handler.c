#include "NAS.h"
#include <string.h>
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


    close(client_fd);
    return NULL;

}
