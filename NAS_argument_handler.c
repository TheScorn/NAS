#include "NAS.h"
#include <string.h>
#include <stdlib.h>
#include <strings.h>
#include <ctype.h>

int verify_int(char* int_str);

/**
 * @brief Function for handling args given to main
 * 
 * Function iterates over arguments given to main, then sets values to corresponding variables
 * Flags:
 * -h Prints help message
 * -p {port number} sets port number
 * -v turn on verbose mode
 * 
 * If -h given at any point, the result will be printing help message and stopping the server
 * 
 * @param argc number of arguments given to main
 * 
 * @param argv address of the list of arguments given to main
 * 
 * @param args address of a struct holding values to be set
 * 
 * @return 0 if execution successful
 */
int handle_arguments(int argc, char** argv, struct input_args_struct* args) {
    
    if(argc == 1) {
        //nie ma argumentów poza nazwą programu więc wychodzimy
        return 0;
    }

    //jest przynajmniej 1 arg wymagający obsużenia
    //zaczynamy od 1
    for(int i = 1; i < argc; i++) {
        if(strcasecmp(argv[i], "-h") == 0) {
            args->print_help = true;
            return 0;
        }
        else if(strcasecmp(argv[i], "-v") == 0) {
            args->verbose_init = true;
        }
        else if(strcasecmp(argv[i], "-p") == 0) {
            //param_flag

            //musimy sprawdzić czy jest kolejna wartość
            if(i + 1 >= argc) {
                //jeśli nie ma koeljnego argumentu to wyświetlamy help i wychodzimy
                args->print_help = true;
                return -1;
            }

            if(verify_int(argv[i+1]) != 0) {
                args->print_help = true;
                return -1;
            }

            args->selected_port = atoi(argv[i + 1]);
            i++;

        }
    }



}

//są dwa typy argumentów
//zwyczajna flaga
//i falga żądająca wartości
//for
//rozpoznajemy flagę
//jeśli jest to prosta flaga to ustawiamy jej wartość
//jeśli dodatkowo jest to -h to od razu wychodzimy bo nie ma już znaczenia jakie będą inne wartości
//jeśli jest to param_flag to zczytujemy kolejną wartość
//sprawdzamy czy zgadza się jej typ
//zapisujemy wartość i zwiększamy i o 1 żeby nie odczytywać samej warości


/**
 * @brief Simple function for checking if str value can be representent by an int
 * 
 * 
 * @param int_str char pointer supposedly representing string
 * 
 * @return 0 if str can be represented by int, -1 if not.
 */
int verify_int(char* int_str) {
    int i = 0;
    size_t len = strlen(int_str);

    while(i < len) {
        if(!isdigit(*(int_str+i))) {
            return -1;
        }
        ++i;

    }
    return 0;
}
