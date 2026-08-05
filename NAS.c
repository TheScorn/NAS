#define _POSIX_C_SOURCE 200809L

#include "NAS.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>

static volatile sig_atomic_t run;
static void sig_handler(int _);


int main(int argc, char **argv) {
    
    run = true;

    struct input_args_struct input_args;
    input_args.verbose_init = VERBOSE_INIT_DEFAULT;
    input_args.print_help = false;
    input_args.selected_port = DEFAULT_PORT;

    //funkcja do handle input
    int handle_args_status = handle_arguments(argc, argv, &input_args);

    //zamiast mówić o errorze to od razu printujemy help
    if(input_args.print_help) {
        printf(HELP_MESSAGE);
        if(handle_args_status == -1) {
            return -6;
        }
        else {
            return 0;
        }
    }


    printf("NAS server initializing\n");
    
    if(input_args.verbose_init) {
        printf("Version: %d.%d\n", NAS_VERSION_MAJOR, NAS_VERSION_MINOR);
    }

    int test_con_status = test_con();
    if(test_con_status == -1) {
        fprintf(stderr, "No sqlite database found\n");
        return -6;
    }
    if(input_args.verbose_init) {
        printf("Db connection successful\n");
    }

    //potrzebna będzie baza danych użytkowników bo w każdym requeście przychodzi login i hasło
    //(szyfrowane ale to na później)


    //create socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(serverSocket == -1) {
        fprintf(stderr, "Error occured while creating socket\n");
        close(serverSocket);
        return -1;
    }
    if(input_args.verbose_init) {
        printf("Socket created\n");
    }
    
    struct sockaddr_in hint;
    memset(&hint, 0, sizeof(hint));
    hint.sin_family = AF_INET;
    hint.sin_port = htons(input_args.selected_port);
    inet_pton(AF_INET, "0.0.0.0", &hint.sin_addr);

    if(bind(serverSocket, (struct sockaddr*)&hint, sizeof(hint)) == -1) {
        fprintf(stderr, "Error occured while binding\n");
        close(serverSocket);
        return -2;
    }
    if(input_args.verbose_init) {
        printf("Bind complete\n");
    }

    
    if(listen(serverSocket, SOMAXCONN) == -1) {
        fprintf(stderr, "Error occuerd while attempting to listen\n");
        close(serverSocket);
        return -3;
    }
    if(input_args.verbose_init) {
        printf("Listening on port: %d\n", input_args.selected_port);
    }


    //sig handling
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, NULL);


    while(run) {

        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int* client_fd = (int *)malloc(sizeof(int));

        if((*client_fd = accept(serverSocket, (struct sockaddr*)&client_addr, &client_addr_len)) < 0) {
            if(errno == EINTR && !run) {
                free(client_fd);
                break;
            }
            fprintf(stderr, "Accept failed.\n");
            continue;
        }

        struct handle_args_struct* handle_args = (struct handle_args_struct*)malloc(sizeof(struct handle_args_struct));
        if(handle_args == NULL) {
            fprintf(stderr, "No memory allocated for handle_args_struct");
            free(client_fd);
            break;
        }

        memcpy(&(handle_args->client_fd), client_fd, sizeof(int));
        memcpy(&(handle_args->verbose_init), &(input_args.verbose_init), sizeof(bool));

        pthread_t thread_id;

        pthread_create(&thread_id, NULL, NAS_handle, (void*)handle_args);
        pthread_detach(thread_id);
        free(client_fd);


    }


    close(serverSocket);
    printf("\nSocket closed\n");

    printf("NAS shutting down.\n");

    return 0;
}

static void sig_handler(int _) {
    (void)_;
    run = false;
}