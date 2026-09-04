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

//{PREFIX}DEL {path/to/file/or/dir} {flag 0/1} login:{login} password:{password} 


/**
 * @brief Function for handling DEL requests
 * 
 * Function releases buffer before returning.
 * Function closes client_fd on loop breaking errors before returning.
 * 
 * @param client_fd int socket descriptor
 * 
 * @param buffer null terminated string with client message
 * 
 * @returns 0 if execution successful, negative values on loop breaking errors,
 * positive on "continue;" errors.
 * 
 */
int DEL_handle(int client_fd, char* buffer) {
    //póki co zakładamy że można usuwać i pliki i dir
    regex_t regex;
    regcomp(&regex, "^DEL ([^ ]+) ([01]) login:([^ ]+) password:([^ \r\n]+\r?$)", REG_EXTENDED);
    regmatch_t matches[5];

    #ifdef DEBUG
    printf("DEBUG mode 0: Function DEL_handle; buffer: %s\n", buffer);
    #endif

    if(regexec(&regex, buffer, 5, matches, 0) != 0) {
        char response[] = "0000000000000017ERROR Malformed request";
        size_t response_len = strlen(response);

        int send_status = send_routine(client_fd, response, response_len);
        if(send_status == -1) {
            fprintf(stderr, "Function PUT_handle; Malformed request error send; 0 bytes sent. Closing connection.\n");
            close(client_fd);
            free(buffer);
            regfree(&regex);
            return -1;
        }

        free(buffer);
        return 1;
    }

    #ifdef DEBUG
    printf("DEBUG mode 0: Function DEL_handle; regex values found.\n");
    #endif

    char* path = buffer + matches[1].rm_so;
    buffer[matches[1].rm_eo] = '\0';


    //w zależności od tej flagi działamy potem na usuwaniu
    char recursive_flag = buffer[matches[2].rm_so];

    char* username = buffer + matches[3].rm_so;
    buffer[matches[3].rm_eo] = '\0';

    char* password = buffer + matches[4].rm_so;
    buffer[matches[4].rm_eo] = '\0';

    #ifdef DEBUG
    printf("DEBUG mode 0.1: Function DEL_handle; Values split on buffer. path: %s, username: %s, password: %s.\n", path, username, password);
    #endif

    regfree(&regex);

    bool elevated = false;
    
    //sprawdzanie czy używamy usuwania z rekursją
    bool recursive;
    if(recursive_flag == '1'){
        recursive = true;
    }
    else{
        recursive = false;
    }

    int auth_status = authenticate(username, password);
    #ifdef DEBUG
    printf("DEBUG mode 0.5: Function DEL_handle;Function authenticate returned with code: %d\n", auth_status);
    #endif
    if(auth_status == -1) {
        fprintf(stderr, "Function DEL_handle; authenticate_size; Db could not be opened during authentication!\n");
        close(client_fd);
        free(buffer);
        return -2;
    }
    else if(auth_status == -2) {
        fprintf(stderr, "Function DEL_handle; authenticate_size; sqlite3_prepare failed during authentication!\n");
        close(client_fd);
        free(buffer);
        return -2;
    }
    else if(auth_status == -3) {
        fprintf(stderr, "Function DEL_handle; authenticate_size; sqlite3_step failed during authentication!\n");
        close(client_fd);
        free(buffer);
        return -2;
    }
    else if(auth_status == -4 || auth_status == -5) {
        char response[] = "000000000000001BERROR Incorrect credentials";
        size_t response_len = strlen(response);
        
        int send_status = send_routine(client_fd, response, response_len);
        if(send_status == -1) {
            fprintf(stderr, "Function DEL_handle; Incorrect credentials error send; 0 bytes sent. Closing connection.\n");
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
    printf("DEBUG mode 1:Function DEL_handle; Authentication successful, elvated: %d\n", (int)elevated);
    #endif

    char* path_buffer = (char*)malloc(sizeof(char) * 4096);

    if(elevated) {
        snprintf(path_buffer, sizeof(char) * 4096, "%s%s", STORAGE_PATH, path);
    }
    else {
        snprintf(path_buffer, sizeof(char) * 4096, "%s/%s%s", STORAGE_PATH, username, path);
    }

    char* resolved = (char*)malloc(sizeof(char) * 4096);

    if(realpath(path_buffer, resolved) == NULL) {
        free(path_buffer);
        free(resolved);
        free(buffer);

        char response[] = "000000000000001FERROR No such directory";
        size_t response_len = strlen(response);

        int send_status = send_routine(client_fd, response, response_len);
        if(send_status == -1) {
            fprintf(stderr, "Function DEL_handle; No such directory error send; 0 bytes sent. Closing connection.\n");
            close(client_fd);
            return -4;
        }

        return 4;

    }

    free(path_buffer);

    #ifdef DEBUG
    printf("DEBUG mode 2:Function DEL_handle; path resolved: %s\n", resolved);
    #endif

    char* check = (char*)malloc(sizeof(char) * 4096);

    if(elevated) {
        snprintf(check, sizeof(char) * 4096, "%s", STORAGE_PATH);
    }
    else {
        snprintf(check, sizeof(char) * 4096, "%s/%s", STORAGE_PATH, username);
    }

    //uwolnienie bufora
    free(buffer);

    if(strncasecmp(resolved, check, strlen(check)) != 0) {
        fprintf(stderr, "Attempting to access forbidden resource!\n");
        free(check);
        free(resolved);

        char response[] = "000000000000002DERROR Attempting to access forbidden resource";
        size_t response_len = strlen(response);
        
        int send_status = send_routine(client_fd, response, response_len);
        if(send_status == -1) {
            fprintf(stderr, "Function DEL_handle; Forbidden resource error send; 0 bytes sent. Closing connection.\n");
            close(client_fd);
            return -5;
        }

        return 5;

    }
    //Rozwiązanie na teraz
    if(equal_paths(resolved, check)) {
        //jeśli resolved prowadzi do ścieżki użytkownika 
        //piszemy że nie wolno usunąć folderu root
        free(resolved);
        free(check);

        char response[] = "0000000000000036ERROR Attemping to remove root directory for this user";
        size_t response_len = strlen(response);

        int send_status = send_routine(client_fd, response, response_len);
        if(send_status == -1) {
            fprintf(stderr, "Function DEL_handle; Deleting root error send; 0 bytes sent. Closing connection.\n");
            close(client_fd);
            return -6;
        }

        return 6;

    }

    free(check);

    #ifdef DEBUG
    printf("DEBUG mode 3:Function DEL_handle; path check 1 passed, check freed.\n");
    #endif

    



    //mamy resolved sprawdzone.
    //sprawdzamy najpierw czy ścieżka prowadzi do pliku czy do dir
    //jeśli do pliku to usuwamy i tyle
    //jeśli do dir to sprawdzamy czy jest pusty

    struct stat st;

    if(stat(resolved, &st) == -1) {
        fprintf(stderr, "Function DEL_handle; stat; Path could not be opened despite beeing resolved.\n");
        free(resolved);
        close(client_fd);
        return -6;
    }
    else if(S_ISREG(st.st_mode)) {
        #ifdef DEBUG
        printf("DEBUG mode 4:Function DEL_handle; ISREG entered.\n");
        #endif

        //pliki usuwamy po prostu
        int remove_status = remove(resolved);
        free(resolved);
        if(remove_status != 0) {
            fprintf(stderr, "Function DEL_handle; error in remove occured. Closing connection.\n");
            close(client_fd);
            return -7;
        }

        if(send_ACK(client_fd) == -1) {
            close(client_fd);
            return -8;
        }

        return 0;
    }
    else if(S_ISDIR(st.st_mode)) {
        #ifdef DEBUG
        printf("DEBUG mode 4:Function DEL_handle; ISDIR entered.\n");
        #endif

        bool empty;

        DIR* dir = opendir(resolved);
        struct dirent* entry;

        int n = 0;
        while((entry = readdir(dir)) != NULL) {
            if(++n > 2);
            break;
        }
        if(n <= 2) {
            empty = true;
        }
        else {
            empty = false;
        }

        //jeśli puste to usuwamy niezależnie od flagi
        if(empty) {
            if(rmdir(resolved) != 0) {
                fprintf(stderr, "Function DEL_handle; rmdir; error occured in rmdir.\n");
                free(resolved);
                close(client_fd);
                return -9;
            }

            free(resolved);

            if(send_ACK(client_fd) == -1) {
                close(client_fd);
                return -10;
            }

            return 0;

        }
        else {
            //folder nie jest pusty
            //1 i nie mamy flagi
            if(!recursive_flag) {
                free(resolved);

                char response[] = "000000000000002EERROR Attempting to remove non-empty directory";
                size_t response_len = strlen(response);

                int send_status = send_routine(client_fd, response, response_len);
                if(send_status == -1) {
                    fprintf(stderr, "Function DEL_handle; Removing non-empty dir error send; 0 bytes sent. Closing connection.\n");
                    close(client_fd);
                    return -11;
                }

                return 11;
            }

            //tutaj logika dla rekursywnego usuwania.
            //bo mamy flagę
            if(remove_all(resolved) < 0) {
                fprintf(stderr, "Function DEL_handle; error occured in remove_all func.\n");
                close(client_fd);
                free(resolved);
                return -12;
            } 

            free(resolved);
            if(send_ACK(client_fd) == -1) {
                close(client_fd);
                return -13;
            }
            return 0;
        }



    }

}

