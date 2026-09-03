#define NAS_VERSION_MAJOR 0
#define NAS_VERSION_MINOR 5

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



enum Request_type_en {
    UNKNOWN, LIST, GET, PUT, DELETE, TEST_CON, TEST_LOGIN
};

#ifdef __cplusplus
extern "C" {
#endif

int handle_arguments(int argc, char** argv, struct input_args_struct* args);

int verify_int(char* int_str);

unsigned long long dir_size(char* dirpath);

bool equal_paths(char* path1, char* path2);

#ifdef __cplusplus
}
#endif

void* NAS_handle(void* arg);

int test_con();

int authenticate(char* username, char* password);

int authenticate_size(char* username, char* password, int* mbytes_max);

int determine_length(unsigned long long* result ,char* prefix);

char* add_prefix(char* message);

int last_occurence(char* str, char chr);

int get_prefix(int client_fd, unsigned long long* prefix_numerical);

int send_routine(int client_fd, char* message, size_t message_len);

int send_REFUSE(int sockD);

int send_ACCEPT(int sockD);

int send_ACK(int client_fd);

int PUT_handle(int client_fd, char* buffer);

int DEL_handle(int client_fd, char* buffer);

#define VERBOSE_INIT_DEFAULT false
#define VERBOSE_INPUT_DEFAULT false
#define DEFAULT_PORT 54004
#define DEFAULT_BUFFER_SIZE 104857600
#define DEFAULT_PATH_BUFFER_SIZE 2048
#define DEFAULT_FILE_BLOCK_SIZE 1048576
#define DEFAULT_PREFIX_SIZE 16
#define HELP_MESSAGE "Help message placeholder\n"
#define STORAGE_PATH "/home/thescorn/science/mine/home_server/NAS/Storage"

#endif
