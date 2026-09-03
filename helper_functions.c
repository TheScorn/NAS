#include "NAS.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

int determine_length(unsigned long long* result ,char* prefix) {
    errno = 0;
    char* end;
    *result = strtoull(prefix, &end, 16);

    if(*result == 0 && end == prefix) {
        //string was not a number
        return -1;
    }   
    else if(*result == ULLONG_MAX && errno) {
        //the value does not fit in unsigned long long
        return -2;
    }
    else if(*end) {
        //str began with a number but has junk left over at the end
        return -3;
    }


    return 0;
}

/**
 * @brief Function for adding lenght prefix to message
 * 
 * 
 * @param message null terminated string representing message
 * 
 * @returns null terminated string with 16 characters added to the front representing prefixed messsage
 * 
 */
char* add_prefix(char* message) {
    size_t message_len = strlen(message);

    char* prefixed = (char*)malloc((message_len + 1 + 16) * sizeof(char));

    snprintf(prefixed, message_len + 1 + 16, "%016zX%s", message_len, message);

    return prefixed;
}


/**
 * @brief Function for finding last occurence of a char in string
 * 
 * 
 * @param str null terminated string
 * 
 * @param chr character to be found
 * 
 * @returns index of a last occurence of a string. -1 If no match found
 */
int last_occurence(char* str, char chr) {
    
    int last_occur = -1;
    //string strlen 6 
    for(int idx = 0; idx < strlen(str); idx++) {
        if(*(str + idx) == chr) {
            last_occur = idx;

        }

    }

    return last_occur;

}


int dir_size_aux(char* dirpath, unsigned long long* total) {
    //otwieramy dir
    //przechodzimy po elementach
    //dla każdego pliku dodajemy size do total
    //dla każdego folderu musimy dokleić ścieżkę i odpalić tą funkcję znowu
    DIR* dir = opendir(dirpath);
    //mamy raczej zapewnione że dirpath jest folderem bo będzie wskazywał na usr

    struct dirent* entry;
    struct stat st;
    char* new_path;
    while((entry = readdir(dir)) != NULL) {
        if(fstatat(dirfd(dir), entry->d_name, &st, 0) == -1) {
            fprintf(stderr, "Function dir_size_aux; fstatat; Error.\n");
            return -1;
        }

        if(strcasecmp(entry->d_name, ".") == 0 || strcasecmp(entry->d_name, "..") == 0) {
            continue;
        }

        if(entry->d_type == 8) { //file
            *total += st.st_size;
        }
        else if(entry->d_type == 4) { //dir
            //tworzymy nową ścieżkę
            //old/path/new_dir\0
            new_path = (char*)malloc(sizeof(char) * (strlen(dirpath) + 1 + strlen(entry->d_name) + 1));
            snprintf(new_path, sizeof(char) * (strlen(dirpath) + 1 + strlen(entry->d_name) + 1), "%s/%s", dirpath, entry->d_name);
            dir_size_aux(new_path, total);
            free(new_path);

        }
        else {
            //póki co nie obsługujemy specjalnych sprawdzań
            fprintf(stderr, "Function dir_size_aux; Unknown element type.\n");
            return -2;
        }

    }


    return 0;
}

/**
 * @brief Get dir size in bytes
 * 
 * Whole logic depends on recursive function dir_size_aux.
 * For more details check dir_size_aux
 * 
 * @param dirpath null terminated string. Path to root directory.
 * 
 * @returns unsigned long long number of bytes this dir uses.
 */
unsigned long long dir_size(char* dirpath) {
    unsigned long long total = 0;
    dir_size_aux(dirpath, &total);
    return total;
}

/**
 * @brief Function for checking path equality
 * 
 * @param path1 null terminated string
 * 
 * @param path2 null terminated string
 * 
 * @returns true if strings point to the same file or dir, false if not
 * 
 */
bool equal_paths(char* path1, char* path2) {
    struct stat st1;
    struct stat st2;

    if(stat(path1, &st1) != 0) {
        return false;
    }

    if(stat(path2, &st2) != 0) {
        return false;
    }

    return((st1.st_dev == st2.st_dev) && (st1.st_ino == st2. st_ino));
}



/**
 * @brief function for removing directory with all its contents
 * 
 * Function recursivly descends into file tree. Removes all files and steps into dirs with added path.
 * After returning to dir with all elemtens inside it removed it removes the dir.
 * 
 * @param path null-terminated string with path to the start dir. Funtion assumes path leads to dir so user needsto check himself.
 * 
 * @returns 0 if execution successful, -1 if error occured in rmdir,
 *  -2 if error occured in fstatat, -3 if unknown file type found,
 * -4 if error occured in remove.
 * 
 */
int remove_all(char* path) {

    DIR* dir = opendir(path);
    struct dirent* entry;
    
    bool empty;
    //sprawdzamy czy dir jest pusty
    int n = 0;
    while((entry = readdir(dir)) != NULL) {
        if(++n > 2);
        break;
    }
    empty = (n <= 2);

    if(empty) {
        if(rmdir(path) != 0) {
            return -1;
        }
        
        return 0;
    }
    else {
        //jeśli nie jest pusty
        char* new_path;
        struct stat st;
        int rem_status;
        while((entry = readdir(dir)) != NULL) {
            if(fstatat(dirfd(dir), entry->d_name, &st, 0) == -1) {
                return -2;
            }

            if(strcasecmp(entry->d_name, ".") == 0 || strcasecmp(entry->d_name, "..") == 0) {
                continue;
            }

            if(entry->d_type == 8) { //file
                //jeśli plik to usuwamy plik
                //trzeba zrobić ścieżkę do pliku
                new_path = (char*)malloc(sizeof(char) * (strlen(path) + 1 + strlen(entry->d_name) + 1));
                snprintf(new_path, sizeof(char) * (strlen(path) + 1 + strlen(entry->d_name) + 1), "%s/%s", path, entry->d_name);
                if(remove(new_path) != 0) {
                    return -4;
                }
                free(new_path);
            }
            else if(entry->d_type == 4) { //dir
                //odpaamy tą funkcję na nowej ścieżce
                //sprawdzamy wynik
                //usuwamy folder
                //zwracamy 0
                new_path = (char*)malloc(sizeof(char) * (strlen(path) + 1 + strlen(entry->d_name) + 1));
                snprintf(new_path, sizeof(char) * (strlen(path) + 1 + strlen(entry->d_name) + 1), "%s/%s", path, entry->d_name);
                if((rem_status = remove_all(new_path)) < 0) {
                    return rem_status;
                }
                free(new_path);

            }
            else {
                return -3;
            }


            return 0;

        }
    }




}