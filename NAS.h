#define NAS_VERSION_MAJOR 0
#define NAS_VERSION_MINOR 4

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifndef NAS_H
#define NAS_H

struct input_args_struct {
    bool verbose_init;
    bool print_help;
    uint16_t selected_port;
};

struct handle_args_struct {
    int client_fd;
    bool verbose_init;
};

struct user_info_struct {
    char* username;
    char* password;
    int flags;
};


#ifdef __cplusplus
extern "C" {
#endif

int handle_arguments(int argc, char** argv, struct input_args_struct* args);

int verify_int(char* int_str);

#ifdef __cplusplus
}
#endif

void* NAS_handle(void* arg);

int test_con();

int authenticate(char* username, char* password);

#define VERBOSE_INIT_DEFAULT false
#define VERBOSE_INPUT_DEFAULT false
#define DEFAULT_PORT 54004
#define DEFAULT_BUFFER_SIZE 104857600
#define HELP_MESSAGE "Help message placeholder\n"
#define STORAGE_PATH "/home/thescorn/science/mine/home_server/NAS/Storage"

#endif
