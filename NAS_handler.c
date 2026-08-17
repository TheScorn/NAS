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

void* NAS_handle(void* arg) {

    struct handle_args_struct* args = arg;
    int client_fd = args->client_fd;
    bool verbose = args->verbose_init;

    free(args);

    bool run = true;
    char* prefix;
    size_t received;

    enum Request_type_en request_type = UNKNOWN;

    while(run) {
        //Prefix determination
        //odbieramy póki nie mamy 16 znaków
        prefix = (char*)malloc(sizeof(char) * (DEFAULT_PREFIX_SIZE + 1));
        received = 0;
        while(received < DEFAULT_PREFIX_SIZE) {
            ssize_t n = recv(client_fd, prefix + received, DEFAULT_PREFIX_SIZE - received, 0);

            if(n == 0) {
                //klient zamknął połączenie
                free(prefix);
                close(client_fd);
                return NULL;
            }
            else if(n < 0) {
                fprintf(stderr, "Recv error occured in prefix determination!\n");
                free(prefix);
                close(client_fd);
                return NULL;
            }

            received += n;


        }

        *(prefix + DEFAULT_PREFIX_SIZE) = '\0';

        printf("prefix: %s\n", prefix);

        unsigned long long prefix_numerical;
        int conversion_status = determine_length(&prefix_numerical, prefix);
        free(prefix);
        //tu już sam prefix nie jest potrzebny
        if(conversion_status != 0) {
            //error w konwersji prefixu na wartość numeryczną
            close(client_fd);
            return NULL;
        }


        //sprawdzanie czy numerical dał 0
        //to oznacza że przychodzi tylko prefix i klient coś kombinuje 
        //należy raczej zerwać połączenie
        if(prefix_numerical <= 0) {
            fprintf(stderr, "0 bytes stated in prefix. Closing connection.\n");
            close(client_fd);
            return NULL;
        }
        else if(prefix_numerical > SIZE_MAX) {
            fprintf(stderr, "Size of expected message larger than system size_t. Closing connection.\n");
            close(client_fd);
            return NULL;
        }

        //tworzymy bufor wielkość spodziewanej wiadomości
        //+1 char na null terminator
        char* buffer = (char*)malloc(prefix_numerical + (1 * sizeof(char)));



        received = 0;
        while(received < prefix_numerical) {
            ssize_t n = recv(client_fd, buffer + received, prefix_numerical - received, 0);


            if(n == 0) {
                //client closed con
                free(buffer);
                close(client_fd);
                return NULL;
            }
            else if(n < 0) {
                fprintf(stderr, "Recv error in message recv!\n");
                free(buffer);
                close(client_fd);
                return NULL;
            }

            received += n;

        }
        //dodajemy null terminator do wiadomości
        *(buffer + prefix_numerical) = '\0';

        printf("buffer: %s\n", buffer);
        

        if(strncasecmp(buffer, "LIST ", 5) == 0) {
            request_type = LIST;
        }

        //czy typ 2
        else if(strncasecmp(buffer, "GET ", 4) == 0) {
            request_type = GET;
        }
        
        else if(strncasecmp(buffer, "PUT ", 4) == 0) {
            request_type = PUT;
        }

        if(strncasecmp(buffer, "DELETE ", 7) == 0) {
            request_type = DELETE;
        }

        else if(strncasecmp(buffer, "TEST", 4) == 0) {
            request_type = TEST_CON;
        }
        else if(strncasecmp(buffer, "LOGINTEST", 9) == 0) {
            request_type = TEST_LOGIN;
        }

        
        //nie trzeba raczej robić żadnego consume bo robimy recv dla konkretnej ilości znaków


        //LIST handle
        if(request_type == LIST) {
            printf("LIST handle\n");
            regex_t regex;
            regcomp(&regex, "^LIST ([^ ]+) login:([^ ]+) password:([^ \r\n]+)\r?$", REG_EXTENDED);
            regmatch_t matches[4];
            char* temp_buffer = (char*)malloc(4096 * sizeof(char));
            memcpy(temp_buffer, buffer, prefix_numerical + 1 * sizeof(char));
            if(regexec(&regex, temp_buffer, 4, matches, 0) != 0) {
                char response[] = "0000000000000017ERROR Malformed request";
                size_t response_len = strlen(response);
                size_t total = 0;
                while(total < response_len) {
                    ssize_t n = send(client_fd, response + total, response_len - total, 0);
                    if(n <= 0) {
                        fprintf(stderr, "0 bytes sent. Closing connection.\n");
                        close(client_fd);
                        free(buffer);
                        regfree(&regex);
                        free(temp_buffer);
                        return NULL;
                    }
                    total += n;
                }
                free(temp_buffer);
                free(buffer);
                continue;
            }

            char* path = temp_buffer + matches[1].rm_so;
            temp_buffer[matches[1].rm_eo] = '\0';
            
            char* username = temp_buffer + matches[2].rm_so;
            temp_buffer[matches[2].rm_eo] = '\0';

            char* password = temp_buffer + matches[3].rm_so;
            temp_buffer[matches[3].rm_eo] = '\0';

            regfree(&regex);

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
                char response[] = "000000000000001BERROR Incorrect credentials";
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
                free(buffer);
                continue;
            }
            else if(auth_status == 1) {
                elevated = true;
            }

            //najpierw napiszemy przypadek bez elevated (potem tego schematu można użyć w każdym innym requeście)
            
            //uprawnienia admina
            //TODO
            char* path_buffer = (char*)malloc(sizeof(char) * 4096);

            if(elevated) {
                snprintf(path_buffer, sizeof(char) * 4096, "%s%s", STORAGE_PATH, path);
            }   
            else {
                snprintf(path_buffer, sizeof(char) * 4096, "%s/%s%s", STORAGE_PATH, username, path);
            } 
            
            printf("path buffer:%s\n", path_buffer);

            char* resolved = (char*)malloc(sizeof(char) * 4096);

            

            if(realpath(path_buffer, resolved) == NULL) {
                free(path_buffer);
                free(resolved);
                free(temp_buffer);

                char response[] = "000000000000001FERROR No such file or directory";
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

                free(buffer);
                continue;

            }



            free(path_buffer);


            char* check = (char*)malloc(sizeof(char) * 4096);
            
            if(elevated) {
                snprintf(check, sizeof(char) * 4096, "%s", STORAGE_PATH);
            }
            else {
                snprintf(check, sizeof(char) * 4096, "%s/%s", STORAGE_PATH, username);
            }
            
            printf("path_buffer freed, check:%s\n", check);


            free(temp_buffer);
            if(strncasecmp(resolved, check, strlen(check)) != 0) {
                fprintf(stderr, "Attempting to access forbidden resource!\n");
                free(check);
                free(resolved);

                char response[] = "000000000000002DERROR Attempting to access forbidden resource";
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

                free(buffer);
                continue;

            }

            free(check);

            char* ls_command = (char*)malloc(strlen(resolved) + 4);
            snprintf(ls_command, strlen(resolved) + 4, "ls %s", resolved);
            free(resolved);

            printf("check freed, resolved freed. ls_command:%s\n", ls_command);

            FILE* fp = popen(ls_command, "r");
            if(fp == NULL) {
                fprintf(stderr, "Error occured in popen: %s\n", strerror(errno));
                free(ls_command);
                free(buffer);
                close(client_fd);
                return NULL;
            }

            char response_ls[4096] = "";
            char line[256];

            while(fgets(line, sizeof(line), fp) != NULL) {
                strncat(response_ls, line, sizeof(response_ls) - strlen(response_ls) - 1);
            }


            

            free(ls_command);
            pclose(fp);

            printf("ls_command freed, fp closed. response:%s\n", response_ls);

            size_t response_ls_len = strlen(response_ls);

            char* response = (char*)malloc(sizeof(char)*(response_ls_len + 17));
            size_t response_len = response_ls_len + 16;

            snprintf(response, response_len + 1, "%016zX%s", response_ls_len, response_ls);
            printf("prefixed response:%s\nstrlen respnse:%ld\n", response, response_len);

            size_t total = 0;
            while(total < response_len) {
                ssize_t n = send(client_fd, response + total, response_len - total, 0);
                if(n <= 0) {
                    fprintf(stderr, "0 bytes sent. Closing connection.\n");
                    close(client_fd);
                    free(buffer);
                    free(response);
                    return NULL;
                }
                total += n;
            }
            
            free(response);
                
        }

        //GET handle



        //TEST HANDLE
        else if(request_type == TEST_CON) {
            char response[] = "0000000000000003ACK";
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


        //LOGINTEST
        else if(request_type == TEST_LOGIN) {
            regex_t regex;
            regcomp(&regex, "^LOGINTEST login:([^ ]+) password:([^ \r\n]+)\r?$", REG_EXTENDED);
            regmatch_t matches[3];
            if(regexec(&regex, buffer, 3, matches, 0) != 0) {
                char response[] = "0000000000000017ERROR Malformed request";
                size_t response_len = strlen(response);
                size_t total = 0;
                while(total < response_len) {
                    ssize_t n = send(client_fd, response + total, response_len - total, 0);
                    if(n <= 0) {
                        fprintf(stderr, "0 bytes sent. Closing connection.\n");
                        close(client_fd);
                        free(buffer);
                        regfree(&regex);
                        return NULL;
                    }
                    total += n;
                }
                free(buffer);
                continue;
            }

            char* username = buffer + matches[1].rm_so;
            buffer[matches[1].rm_eo] = '\0';

            char* password = buffer + matches[2].rm_so;
            buffer[matches[2].rm_eo] = '\0';

            regfree(&regex);

            int auth_status = authenticate(username, password);
            
            if(auth_status == -1) {
                fprintf(stderr, "Db could not be opened during authentication!\n");
                close(client_fd);
                free(buffer);
                return NULL;
            }
            else if(auth_status == -2) {
                fprintf(stderr, "sqlite3_prepare failed during authentication!\n");
                close(client_fd);
                free(buffer);
                return NULL;
            }
            else if(auth_status == -3) {
                fprintf(stderr, "sqlite3_step failed during authentication!\n");
                close(client_fd);
                free(buffer);
                return NULL;
            }
            else if(auth_status == -4 || auth_status == -5) {
                char response[] = "000000000000000ALOGINFALSE";
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

                free(buffer);
                continue;
            }


            char response[] = "0000000000000009LOGINACK";
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
            char response[] = "000000000000000FUNKNOWN REQUEST";
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
    
        
        free(buffer);



    }

    close(client_fd);
    return NULL;


}