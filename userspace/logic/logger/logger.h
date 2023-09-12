#ifndef LOGGER_H_INCLUDED
#define LOGGER_H_INCLUDED

#include <errno.h>
#include <stdbool.h>

#define LOG_MSG_MAX_LEN 1024
#define LOG_MSG_SEP ": "

#ifndef LOG_LEVEL
#define LOG_LEVEL 1  // Only messages with a Tag <= LOG_LEVEL will be displayed
#endif

#include <string.h>
#include <stdlib.h>

#define LOG_ERROR(msg, ...)\
    do {\
        char *log_error_buffer = (char *)calloc(strlen(__func__) + strlen(LOG_MSG_SEP) + strlen(msg) + 1, sizeof(char)); \
        memcpy(log_error_buffer, __func__, strlen(__func__)); \
        log_error_buffer = strcat(log_error_buffer, LOG_MSG_SEP); \
        log_error_buffer = strcat(log_error_buffer, msg); \
        logMsg(LOG_TAG_E, log_error_buffer __VA_OPT__(,) __VA_ARGS__); \
    } while(0);

/**
 * Interprets the non-null return value of a function as an error code and logs the
 * provided message
*/
#define ON_ERROR_LOG(isError, msg, ...)  \
    if (isError){  \
        LOG_ERROR(msg __VA_OPT__(,) __VA_ARGS__); \
    }
/**
 * Interprets the non-null return value of a function as an error code and logs the
 * specified message, along with the error string corresponding to the current value
 * of errno.
*/

#define LOG_ERRNO(isError, msg, ...) \
    do{\
        int err = errno; \
        char *errmsg = strerror(err); \
        char *log_errno_buffer = (char *)calloc(strlen(msg) +strlen(LOG_MSG_SEP) + strlen(errmsg) + 2, sizeof(char));\
        memcpy(log_errno_buffer, msg, strlen(msg)); \
        log_errno_buffer = strcat(log_errno_buffer, LOG_MSG_SEP); \
        log_errno_buffer = strcat(log_errno_buffer, errmsg); \
        log_errno_buffer = strcat(log_errno_buffer, "\n");\
        LOG_ERROR(log_errno_buffer __VA_OPT__(,) __VA_ARGS__); \
        free(log_errno_buffer); \
    } while (0);


#define ON_ERROR_LOG_ERRNO_AND_RETURN(isError, retVal, msg, ...) \
    if (isError){\
        LOG_ERRNO(isError, msg __VA_OPT__(,) __VA_ARGS__); \
        return retVal; \
    }

/**
 * Interprets the non-null return value of a function as an error code and logs the
 * specified message, then returns the provided error code to the caller of the function.
*/
#define ON_ERROR_LOG_AND_RETURN(ret, retVal, msg, ...) \
    if (ret != 0){  \
        LOG_ERROR(msg __VA_OPT__(,) __VA_ARGS__); \
        return retVal; \
    } 


/**
 * Use it to throw an error message signaling that a function is not implemented yet
*/
#define LOG_UNIMPLEMENTED_CALL() \
    logMsg(LOG_TAG_E, "%s is not implemented yet\n", __func__); \


enum Tag
{

    LOG_TAG_I, // Informative messages
    LOG_TAG_E, // Errors
    LOG_TAG_W, // Warnings
    LOG_TAG_D  // debug messages.

};

/**
 * Logs a message, if the tag is <= LOG_LEVEL.
 * You can pass a format string and a variable number of arguments, just like printf.
*/
void logMsg(enum Tag tag, const char* msg, ...);

/**
 * Use this to log the call of passed function name as unimplemented.
*/
void logUnimplementedCall(const char* funcName);

#endif // LOGGER_H_INCLUDED