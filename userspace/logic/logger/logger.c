#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "logger.h"

static const char* tagStrings[] = {"I", "E", "W", "D"};

const char* getStringFromTag(enum Tag tag){
    return tagStrings[tag];
}

void logMsg(enum Tag tag, const char* format, ...){

    if ((int)tag <= LOG_LEVEL && format != NULL){
        char buf[LOG_MSG_MAX_LEN];
        int firstHalfLen;
        va_list ap;
        va_start(ap, format);

        // Inserts tag on first half of log msg
        snprintf(buf, sizeof(char) * LOG_MSG_MAX_LEN, "[%s] ", getStringFromTag(tag));
        // concats format to log msg
        firstHalfLen = strlen(buf);
        vsnprintf(buf + firstHalfLen, LOG_MSG_MAX_LEN - firstHalfLen, format, ap);
        // Prints log msg on stdout
        fprintf(stdout, buf, getStringFromTag(tag), format);
        fflush(stdout);
        va_end(ap);
    }
}