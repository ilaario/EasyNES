#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h> 
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <syslog.h>


typedef enum log_level{
    DEBUG,
    TRACE,
    APPINFO,
    APPERR,
    SYSINFO,
    SYSERR
} log_level;

/***
 * Return the log level
 * @param level log_level: The requested log_level in the enum
 * @return The requested log_level as a string
 */
char* get_log_level(log_level level);

/***
 * Write a buffer inside a specific FILE
 * @param buffer char[256]: The requested buffer
 * @param level log_level: The log_level requested
 * @return Return 0 if OK, else if error
 */
int write_buffer(const char* buffer, log_level level);

/***
 * Log general function
 * @param log log_level: The requested log level
 * @param buffer char[256]: Buffer
 * @return Return 0 if OK, else if error
 */
int logx(log_level level, const char *buffer, ...);

/***
 * Log in SYSERROR level
 * @param buffer char[256]: Buffer
 * @return Return 0 if OK, else if error
 */
int syserr_log(const char* buffer, ...);

/***
 * Log in SYSINFO level
 * @param buffer char[256]: Buffer
 * @return Return 0 if OK, else if error
 */
int sysinfo_log(const char* buffer, ...);

/***
 * Log in APPERROR level
 * @param buffer char[256]: Buffer
 * @return Return 0 if OK, else if error
 */
int apperr_log(const char* buffer, ...);

/***
 * Log in APPINFO level
 * @param buffer char[256]: Buffer
 * @return Return 0 if OK, else if error
 */
int appinfo_log(const char* buffer, ...);

/***
 * Initialise the log, opening the file
 * @param filePath const char*: Path to the file we want to write on
 * @return Return 0 if OK, else if error
 */
int log_init(const char* filePath);

/***
 * Interrupt the log, closing the file pointer
 * @return Return 0 if OK, else if error
 */
int log_stop();

#endif