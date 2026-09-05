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
#include <dirent.h>
#include <fcntl.h>

void* NAS_handle(void* arg) {

    struct handle_args_struct* args = arg;
    int client_fd = args->client_fd;
    bool verbose = args->verbose_init;


    free(args);

    bool run = true;
    unsigned long long prefix_numerical;
    size_t received;



    enum Request_type_en request_type = UNKNOWN;

    while(run) {
        //Prefix determination
        //odbieramy póki nie mamy 16 znaków
        int get_prefix_status = get_prefix(client_fd, &prefix_numerical);

        if(get_prefix_status < 0) {
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

        //printf("buffer: %s\n", buffer);
        

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

        else if(strncasecmp(buffer, "DEL ", 4) == 0) {
            request_type = DELETE;
        }

        else if(strncasecmp(buffer, "MKDIR", 5) == 0) {
            request_type = MKDIR;
        }

        else if(strncasecmp(buffer, "TEST", 4) == 0) {
            request_type = TEST_CON;
        }
        else if(strncasecmp(buffer, "LOGINTEST", 9) == 0) {
            request_type = TEST_LOGIN;
        }

        
        //nie trzeba raczej robić żadnego consume bo robimy recv dla konkretnej ilości znaków

        ///////////////////////////////////////////////////////
        //LIST handle
        ////////////
        //TODO
        //Zmienić przesył listy i dodać od razu do info o danym pliku czas ostatniej modyfikacji
        //oraz wielkość tak żeby można było wyświetlić te dane bez przesyłania samego pliku


        if(request_type == LIST) {

            regex_t regex;
            regcomp(&regex, "^LIST ([^ ]+) login:([^ ]+) password:([^ \r\n]+)\r?$", REG_EXTENDED);
            regmatch_t matches[4];
            char* temp_buffer = (char*)malloc(4096 * sizeof(char));
            memcpy(temp_buffer, buffer, prefix_numerical + 1 * sizeof(char));
            free(buffer);
            if(regexec(&regex, temp_buffer, 4, matches, 0) != 0) {
                char response[] = "0000000000000017ERROR Malformed request";
                size_t response_len = strlen(response);
                
                int send_status = send_routine(client_fd, response, response_len);
                if(send_status == -1) {
                    fprintf(stderr, "0 bytes sent. Closing connection.\n");
                    close(client_fd);
                    regfree(&regex);
                    free(temp_buffer);
                    return NULL;
                }
                
                
                free(temp_buffer);
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
                return NULL;
            }
            else if(auth_status == -2) {
                fprintf(stderr, "sqlite3_prepare failed during authentication!\n");
                close(client_fd);
                free(temp_buffer);
                return NULL;
            }
            else if(auth_status == -3) {
                fprintf(stderr, "sqlite3_step failed during authentication!\n");
                close(client_fd);
                free(temp_buffer);
                return NULL;
            }
            else if(auth_status == -4 || auth_status == -5) {
                char response[] = "000000000000001BERROR Incorrect credentials";
                size_t response_len = strlen(response);
                
                int send_status = send_routine(client_fd, response, response_len);
                if(send_status == -1) {
                    fprintf(stderr, "0 bytes sent. Closing connection.\n");
                    close(client_fd);
                    free(temp_buffer);
                    return NULL;
                }

                free(temp_buffer);
                continue;
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
                free(temp_buffer);

                char response[] = "000000000000001FERROR No such file or directory";
                size_t response_len = strlen(response);
                
                int send_status = send_routine(client_fd, response, response_len);
                if(send_status == -1) {
                    fprintf(stderr, "0 bytes sent. Closing connection.\n");
                    close(client_fd);
                    return NULL;
                }

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
            
            


            free(temp_buffer);
            if(strncasecmp(resolved, check, strlen(check)) != 0) {
                fprintf(stderr, "Attempting to access forbidden resource!\n");
                free(check);
                free(resolved);

                char response[] = "000000000000002DERROR Attempting to access forbidden resource";
                size_t response_len = strlen(response);
                
                int send_status = send_routine(client_fd, response, response_len);
                if(send_status == -1) {
                    fprintf(stderr, "0 bytes sent. Closing connection.\n");
                    close(client_fd);
                    return NULL;
                }
                

                continue;

            }

            free(check);
            
            DIR* dir = opendir(resolved);
            bool file = false;
            
            if(dir == NULL) {
                if(errno == ENOTDIR) {
                    //ścieżka wskazuje do pliku
                    file = true;
                }
                else {
                    fprintf(stderr, "Error occured in opendir: %s", strerror(errno));
                    close(client_fd);
                    free(resolved);
                    return NULL;
                }
            }

            //0 file, 1 dir, 2 symlink
            //jeśli odpowiedzią jest pojedyńczy plik to wysyłamy po prostu tą jedną nazwę
            //też korzystamy ze schematu [type][namelength][name] dla stałości
            //type {0, 1, 2}, size(16 x hex), last_mod(16 x hex), namelength 4 bytes in hex, name
            char* raw_response;
            if(file) {

                struct stat st;
                if(stat(resolved, &st) == -1) {
                    //ścieżki nie ma mimo sprawdzenia wczesniej
                    fprintf(stderr, "Path could not be opened by stat despite beeing resolved.\n");
                    free(resolved);
                    close(client_fd);
                    return NULL;
                }

                unsigned long long file_size = (unsigned long long)st.st_size;
                unsigned long long last_mod = (unsigned long long)st.st_mtime;


                //bierzemy całą ścieżkę
                //musimy jakoś znaleźć ostatni /
                int last_dash = last_occurence(resolved, '/');
                resolved = resolved + last_dash + 1;

                size_t name_length = strlen(resolved);

                raw_response = (char*)malloc(sizeof(char) * (1 + 16 + 16 + 4 + name_length + 1));
                snprintf(raw_response, 1 + 16 + 16 + 4 + name_length + 1, "0%016llX%016llX%04zX%s", file_size, last_mod,name_length ,resolved);
                free(resolved);
                resolved = NULL;

                //reszta potem bo prefix można dodać już później
            }
            else {
                struct dirent *entry;
                struct stat st;


                size_t name_length;
                char type;
                raw_response = (char*)malloc(sizeof(char));
                *raw_response = '\0';

                char* temp;
               
                //śmieciowy alloc żeby móc robić realloc w pętli
                char* element;

                while((entry = readdir(dir)) != NULL) {
                    

                    if(fstatat(dirfd(dir), entry->d_name, &st, 0) == -1) {
                        fprintf(stderr, "Error occured on fstatat. Closing connection.\n");
                        free(raw_response);
                        free(resolved);
                        close(client_fd);
                        return NULL;
                    }

                    //pomijamy . i ..
                    if(strcasecmp(entry->d_name, ".") == 0 || strcasecmp(entry->d_name, "..") == 0) {
                        continue;
                    }
                    
                    //odczytujemy każdy element i dopisujemy coraz więcej do raw response
                    //[type][namelength][name]  
                    if(entry->d_type == 8) {
                        type = '0';
                    }
                    else if(entry->d_type == 4) {
                        type = '1';
                    }
                    else if(entry->d_type == 10) {
                        type = '2';
                    }
                    else if(entry->d_type == 0) {
                        //nieznany plik. trzeba użyć lstat
                        //może na potem bo potrzeba ścieżki
                    }

                    
                    //tworzymy filesize
                    unsigned long long file_size = (unsigned long long)st.st_size;


                    //tworzymy last_mod
                    unsigned long long last_mod = (unsigned long long)st.st_mtime;


                    name_length = strlen(entry->d_name);

                    element = (char*)malloc(sizeof(char) * (1 + 16 + 16 + 4 + name_length + 1));
                    snprintf(element, 1 + 16 + 16 + 4 + name_length + 1, "%c%016llX%016llX%04zX%s", type, file_size, last_mod ,name_length, entry->d_name);
                    
                    //raw_response = (char*)realloc(raw_response, sizeof(char) * (strlen(raw_response)));
                    
                    temp = (char*)malloc(sizeof(char) * (strlen(raw_response) + strlen(element) + 1));
                    

                    snprintf(temp, strlen(raw_response) + strlen(element) + 1, "%s%s", raw_response, element);
                    raw_response = (char*)realloc(raw_response, sizeof(char) * (strlen(temp) + 1));
                    snprintf(raw_response, strlen(temp) + 1, "%s", temp);

                    free(temp);
                    free(element);

                }
                free(resolved);
                resolved = NULL;
                closedir(dir);
            }
            
            //mamy raw_response gotowe
            size_t raw_response_len = strlen(raw_response);
            char* response = (char*)malloc(sizeof(char) * (raw_response_len + 16 + 1));
            size_t response_len = raw_response_len + 16;

            snprintf(response, response_len + 1, "%016zX%s", raw_response_len, raw_response);
            free(raw_response);

            int send_status = send_routine(client_fd, response, response_len);
            if(send_status == -1) {
                fprintf(stderr, "0 bytes sent. Closing connection");
                free(response);
                close(client_fd);
                return NULL;
            }

            free(response);
        }





        /////////////////////////////////////////////////////////////////////
        //GET handle
        else if(request_type == GET) {
            
            //sprawdzamy credentiale
            //sprawdzamy typ pliku
            //na folderze zatrzymujemy
            //wczytujemy plik i jego stat
            //wysyłamy stat z prefixem
            //wysyłamy prefix pliku
            //streamujemy plik w blokach
            
            regex_t regex;
            regcomp(&regex, "^GET ([^ ]+) login:([^ ]+) password:([^ \r\n]+)\r?$", REG_EXTENDED);
            regmatch_t matches[4];
            
            if(regexec(&regex, buffer, 4, matches, 0) != 0) {
                char response[] = "0000000000000017ERROR Malformed request";
                size_t response_len = strlen(response);
                
                int send_status = send_routine(client_fd, response, response_len);
                if(send_status == -1) {
                    fprintf(stderr, "0 bytes sent. Closing connection.\n");
                    close(client_fd);
                    free(buffer);
                    regfree(&regex);
                    return NULL;
                }
                
                free(buffer);
                continue;
            }


            char* path = buffer + matches[1].rm_so;
            buffer[matches[1].rm_eo] = '\0';
            
            char* username = buffer + matches[2].rm_so;
            buffer[matches[2].rm_eo] = '\0';

            char* password = buffer + matches[3].rm_so;
            buffer[matches[3].rm_eo] = '\0';

            regfree(&regex);

            bool elevated = false;

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
                char response[] = "000000000000001BERROR Incorrect credentials";
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

                char response[] = "000000000000001FERROR No such file or directory";
                size_t response_len = strlen(response);
                
                int send_status = send_routine(client_fd, response, response_len);
                if(send_status == -1) {
                    fprintf(stderr, "0 bytes sent. Closing connection.\n");
                    close(client_fd);
                    return NULL;
                }

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

            free(buffer);

            if(strncasecmp(resolved, check, strlen(check)) != 0) {
                fprintf(stderr, "Attempting to access forbidden resource!\n");
                free(check);
                free(resolved);

                char response[] = "000000000000002DERROR Attempting to access forbidden resource";
                size_t response_len = strlen(response);
                
                int send_status = send_routine(client_fd, response, response_len);
                if(send_status == -1) {
                    fprintf(stderr, "0 bytes sent. Closing connection.\n");
                    close(client_fd);
                    return NULL;
                }

                continue;

            }

            free(check);

            //na tym etapie wiemy że ścieżka jest poprawna i użytkownik ma prawa
            //trzeba sprawdzić czy to folder czy plik i póki co foldery odrzucać
            struct stat st;

            if(stat(resolved, &st) == -1) {
                //ścieżki nie ma mimo sprawdzenia wczesniej
                fprintf(stderr, "Path could not be opened by stat despite beeing resolved.\n");
                free(resolved);
                close(client_fd);
                return NULL;
            }
            //przesył folderów
            else if(S_ISDIR(st.st_mode)) {
                //chyba rozwiążemy go zwyczajnym zipem
                //będzie prościej i chyba szybciej
                free(resolved);
                char response[] = "000000000000002FERROR sending directories is not yet supported.";
                size_t response_len = strlen(response);
                
                int send_status = send_routine(client_fd, response, response_len);
                if(send_status == -1) {
                    fprintf(stderr, "0 bytes sent. Closing connection.\n");
                    close(client_fd);
                    return NULL;
                }
                
                continue;
            }

            //przesył pliku
            else if(S_ISREG(st.st_mode)) {
                //tworzymy i przesyłamy metadane w formie jsona
                //1 nazwa (pytanie czy potrzebna bo użytkownik i tak musi ją znać przed wysłaniem requesta)
                //lepiej wysłać bo przy przesyle folderu potem może się przydać
                //2 typ, też trzeba by przesłać dla unifikacji działania tu i w przesyle folderów
                //3 czas ostatniej modyfikacji (16 x hex)
                //4 wielkość pliku (16 x hex)
                char type = '0'; //plik
                
                unsigned long long last_mod = (unsigned long long)st.st_mtime;
                unsigned long long file_size = (unsigned long long)st.st_size;

                char* message_raw = (char*)malloc(sizeof(char) * (1 + 16 + 16 + 1));
                snprintf(message_raw, 1 + 16 + 16 + 1, "%c%016llX%016llX", type, last_mod, file_size);
                size_t raw_message_len = strlen(message_raw);
                
                size_t message_len = raw_message_len + 16;
                char* message = (char*)malloc(sizeof(char) * (raw_message_len + 16 + 1));
                snprintf(message, message_len + 1, "%016zX%s", raw_message_len, message_raw);
                //opakowujemy w prefix
                free(message_raw);
                message_raw = NULL;
                
                int send_status = send_routine(client_fd, message, message_len);
                if(send_status == -1) {
                    fprintf(stderr, "0 bytes sent. Closing connection.\n");
                    close(client_fd);
                    free(message);
                    free(resolved);
                    return NULL;
                }

                

                free(message);
                //host po odczytaniu metadanych powinien potwierdzic za pomocą
                //ACCEPT
                //jeśli otrzymamy REFUSE to kończymy tą iterację i nie robimy nic więcej
                //oba mają taką samą długość
                buffer = (char*)malloc(sizeof(char) * (16 + 6 + 1));
                
                size_t total = 0;
                while(total < 16 + 6) {
                    ssize_t n = recv(client_fd, buffer + total, 16 + 6 - total, 0);
                    
                    if(n == 0) {
                        fprintf(stderr, "Functionality GET, recv ACCEPT/REFUSE buffer. Client closed connection.\n");
                        free(buffer);
                        free(resolved);
                        close(client_fd);
                        return NULL;
                    }
                    else if(n < 0) {
                        fprintf(stderr, "Functionality GET, recv ACCEPT/REFUSE buffer. recv error occured!\n");
                        free(buffer);
                        free(resolved);
                        close(client_fd);
                        return NULL;
                    }

                    total += n;
                }

                *(buffer + 16 + 6) = '\0';

                

                if(strcasecmp(buffer, "0000000000000006REFUSE") == 0) {
                    //client wysłał request ale odmówił przyjecia pliku
                    free(buffer);
                    free(resolved);
                    continue;
                }
                else if(strcasecmp(buffer, "0000000000000006ACCEPT") != 0) {
                    free(buffer);
                    free(resolved);
                    fprintf(stderr, "Unexpected message from client. Closing connection.\n");
                    close(client_fd);
                    return NULL;
                }

                //klient zaakceptował
                free(buffer);
                buffer = NULL;

                //wysyłamy prefix
                char* prefix = (char*)malloc(sizeof(char) * 17);
                snprintf(prefix, 17, "%016llX", file_size);

                send_status = send_routine(client_fd, prefix, 16);
                if(send_status == -1) {
                    fprintf(stderr, "0 bytes sent. Closing connection.\n");
                    free(prefix);
                    free(resolved);
                    close(client_fd);
                    return NULL;
                }
                free(prefix);
                prefix = NULL;

                //otwieramy plik jako bytes
                FILE *file = fopen(resolved, "rb");                
                if(file == NULL) {
                    fprintf(stderr, "File could not be opened despite beeing resolved.\n");
                    free(resolved);
                    close(client_fd);
                    return NULL;
                }
                
                free(resolved);
                resolved = NULL;

                unsigned char* partial_buffer = (unsigned char*)malloc(DEFAULT_FILE_BLOCK_SIZE);

                size_t bytes_read;

                while((bytes_read = fread(partial_buffer, 1, DEFAULT_BUFFER_SIZE, file)) > 0) {
                    //czytamy do bufora tyle ile jest w pliku
                    size_t bytes_sent = 0;

                    while(bytes_sent < bytes_read) {
                        ssize_t n = send(client_fd, partial_buffer + bytes_sent, bytes_read - bytes_sent, 0);
                        if(n <= 0) {
                            fprintf(stderr, "0 bytes sent. Closing connection.\n");
                            free(partial_buffer);
                            close(client_fd);
                            fclose(file);
                            return NULL;
                        }
                        bytes_sent += n;

                    }


                }

                fclose(file);
                //wczytujemy część pliku
                //wysyłamy
                //powtarzamy póki total < sent.
                free(partial_buffer);

            }

        }
        

        /////////////////////////////////////////////////////////////
        //PUT handle
        else if(request_type == PUT) {
            int put_status = PUT_handle(client_fd, buffer);
            if(put_status < 0) {
                return NULL;
            }
        }

        //DELETE handle
        else if(request_type == DELETE) {
            int delete_status = DEL_handle(client_fd, buffer);
            if(delete_status < 0) {
                return NULL;
            }
        }

        //MKDIR handle
        else if(request_type == MKDIR) {
            int mkdir_status = MKDIR_handle(client_fd, buffer);
            if(mkdir_status < 0) {
                return NULL;
            }
        }

        //TEST HANDLE
        else if(request_type == TEST_CON) {
            char response[] = "0000000000000003ACK";
            size_t response_len = strlen(response);

            int send_status = send_routine(client_fd, response, response_len);
            if(send_status == -1) {
                fprintf(stderr, "0 bytes sent. Closing connection.\n");
                close(client_fd);
                free(buffer);
                return NULL;
            }


            free(buffer);
            
        }


        //LOGINTEST
        else if(request_type == TEST_LOGIN) {
            regex_t regex;
            regcomp(&regex, "^LOGINTEST login:([^ ]+) password:([^ \r\n]+)\r?$", REG_EXTENDED);
            regmatch_t matches[3];
            if(regexec(&regex, buffer, 3, matches, 0) != 0) {
                char response[] = "0000000000000017ERROR Malformed request";
                size_t response_len = strlen(response);
                
                int send_status = send_routine(client_fd, response, response_len);
                if(send_status == -1) {
                    fprintf(stderr, "0 bytes sent. Closing connection.\n");
                    close(client_fd);
                    free(buffer);
                    regfree(&regex);
                    return NULL;
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
                
                int send_status = send_routine(client_fd, response, response_len);
                if(send_status == -1) {
                   fprintf(stderr, "0 bytes sent. Closing connection.\n");
                    close(client_fd);
                    free(buffer);
                    return NULL; 
                }

                free(buffer);
                continue;
            }


            char response[] = "0000000000000009LOGINACK";
            size_t response_len = strlen(response);
            
            int send_status = send_routine(client_fd, response, response_len);
            if(send_status == -1) {
                fprintf(stderr, "0 bytes sent. Closing connection.\n");
                close(client_fd);
                free(buffer);
                return NULL;
            }

            free(buffer);

        }



            //unknown handle
        else {
            //handle unknown requests
            char response[] = "000000000000000FUNKNOWN REQUEST";
            size_t response_len = strlen(response);

            int send_status = send_routine(client_fd, response, response_len);
            if(send_status == -1) {
                fprintf(stderr, "0 bytes sent. Closing connection.\n");
                close(client_fd);
                free(buffer);
                return NULL;
            }

            free(buffer);

        }
    
        
        



    }

    close(client_fd);
    return NULL;


}