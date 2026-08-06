#include "NAS.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <regex.h>
#include <errno.h>

void consume_request(char* buffer, size_t* used, char* newline);

void* NAS_handle(void* arg) {

    struct handle_args_struct* args = arg;
    int client_fd = args->client_fd;
    bool verbose = args->verbose_init;

    free(args);

    bool run = true;
    char* buffer = (char*)malloc(sizeof(char) * DEFAULT_BUFFER_SIZE);
    size_t used = 0;
    //wszystko odtąd trzeba wsadzić w while i oddzielać przychodzące wiadomości
    
    while(run) {

        //Typy requestów
        //0 NONE - nierozpoznany request
        //1 lista plików
        //2 get file
        //3 put file
        //4 Test con
        //5 delete file

        //jak zrobić listowanie plików jeśli uznamy że foldery są legalne.

        
        
        ssize_t bytes_received = recv(client_fd, buffer + used, DEFAULT_BUFFER_SIZE - used - 1, 0);
        
        if(bytes_received == 0) {
            //client closed the connection
            break;
        }
        else if(bytes_received < 0) {
            fprintf(stderr, "Recv error");
            break;
        }
        
        
        used += bytes_received;
        buffer[used] = '\0';

        //zakładamy że kolejne komendy z recv są dzielone prze \n
        char* end;
        
        while((end = strstr(buffer, "END\n")) != NULL) {
            *end = '\0'; //ograniczamy gotową komendę
            
            //wewnątrz tego powinna być cała logika procesowania requestów
            char request_type = 0;

            //Czy typ 1
            if(strncasecmp(buffer, "LIST ", 5) == 0) {
                request_type = 1;
            }

            //czy typ 2
            if(strncasecmp(buffer, "GET ", 4) == 0) {
                request_type = 2;
            }
            
            if(strncasecmp(buffer, "PUT ", 4) == 0) {
                request_type = 3;
            }

            //czy typ 4
            if(strncasecmp(buffer, "TEST", 4) == 0) {
                request_type = 4;
            }

            if(strncasecmp(buffer, "DELETE ", 7) == 0) {
                request_type = 5;
            }




            /////////////////////////////////////////////////////////////////////////
            //LIST handle

            //dla zwykłego użytkownika ścieżka jest automatycznie ustawiana na
            // /username/ 
            //zwykły użytkownik powinien móc pracować w swoim folderze widząc tylko / jako początek jego przydzielonej pamięci
            //w przypadku elevated ścieżka początkowa to całe Storage/

            //Stąd wynika że pierwsze co trzeba to zrobić autoryzację i sprawdzić wynik

            if(request_type == 1) {
                regex_t regex;
                regcomp(&regex, "^LIST ([^ ]+) login:([^ ]+) password:([^ \r\n]+)\r?$", REG_EXTENDED);
                regmatch_t matches[4];
                char* temp_buffer = (char*)malloc(sizeof(char) * DEFAULT_BUFFER_SIZE);
                memcpy(temp_buffer, buffer, sizeof(char) * DEFAULT_BUFFER_SIZE);
                if(regexec(&regex, temp_buffer, 4, matches, 0) != 0) {
                    char response[] = "ERROR Malformed request";
                    size_t response_len = strlen(response);
                    size_t total = 0;
                    while(total < response_len) {
                        ssize_t n = send(client_fd, response + total, response_len - total, 0);
                        if(n <= 0) {
                            fprintf(stderr, "0 bytes sent. Closing connection.\n");
                            close(client_fd);
                            free(buffer);
                            free(temp_buffer);
                            return NULL;
                        }
                        total += n;
                    }
                    free(temp_buffer);
                    consume_request(buffer, &used, end);
                    continue;
                }

                char* path = temp_buffer + matches[1].rm_so;
                temp_buffer[matches[1].rm_eo] = '\0';
                
                char* username = temp_buffer + matches[2].rm_so;
                temp_buffer[matches[2].rm_eo] = '\0';

                char* password = temp_buffer + matches[3].rm_so;
                temp_buffer[matches[3].rm_eo] = '\0';

                

                bool elevated = false;

                int auth_status = authenticate(username, password);
                
                if(auth_status == -1) {
                    fprintf(stderr, "Db could not be opened during authentication!\n");
                    close(client_fd);
                    free(temp_buffer);
                    free(buffer);
                    return NULL;
                }
                else if(auth_status == -2) {
                    fprintf(stderr, "sqlite3_prepare failed during authentication!\n");
                    close(client_fd);
                    free(buffer);
                    free(temp_buffer);
                    return NULL;
                }
                else if(auth_status == -3) {
                    fprintf(stderr, "sqlite3_step failed during authentication!\n");
                    close(client_fd);
                    free(buffer);
                    free(temp_buffer);
                    return NULL;
                }
                else if(auth_status == -4 || auth_status == -5) {
                    char response[] = "ERROR Incorrect credentialsEND\n";
                    size_t response_len = strlen(response);
                    size_t total = 0;
                    while(total < response_len) {
                        ssize_t n = send(client_fd, response + total, response_len - total, 0);
                        if(n <= 0) {
                            fprintf(stderr, "0 bytes sent. Closing connection.\n");
                            close(client_fd);
                            free(buffer);
                            free(temp_buffer);
                            return NULL;
                        }
                        total += n;
                    }

                    consume_request(buffer, &used, end);
                    continue;
                }
                else if(auth_status == 1) {
                    elevated = true;
                }

                //najpierw napiszemy przypadek bez elevated (potem tego schematu można użyć w każdym innym requeście)
                if(elevated) {
                    //uprawnienia admina
                    //TODO
                    char* path_buffer = (char*)malloc(sizeof(char) * DEFAULT_BUFFER_SIZE);
                    snprintf(path_buffer, sizeof(char) * DEFAULT_BUFFER_SIZE, "%s%s", STORAGE_PATH, path);

                    char* resolved = (char*)malloc(sizeof(char) * DEFAULT_BUFFER_SIZE);

                    if(realpath(path_buffer, resolved) == NULL) {
                        free(path_buffer);
                        free(resolved);
                        free(temp_buffer);

                        char response[] = "ERROR No such file or directoryEND\n";
                        size_t response_len = strlen(response);
                        size_t total = 0;
                        while(total < response_len) {
                            ssize_t n = send(client_fd, response + total, response_len - total, 0);
                            if(n <= 0) {
                                fprintf(stderr, "0 bytes sent. Closing connection.\n");
                                close(client_fd);
                                free(buffer);
                                return NULL;
                            }
                            total += n;
                        }

                        consume_request(buffer, &used, end);
                        continue;

                    }

                    free(path_buffer);

                    char* check = (char*)malloc(sizeof(char) * DEFAULT_BUFFER_SIZE);
                    snprintf(check, sizeof(char) * DEFAULT_BUFFER_SIZE, "%s", STORAGE_PATH);


                    free(temp_buffer);
                    if(strncasecmp(resolved, check, strlen(check)) != 0) {
                        fprintf(stderr, "Attempting to access forbidden resource!\n");
                        free(check);
                        free(resolved);

                        char response[] = "ERROR Attempting to access forbidden resourceEND\n";
                        size_t response_len = strlen(response);
                        size_t total = 0;
                        while(total < response_len) {
                            ssize_t n = send(client_fd, response + total, response_len - total, 0);
                            if(n <= 0) {
                                fprintf(stderr, "0 bytes sent. Closing connection.\n");
                                close(client_fd);
                                free(buffer);
                                return NULL;
                            }
                            total += n;
                        }

                        consume_request(buffer, &used, end);
                        continue;

                    }

                    free(check);

                    char* ls_command = (char*)malloc(strlen(resolved) + 4);
                    snprintf(ls_command, strlen(resolved) + 4, "ls %s", resolved);
                    free(resolved);

                    FILE* fp = popen(ls_command, "r");
                    if(fp == NULL) {
                        fprintf(stderr, "Error occured in popen: %s\n", strerror(errno));
                        free(ls_command);
                        free(buffer);
                        close(client_fd);
                        return NULL;
                    }

                    char response[4096] = "";
                    char line[256];

                    while(fgets(line, sizeof(line), fp) != NULL) {
                        strncat(response, line, sizeof(response) - strlen(response) - 1);
                    }

                    strncat(response, "END\n", 5);

                    free(ls_command);
                    pclose(fp);

                    size_t response_len = strlen(response);
                    size_t total = 0;
                    while(total < response_len) {
                        ssize_t n = send(client_fd, response + total, response_len - total, 0);
                        if(n <= 0) {
                            fprintf(stderr, "0 bytes sent. Closing connection.\n");
                            close(client_fd);
                            free(buffer);
                        }
                        total += n;
                    }
                    
                    
                }
                else {
                    
                    //tworzymy ścieżkę którą chcemy listować
                    //bierzemy abspath do Storage
                    //bierzemy username
                    //bierzemy ścieżkę podaną w requeście
                    
                    //sklejamy to w jedno i robimy z tego realpath
                    //char* path_buff = (char*)malloc(sizeof(char) * (strlen(STORAGE_PATH) + strlen(username) + strlen(path) + 2));
                    char* path_buffer = (char*)malloc(sizeof(char) * DEFAULT_BUFFER_SIZE);
                    
                    snprintf(path_buffer ,sizeof(char) * DEFAULT_BUFFER_SIZE , "%s/%s%s", STORAGE_PATH, username, path);
                    
                    
                    char* resolved = (char*)malloc(sizeof(char) * DEFAULT_BUFFER_SIZE);
                    

                    if(realpath(path_buffer, resolved) == NULL) {
                        
                        free(path_buffer);
                        free(resolved);
                        free(temp_buffer);

                        char response[] = "ERROR No such file or directoryEND\n";
                        size_t response_len = strlen(response);
                        size_t total = 0;
                        while(total < response_len) {
                            ssize_t n = send(client_fd, response + total, response_len - total, 0);
                            if(n <= 0) {
                                fprintf(stderr, "0 bytes sent. Closing connection.\n");
                                free(buffer);
                                close(client_fd);
                                return NULL;
                            }
                            total += n;
                        }

                        consume_request(buffer, &used, end);
                        continue;   

                    }
                    
                    free(path_buffer);
                    
                    //sprawdzanie czy resolved zaczyna się od /Storage/user
                    char* check = (char*)malloc(sizeof(char) * DEFAULT_BUFFER_SIZE);
                    snprintf(check, sizeof(char) * DEFAULT_BUFFER_SIZE, "%s/%s", STORAGE_PATH, username);
                    free(temp_buffer);
                    
                    if(strncasecmp(resolved, check, strlen(check)) != 0) {
                        fprintf(stderr, "Attempting to access forbidden resource!\n");
                        free(check);
                        free(resolved);

                        char response[] = "ERROR Attempting to acces forbidden resourceEND\n";
                        size_t response_len = strlen(response);
                        size_t total = 0;
                        while(total < response_len) {
                            ssize_t n = send(client_fd, response + total, response_len - total, 0);
                            if(n <= 0) {
                                fprintf(stderr, "0 bytes sent. Closing connection.\n");
                                close(client_fd);
                                free(buffer);
                                return NULL;
                            }
                            total += n;
                        }

                        consume_request(buffer, &used, end);
                        continue;

                    }
                    free(check);


                    char* ls_command = (char*)malloc(strlen(resolved) + 4);
                    snprintf(ls_command, strlen(resolved) + 4, "ls %s", resolved);
                    free(resolved);

                    //wczytujemy wynik ls
                    FILE* fp = popen(ls_command, "r");
                    if(fp == NULL) {
                        fprintf(stderr, "Error occured in popen: %s\n", strerror(errno));
                        free(ls_command);
                        free(buffer);
                        close(client_fd);
                        return NULL;
                    }

                    char response[4096] = "";
                    char line[256];

                    while(fgets(line, sizeof(line), fp) != NULL) {
                        strncat(response, line, sizeof(response) - strlen(response) - 1);
                    }
                    
                    strncat(response, "END\n", 5);

                    free(ls_command);
                    pclose(fp);

                    size_t response_len = strlen(response);
                    size_t total = 0;
                    while(total < response_len) {
                        ssize_t n = send(client_fd, response + total, response_len - total, 0);
                        if(n <= 0) {
                            fprintf(stderr, "0 bytes sent. Closing connection.\n");
                            close(client_fd);
                            free(buffer);
                            return NULL;
                        }
                        total += n;
                    }
                    
                }
                


            }



            ////////////////////////////////////////////////////////////////////////
            //TEST handle
            else if(request_type == 4) {
                char response[] = "ACK";
                size_t response_len = strlen(response);

                size_t total = 0;
                while(total < response_len) {
                    ssize_t n = send(client_fd, response + total, response_len - total, 0);
                    if(n <= 0) {
                        fprintf(stderr, "0 bytes sent. Closing connection.\n");
                        close(client_fd);
                        free(buffer);
                        return NULL;
                    }
                    total += n;
                }

                
            }
            //unknown handle
            else {
                //handle unknown requests
                char response[] = "UNKNOWN REQUEST";
                size_t response_len = strlen(response);

                size_t total = 0;
                while(total < response_len) {
                    ssize_t n = send(client_fd, response + total, response_len - total, 0);
                    if(n <= 0) {
                        fprintf(stderr, "0 bytes sent. Closing connection.\n");
                        close(client_fd);
                        free(buffer);
                        return NULL;
                    }
                    total += n;
                }
            }
        


            //problem jest taki że to poinno się wykonać po każdej sprawdzonej komendzie a chcemy zrobić break
            //możnaby to wrzucić w funkcję i odpalać przed breakiem
            consume_request(buffer, &used, end);
        }
    }


    
    close(client_fd);
    free(buffer);
    return NULL;
}

void consume_request(char* buffer, size_t* used, char* end) {
    size_t consumed = (end - buffer) + 4;
    *used -= consumed;
    memmove(buffer, buffer + consumed, *used);
    buffer[*used] = '\0';
}


