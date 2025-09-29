#include "interface.h"

void log_connection(char* server_addr){
    time_t current_time = time(NULL); // obtain time since 01/01/1970 UTC

    struct tm *local_time = localtime(&current_time); // convert to local time

    printf("%d-%02d-%02d %02d:%02d:%02d server_addr %s\n",
        local_time->tm_year + 1900, // tm_year is the number of years since 1900
        local_time->tm_mon + 1, // tm_mon is the month (0=January, 11=December)
        local_time->tm_mday, // tm_mday is the day of the month
        local_time->tm_hour, // tm_hour is the hour (0-23)
        local_time->tm_min, // tm_min is the minute
        local_time->tm_sec, // tm_sec is the second
        server_addr);
}
