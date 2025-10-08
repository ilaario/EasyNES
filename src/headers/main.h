//
// Created by Dario Bonfiglio on 10/4/25.
//

#ifndef EASYNES_MAIN_H
#define EASYNES_MAIN_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cartridge.h"
#include "logger.h"

#define ANSI_RED   "\x1b[31m"
#define ANSI_RESET "\x1b[0m"

#define KIB(x) ((x) * 1024)


#ifdef DEBUGLOG
#define perror(fmt, ...)                                                       \
    do {                                                                       \
        struct timeval tv;                                                     \
        gettimeofday(&tv, NULL);                                               \
                                                                               \
        struct tm tm_info;                                                     \
        localtime_r(&tv.tv_sec, &tm_info);                                     \
                                                                               \
        char ts[64];                                                           \
        size_t ts_len = strftime(ts, sizeof(ts),                               \
                                 "%d/%m/%Y - %H:%M:%S", &tm_info);             \
        ts_len += snprintf(ts + ts_len, sizeof(ts) - ts_len,                   \
                           ".%03d", (int)(tv.tv_usec / 1000));                 \
                                                                               \
        fprintf(stdout, ANSI_RED                                               \
                "[%s] - [%s:%d] - [PID:%d] - [%s] --> [" fmt "]\n" ANSI_RESET, \
                "SYSER",                                                       \
                __FILE__,__LINE__,                                             \
                (int)getpid(),                                                 \
                ts,                                                            \
                ##__VA_ARGS__);                                                \
    } while (0)

#else
#define perror(buffer, ...) syserr_log(buffer, ##__VA_ARGS__)
#endif

#ifdef DEBUGLOG
#define printf(fmt, ...)                                                       \
    do {                                                                       \
        struct timeval tv;                                                     \
        gettimeofday(&tv, NULL);                                               \
                                                                               \
        struct tm tm_info;                                                     \
        localtime_r(&tv.tv_sec, &tm_info);                                     \
                                                                               \
        char ts[64];                                                           \
        size_t ts_len = strftime(ts, sizeof(ts),                               \
                                 "%d/%m/%Y - %H:%M:%S", &tm_info);             \
        ts_len += snprintf(ts + ts_len, sizeof(ts) - ts_len,                   \
                           ".%03d", (int)(tv.tv_usec / 1000));                 \
                                                                               \
        fprintf(stdout,                                                        \
                "[%s] - [%s:%d] - [PID:%d] - [%s] --> [" fmt "]\n",            \
                "TRACE",                                                       \
                __FILE__, __LINE__,                                            \
                (int)getpid(),                                                 \
                ts,                                                            \
                ##__VA_ARGS__);                                                \
    } while (0)

#else
#define printf(buffer, ...) appinfo_log(buffer, ##__VA_ARGS__)
#endif

#ifdef DEBUGLOG
#define log_init(filePath) printf("DEBUG LOG STARTED AT %s", filePath)
#else
#define log_init(filePath) log_init(filePath)
#endif

#ifdef DEBUGLOG
#define log_stop() printf("DEBUG LOG STOPPED")
#else
#define log_stop() log_stop()
#endif

#endif //EASYNES_MAIN_H
