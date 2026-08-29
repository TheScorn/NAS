#include "NAS.h"
#include <regex.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>

/**
 * @brief Function for handling PUT requests
 * 
 * Funtion releases buffer before returning.
 * Function closes client_fd on loop breaking errors before returning.
 *
 * @param client_fd int describing client connection socket
 * 
 * @param buffer buffer with client message.
 * 
 * @returns 0 if execution successful, negative values on loop braking errors, positive on "continue;" errors.
 */
int PUT_handle(int client_fd, char* buffer) {
    regex_t regex;
    regcomp(&regex, "^PUT ([^ ]+) login:([^ ]+) password:([^ \r\n]+\r?$)", REG_EXTENDED);
    regmatch_t matches[4];

    if(regexec(&regex, buffer, 4, matches, 0) != 0) {
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


    char* path = buffer + matches[1].rm_so;
    buffer[matches[1].rm_eo] = '\0';

    char* username = buffer + matches[2].rm_so;
    buffer[matches[2].rm_eo] = '\0';

    char* password = buffer + matches[3].rm_so;
    buffer[matches[3].rm_eo] = '\0';

    regfree(&regex);

    bool elevated = false;

    int* mbytes_max;

    int auth_status = authenticate_size(username, password, mbytes_max);
    if(auth_status == -1) {
        fprintf(stderr, "Function PUT_handle; authenticate_size; Db could not be opened during authentication!\n");
        close(client_fd);
        free(buffer);
        return -2;
    }
    else if(auth_status == -2) {
        fprintf(stderr, "Function PUT_handle; authenticate_size; sqlite3_prepare failed during authentication!\n");
        close(client_fd);
        free(buffer);
        return -2;
    }
    else if(auth_status == -3) {
        fprintf(stderr, "Function PUT_handle; authenticate_size; sqlite3_step failed during authentication!\n");
        close(client_fd);
        free(buffer);
        return -2;
    }
    else if(auth_status == -4 || auth_status == -5) {
        char response[] = "000000000000001BERROR Incorrect credentials";
        size_t response_len = strlen(response);
        
        int send_status = send_routine(client_fd, response, response_len);
        if(send_status == -1) {
            fprintf(stderr, "Function PUT_handle; Incorrect credentials error send; 0 bytes sent. Closing connection.\n");
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
            fprintf(stderr, "Function PUT_handle; No such directory error send; 0 bytes sent. Closing connection.\n");
            close(client_fd);
            return -4;
        }

        return 4;

    }

    free(path_buffer);

    



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

        char response[] = "000000000000002DERROR Attempting to access forbidden resource";
        size_t response_len = strlen(response);
        
        int send_status = send_routine(client_fd, response, response_len);
        if(send_status == -1) {
            fprintf(stderr, "Function PUT_handle; Forbidden resource error send; 0 bytes sent. Closing connection.\n");
            close(client_fd);
            return -5;
        }

        return 5;

    }

    //check to dokładnie to co jest potrzebne do sprawdzenia zajętego miejsca
    //trzeba napisać funkcję która to ogarnie



    free(check);

    struct stat st;

    if(stat(resolved, &st) == -1) {
        fprintf(stderr, "Function PUT_handle; stat; Path could not be opened despite beeing resolved.\n");
        free(resolved);
        close(client_fd);
        return -6;
    }
    else if(S_ISREG(st.st_mode)) {
        free(resolved);
        char response[] = "000000000000002DERROR attempting to save file in another file"; //45 znaków + 0
        size_t response_len = strlen(response);

        int send_status = send_routine(client_fd, response, response_len);
        if(send_status == -1) {
            fprintf(stderr, "Function PUT_handle; saving file in file error; 0 bytes sent. Closing connection.\n");
            close(client_fd);
            return -7;
        }

        return 7;
    }
    else if(S_ISDIR(st.st_mode)) {
        //wiemy że ścieżka jest u użytkownika i prowadzi do folderu
        //trzeba odebrać metadane i zebrać z nich info

        //otworzyć potem folder użytkownika i porównać wielkości.

        unsigned long long prefix_numerical;
        int get_prefix_status = get_prefix(client_fd, &prefix_numerical);
        if(get_prefix_status < 0) {
            return -8;
        }
        if(prefix_numerical != 33) {
            fprintf(stderr, "Function PUT_handle; prefix determination; Expected 33 bytes in metadata message, prefix stated: %lld\n", prefix_numerical);
            close(client_fd);
            return -9;
        }
        else if(prefix_numerical > SIZE_MAX) {
            fprintf(stderr, "Function PUT_handle; prefix determination; Size of message larger than system size_t. Closing connection\n");
            close(client_fd);
            return -10;
        }

        char* metadata_buffer = (char*)malloc(prefix_numerical + (1 * sizeof(char)));

        size_t recieved = 0;
        while(recieved < prefix_numerical) {
            ssize_t n = recv(client_fd, buffer + recieved, prefix_numerical - recieved, 0);

            if(n == 0) {
                printf("Function PUT_handle; metadata recv; Client closed connection\n");
                free(metadata_buffer);
                close(client_fd);
                return -11;
            }
            else if(n < 0) {
                fprintf(stderr, "Function PUT_handle; metadata recv; Recv error\n");
                free(metadata_buffer);
                close(client_fd);
                return -12;
            }

            recieved += n;


        }

        *(metadata_buffer + prefix_numerical) = '\0';

        char file_type;
        unsigned long long mtime;
        unsigned long long file_size;

        char* mtime_buffer = (char*)malloc(sizeof(char) * 17);
        char* file_size_buffer = (char*)malloc(sizeof(char) * 17);

        if(*metadata_buffer == '0') {
            file_type = 0;
        }
        else if(*metadata_buffer == '1') {
            file_type = 1;
        }
        else {
            file_type = 2;
        }

        snprintf(mtime_buffer, 17, "%s", metadata_buffer + 1);

        snprintf(file_size_buffer, 17, "%s", metadata_buffer + 17);

        free(metadata_buffer);

        int conversion_status = determine_length(&mtime, mtime_buffer);
        free(mtime_buffer);
        if(conversion_status == -1) {
            fprintf(stderr, "Function PUT_handle; Determine mtime; Mtime was not a number.\n");
            return -13;
        }
        else if(conversion_status == -2) {
            fprintf(stderr, "Function PUT_handle; Determine mtime; Value converted does not fit in unsigned long long.\n");
            return -13;
        }
        else if(conversion_status == -3) {
            fprintf(stderr, "Function PUT_handle; Determine mtime; Mtime contained garbage values.\n");
            return -13;
        }

        conversion_status = determine_length(file_size, file_size_buffer);
        free(file_size_buffer);
        if(conversion_status == -1) {
            fprintf(stderr, "Function PUT_handle; Determine file size; File size was not a number.\n");
            return -14;
        }
        else if(conversion_status == -2) {
            fprintf(stderr, "Function PUT_handle; Determine file size; Value converted does not fit in unsigned long long.\n");
            return -14;
        }
        else if(conversion_status == -3) {
            fprintf(stderr, "Function PUT_handle; Determine file size; File size contained garbage values.\n");
            return -14;
        }


        //mamy wielkość max dla użytownika w MB
        //mamy przewidywaną wielkość w B

        //jeśli nie jest elevated to trzeba otworzyć folder root i sprawdzić ile się mieści

        if(!elevated) {
            //otwieramy DEFAULT_STORAGE + username i sprawdzamy wielkość folderu.
            
        }



    }


}

