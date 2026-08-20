#include "NAS.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

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
