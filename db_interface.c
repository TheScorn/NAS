#include "NAS.h"
#include <string.h>
#include <strings.h>
#include "sqlite3.h"
#include <stdlib.h>
#define DB_PATH "/home/thescorn/science/mine/home_server/NAS/Database/NAS.db"

/**
 * @brief Function for testing db connection
 * 
 * 
 * @return 0 if connection is successful, -1 if not.
 */
int test_con() {
    sqlite3 *db;
    if(sqlite3_open(DB_PATH, &db) != 0) {
        return -1;
    }
    return 0;
}


/**
 * @brief Authenticating function
 * 
 * Function accepts username and password, retrieves password from db compares them 
 * and returns int marking the result
 * 
 * @param username pointer to null terminated string containing username
 * 
 * @param password pointer to null terminated string containing password
 * 
 * @return 0 if authentication successful, 1 if authentication successful with elevated priviledges, 
 * -1 if db could not be opened, -2 if prepare failed, -3 if step failed, -4 if user not found, -5 if password incorrect.
 * 
 */
int authenticate(char* username, char* password) {
    sqlite3 *db;
    if(sqlite3_open(DB_PATH, &db) != 0) {
        return -1;
    }

    sqlite3_stmt* stmt;

    char select_statement[100];

    snprintf(select_statement, 100, "SELECT password, access FROM Users WHERE username = \"%s\";", username);
    if(sqlite3_prepare_v2(db, select_statement, -1, &stmt, NULL) != 0) {
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return -2;
    }

    int sqlite_step = sqlite3_step(stmt);
    if(sqlite_step == SQLITE_DONE) {
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return -4;
    }

    else if(sqlite_step != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return -3;
    }

    const unsigned char* selected_password = (const unsigned char*)sqlite3_column_text(stmt, 0);
    

    if(strcasecmp(password, selected_password) != 0) {
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return -5;
    }

    int access = sqlite3_column_int(stmt, 1);
    

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    if(access == 1) {
        return 1;
    }
    else {
        return 0;
    }

}