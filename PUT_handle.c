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

    //instrukcje preprocessora, można wstawić printf tylko jeśli symbol jest zdefiniowany
    #ifdef DEBUG
    printf("DEBUG mode 0:Function PUT_handle; regex values found.\n");
    #endif


    char* path = buffer + matches[1].rm_so;
    buffer[matches[1].rm_eo] = '\0';

    char* username = buffer + matches[2].rm_so;
    buffer[matches[2].rm_eo] = '\0';

    char* password = buffer + matches[3].rm_so;
    buffer[matches[3].rm_eo] = '\0';

    #ifdef DEBUG
    printf("DEBUG mode 0.1: Function PUT_handle; Values split on buffer. path: %s, username: %s, password: %s.\n", path, username, password);
    #endif

    regfree(&regex);

    #ifdef DEBUG
    printf("DEBUG mode 0.2: Function PUT_handle; regex freed.\n");
    #endif

    bool elevated = false;

    int mbytes_max;

    int auth_status = authenticate_size(username, password, &mbytes_max);
    #ifdef DEBUG
    printf("DEBUG mode 0.5: Function PUT_handle; authenticate_size returned with code: %d, mbytes_max: %d\n", auth_status, mbytes_max);
    #endif
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

    #ifdef DEBUG
    printf("DEBUG mode 1:Function PUT_handle; Authentication successful, elvated: %d\n", (int)elevated);
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
            fprintf(stderr, "Function PUT_handle; No such directory error send; 0 bytes sent. Closing connection.\n");
            close(client_fd);
            return -4;
        }

        return 4;

    }

    free(path_buffer);

    #ifdef DEBUG
    printf("DEBUG mode 2:Function PUT_handle; path resolved: %s\n", resolved);
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

    #ifdef DEBUG
    printf("DEBUG mode 3:Function PUT_handle; path check passed.\n");
    #endif

    //check to dokładnie to co jest potrzebne do sprawdzenia zajętego miejsca
    //trzeba napisać funkcję która to ogarnie

    unsigned long long current_space_taken = 0;
    if(!elevated) {
        //current space in bytes
        current_space_taken = dir_size(check);
    }
    
    #ifdef DEBUG
    printf("DEBUG mode 4:Function PUT_handle; current_space_checked: %lld\n", current_space_taken);
    #endif


    free(check);

    struct stat st;

    if(stat(resolved, &st) == -1) {
        fprintf(stderr, "Function PUT_handle; stat; Path could not be opened despite beeing resolved.\n");
        free(resolved);
        close(client_fd);
        return -6;
    }
    else if(S_ISREG(st.st_mode)) {
        #ifdef DEBUG
        printf("DEBUG mode 5:Function PUT_handle; ISREG entered.\n");
        #endif

        free(resolved);
        char response[] = "000000000000002DERROR attempting to save file in another file"; //45 znaków + 0
        size_t response_len = strlen(response);

        int send_status = send_routine(client_fd, response, response_len);
        if(send_status == -1) {
            fprintf(stderr, "Function PUT_handle; saving file in file error; 0 bytes sent. Closing connection.\n");
            close(client_fd);
            return -7;
        }

        #ifdef DEBUG
        printf("DEBUG mode 6:Function PUT_handle; function end.\n");
        #endif
        return 7;
    }
    else if(S_ISDIR(st.st_mode)) {
        
        #ifdef DEBUG
        printf("DEBUG mode 5: Function PUT_handle; ISDIR entered.\n");
        #endif
        
        
        #ifdef DEBUG
        printf("DEBUG mode 5.1:Function PUT_handle; checkpoint before send_ACK.\n");
        #endif
        //wysyłamy ACK żeby klient spodziewał się wiadomości i mógł odebrać errory
        if(send_ACK(client_fd) == -1){
            close(client_fd);
            free(resolved);
            return -10;
        }
        #ifdef DEBUG
        printf("DEBUG mode 5.2:Function PUT_handle; ACK sent.\n");
        #endif

        unsigned long long prefix_numerical;
        int get_prefix_status = get_prefix(client_fd, &prefix_numerical);
        #ifdef DEBUG
        printf("DEBUG mode 5.3: Function PUT_handle; get_prefix returned with prefix numerical: %lld\n", prefix_numerical);
        #endif
        if(get_prefix_status < 0) {
            close(client_fd);
            free(resolved);
            return -8;
        }
        else if(prefix_numerical > SIZE_MAX) {
            fprintf(stderr, "Function PUT_handle; prefix determination; Size of message larger than system size_t. Closing connection\n");
            close(client_fd);
            free(resolved);
            return -10;
        }



        char* metadata_buffer = (char*)malloc(prefix_numerical + (1 * sizeof(char)));

        size_t recieved = 0;
        while(recieved < prefix_numerical) {
            ssize_t n = recv(client_fd, metadata_buffer + recieved, prefix_numerical - recieved, 0);

            if(n == 0) {
                printf("Function PUT_handle; metadata recv; Client closed connection\n");
                free(metadata_buffer);
                free(resolved);
                close(client_fd);
                return -11;
            }
            else if(n < 0) {
                fprintf(stderr, "Function PUT_handle; metadata recv; Recv error\n");
                free(metadata_buffer);
                free(resolved);
                close(client_fd);
                return -12;
            }

            recieved += n;


        }

        *(metadata_buffer + prefix_numerical) = '\0';

        
        #ifdef DEBUG
        printf("DEBUG mode 6:Function PUT_handle; metadata recieved: %s\n", metadata_buffer);
        #endif

        char file_type;
        unsigned long long mtime;
        unsigned long long file_size;
        

        char* mtime_buffer = (char*)malloc(sizeof(char) * 17);
        char* file_size_buffer = (char*)malloc(sizeof(char) * 17);
        char* file_name = (char*)malloc(sizeof(char) * (prefix_numerical - 16 - 16 - 1 + 1));


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

        snprintf(file_name, sizeof(char) * (prefix_numerical - 16 - 16 - 1 + 1), "%s", metadata_buffer + 1 + 16 + 16);

        free(metadata_buffer);

        #ifdef DEBUG
        printf("Function PUT_handle; metadata split. mtime: %s, size: %s, file_name: %s\n", mtime_buffer, file_size_buffer, file_name);
        #endif

        int conversion_status = determine_length(&mtime, mtime_buffer);
        free(mtime_buffer);
        if(conversion_status == -1) {
            free(file_name);
            free(file_size_buffer);
            free(resolved);
            fprintf(stderr, "Function PUT_handle; Determine mtime; Mtime was not a number.\n");
            return -13;
        }
        else if(conversion_status == -2) {
            free(file_name);
            free(file_size_buffer);
            free(resolved);
            fprintf(stderr, "Function PUT_handle; Determine mtime; Value converted does not fit in unsigned long long.\n");
            return -13;
        }
        else if(conversion_status == -3) {
            free(file_name);
            free(file_size_buffer);
            free(resolved);
            fprintf(stderr, "Function PUT_handle; Determine mtime; Mtime contained garbage values.\n");
            return -13;
        }

        conversion_status = determine_length(&file_size, file_size_buffer);
        free(file_size_buffer);
        if(conversion_status == -1) {
            free(file_name);
            free(resolved);
            fprintf(stderr, "Function PUT_handle; Determine file size; File size was not a number.\n");
            return -14;
        }
        else if(conversion_status == -2) {
            free(file_name);
            free(resolved);
            fprintf(stderr, "Function PUT_handle; Determine file size; Value converted does not fit in unsigned long long.\n");
            return -14;
        }
        else if(conversion_status == -3) {
            free(file_name);
            free(resolved);
            fprintf(stderr, "Function PUT_handle; Determine file size; File size contained garbage values.\n");
            return -14;
        }


        //mamy wielkość pliku "file_size" w bajtach
        //mamy obecne zajęte miejsce "current_space_taken" w bajtach
        //mamy pojemność dla użytkownika "mbytes_max" w megabajtach

        if(!elevated && (file_size + current_space_taken > mbytes_max * 1024 * 1024)) {
            //jeśli trzeba wysłać refuse to i tak wszystkiego się pozbywamy i tylko
            //robimy continue, ewentualnie return NULL
            free(file_name);
            free(resolved);
            if(send_REFUSE(client_fd) == -1) {
                return -15;
            }
            return 15;
        }

        if(send_ACCEPT(client_fd) == -1) {
            free(file_name);
            free(resolved);
            return -15;
        }

        #ifdef DEBUG
        printf("DEBUG mode 7:Function PUT_handle; ACCEPT sent.\n");
        #endif

        //Przyjmujemy znowu prefix, sprawdzamy czy jest równy file_size.
        //jeśli nie to zamykamy połączenie
        //jeśli tak to otwieramy w dir nowy plik "file_name"
        //przyjmujemy po 1MB i zapisujemy od razu tam.

        prefix_numerical = 0;
        get_prefix_status = get_prefix(client_fd, &prefix_numerical);
        if(get_prefix_status < 0) {
            free(file_name);
            free(resolved);
            return -8;
        }
        else if(prefix_numerical > SIZE_MAX) {
            fprintf(stderr, "Function PUT_handle; prefix determination; Size of message larger than system size_t. Closing connection\n");
            close(client_fd);
            free(file_name);
            free(resolved);
            return -10;
        }


        if(prefix_numerical != file_size) {
            fprintf(stderr, "Function PUT_handle; compare prefix and file_size Failed. Closing connection.\n");
            free(file_name);
            free(resolved);
            return -16;
        }

        #ifdef DEBUG
        printf("DEBUG mode 8:Function PUT_handle; prefix recieved and compared to file_size.\n");
        #endif

        //bufor do odbioru pliku
        char* buffer = (char*)malloc(DEFAULT_FILE_BLOCK_SIZE);


        recieved = 0;

        char* file_path = (char*)malloc(sizeof(char) * (strlen(resolved) + 1 + strlen(file_name) + 1));
        snprintf(file_path, sizeof(char) * (strlen(resolved) + 1 + strlen(file_name) + 1), "%s/%s", resolved, file_name);

        free(file_name);
        free(resolved);

        FILE* f = fopen(file_path, "wb");

        while(recieved < prefix_numerical) {
            unsigned long long remaining = prefix_numerical - recieved;

            size_t to_recieve = remaining < DEFAULT_FILE_BLOCK_SIZE ? remaining : DEFAULT_FILE_BLOCK_SIZE;

            ssize_t n = recv(client_fd, buffer, to_recieve, 0);

            if(n < 0) {
                fprintf(stderr, "Function PUT_handle; Error occured during file recv\n");
                fclose(f);
                free(buffer);
                free(file_path);
                return -17;
            }
            else if(n == 0) {
                fprintf(stderr, "Function PUT_handle; File recv; Client closed connection.\n");
                fclose(f);
                free(buffer);
                free(file_path);
                return -18;
            }

            fwrite(buffer, 1, n, f);

            recieved += n;

        }

        if(send_ACK(client_fd) == -1) {
            close(client_fd);
            free(file_path);
            free(buffer);
            return -19;
        }


        fclose(f);
        free(buffer);
        free(file_path);
        #ifdef DEBUG
        printf("DEBUG mode function end: Function PUT_handle; f closed, buffer freed, file_path freed, returning 0.\n");
        #endif
        return 0;

    }


}

