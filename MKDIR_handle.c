
#include "NAS.h"
#include <regex.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

/**
 * @brief Function for handling MKDIR requests
 * 
 * Funtion releases buffer before returning.
 * Function closes client_fd on loop breaking errors before returning.
 * 
 * @param client_fd int describing client connection socket
 * 
 * @param buffer buffer with client message.
 * 
 * @returns 0 if execution successful, negative values on loop braking errors, positive on "continue;" errors.
 *
 * 
 */
int MKDIR_handle(int client_fd, char* buffer) {
    regex_t regex;
    regcomp(&regex, "^MKDIR ([^ ]+) login:([^ ]+) password:([^ \r\n]+\r?$)", REG_EXTENDED);
    regmatch_t matches[4];

    if(regexec(&regex, buffer, 4, matches, 0) != 0) {
        char response[] = "0000000000000017ERROR Malformed request";
        size_t response_len = strlen(response);

        int send_status = send_routine(client_fd, response, response_len);
        if(send_status == -1) {
            fprintf(stderr, "Function MKDIR_handle; Malformed request error send; 0 bytes sent. Closing connection.\n");
            close(client_fd);
            free(buffer);
            regfree(&regex);
            return -1;
        }

        free(buffer);
        return 1;


    }

    #ifdef DEBUG
    printf("DEBUG mode 0:Function MKDIR_handle; regex values found.\n");
    #endif


    char* path = buffer + matches[1].rm_so;
    buffer[matches[1].rm_eo] = '\0';

    char* username = buffer + matches[2].rm_so;
    buffer[matches[2].rm_eo] = '\0';

    char* password = buffer + matches[3].rm_so;
    buffer[matches[3].rm_eo] = '\0';

    #ifdef DEBUG
    printf("DEBUG mode 0.1: Function MKDIR_handle; Values split on buffer. path: %s, username: %s, password: %s.\n", path, username, password);
    #endif

    regfree(&regex);

    #ifdef DEBUG
    printf("DEBUG mode 0.2: Function MKDIR_handle; regex freed.\n");
    #endif


    bool elevated = false;

    int auth_status = authenticate(username, password);
    #ifdef DEBUG
    printf("DEBUG mode 0.5: Function MKDIR_handle; authenticate_size returned with code: %d\n", auth_status);
    #endif
    if(auth_status == -1) {
        fprintf(stderr, "Function MKDIR_handle; authenticate_size; Db could not be opened during authentication!\n");
        close(client_fd);
        free(buffer);
        return -2;
    }
    else if(auth_status == -2) {
        fprintf(stderr, "Function MKDIR_handle; authenticate_size; sqlite3_prepare failed during authentication!\n");
        close(client_fd);
        free(buffer);
        return -2;
    }
    else if(auth_status == -3) {
        fprintf(stderr, "Function MKDIR_handle; authenticate_size; sqlite3_step failed during authentication!\n");
        close(client_fd);
        free(buffer);
        return -2;
    }
    else if(auth_status == -4 || auth_status == -5) {
        char response[] = "000000000000001BERROR Incorrect credentials";
        size_t response_len = strlen(response);
        
        int send_status = send_routine(client_fd, response, response_len);
        if(send_status == -1) {
            fprintf(stderr, "Function MKDIR_handle; Incorrect credentials error send; 0 bytes sent. Closing connection.\n");
            close(client_fd);
            free(buffer);
            return -3;
        }

        free(buffer);
        return 2;   
    }
    else if(auth_status == 1) {
        elevated = true;
    }

    #ifdef DEBUG
    printf("DEBUG mode 1:Function MKDIR_handle; Authentication successful, elvated: %d\n", (int)elevated);
    #endif

    char* path_buffer = (char*)malloc(sizeof(char) * 4096);

    if(elevated) {
        snprintf(path_buffer, sizeof(char) * 4096, "%s%s", STORAGE_PATH, path);
    }
    else {
        snprintf(path_buffer, sizeof(char) * 4096, "%s/%s%s", STORAGE_PATH, username, path);
    }

    #ifdef DEBUG
    printf("DEBUG mode 1.5: Function MKDIR_handle; path_buffer created: %s\n", path_buffer);
    #endif

    //dotąd jest dobrze, resolved może się psuć

    //dzielimy ścieżkę na nazwę folderu i tą do resoved
    int index = last_occurence(path_buffer, '/');
    // /admin/test 11   potrzeba (4 + 1)
    // 0123456
    char* path_to_dir = (char*)malloc(sizeof(char) * (index + 1));
    char* dirname = (char*)malloc(sizeof(char) * (strlen(path_buffer) - index));

    snprintf(path_to_dir, sizeof(char) * (index + 1), "%s", path_buffer);
    snprintf(dirname, sizeof(char) * (strlen(path_buffer) - index), "%s", path_buffer + index + 1);

    free(path_buffer);
    path_buffer = NULL;

    #ifdef DEBUG
    printf("DEBUG mode 1.6: Function MKDIR_handle; path_buffer freed, path: %s, dirname: %s\n", path, dirname);
    #endif


    char* resolved = (char*)malloc(sizeof(char) * 4096);

    if(realpath(path_to_dir, resolved) == NULL) {
        free(path_to_dir);
        free(dirname);
        free(resolved);
        free(buffer);

        #ifdef DEBUG
        printf("DEBUG mode error No such file or dir: Function MKDIR_handle; realpath points to NULL.\n");
        #endif


        char response[] = "0000000000000017ERROR No such directory";
        size_t response_len = strlen(response);

        int send_status = send_routine(client_fd, response, response_len);
        if(send_status == -1) {
            fprintf(stderr, "Function MKDIR_handle; No such directory error send; 0 bytes sent. Closing connection.\n");
            close(client_fd);
            return -4;
        }

        #ifdef DEBUG
        printf("DEBUG mode error no such file or dir: Function MKDIR_handle; ERROR sent, returning 4.\n");
        #endif

        return 4;

    }

    free(path_to_dir);
    path_to_dir = NULL;


    #ifdef DEBUG
    printf("DEBUG mode 2:Function PUT_handle;path_to_dir freed, path resolved: %s\n", resolved);
    #endif

    char* check = (char*)malloc(sizeof(char) * 4096);

    if(elevated) {
        snprintf(check, sizeof(char) * 4096, "%s", STORAGE_PATH);
    }
    else {
        snprintf(check, sizeof(char) * 4096, "%s/%s", STORAGE_PATH, username);
    }

    free(buffer);

    if(strncasecmp(resolved, check, strlen(check)) != 0) {
        fprintf(stderr, "Attempting to access forbidden resource!\n");
        free(check);
        free(resolved);
        free(dirname);

        char response[] = "000000000000002DERROR Attempting to access forbidden resource";
        size_t response_len = strlen(response);
        
        int send_status = send_routine(client_fd, response, response_len);
        if(send_status == -1) {
            fprintf(stderr, "Function MKDIR_handle; Forbidden resource error send; 0 bytes sent. Closing connection.\n");
            close(client_fd);
            return -5;
        }

        return 5;

    }
    free(check);
    #ifdef DEBUG
    printf("DEBUG mode 3:Function MKDIR_handle; path check passed, check freed.\n");
    #endif

    //mkdir samo sprawdza czy folder już istnieje
    //trzeba tylko obsłużyć ten error

    //tutaj trzeba stworzyć zowu całość
    char* mkdir_path = (char*)malloc(sizeof(char) * (strlen(resolved) + 1 + strlen(dirname) + 1));
    snprintf(mkdir_path, sizeof(char) * (strlen(resolved) + 1 + strlen(dirname) + 1), "%s/%s", resolved, dirname);

    free(resolved);
    free(dirname);
    #ifdef DEBUG
    printf("DEBUG mode 4: Function MKDIR_handle; resolved and dirname freed, mkdir_path: %s\n", mkdir_path);
    #endif

    int mkdir_status = mkdir(mkdir_path, 0777);
    free(mkdir_path);
    if(mkdir_status != 0) {
        //obsługujemy tylko kilka errorów, póki co jeden z zajętą ścieżką.
        if(errno == EEXIST) {
            //wysyłamy wiad o powtórzonej ścieżce
            char response[] = "0000000000000032ERROR cannot create directory, file already exists";
            size_t response_len = strlen(response);

            int send_status = send_routine(client_fd, response, response_len);
            if(send_status == -1) {
                fprintf(stderr, "Function MKDIR_handle; Forbidden resource error send; 0 bytes sent. Closing connection.\n");
                close(client_fd);
                return -6;
            }

        }
        else {
            fprintf(stderr, "Function MKDIR_handle; unhandled error occured in mkdir: %s. Closing connection.\n", strerror(errno));
            close(client_fd);
            return -7;
        }
    }



    if(send_ACK(client_fd) == -1) {
        close(client_fd);
        return -8;
    }
    
    return 0;


}