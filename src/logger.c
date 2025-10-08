#include "headers/logger.h"

#define LOG_INITIAL_BUFSZ 256
#define LOG_MAX_BUFSZ  (1<<20)
#define LOG_TS_BUFSZ   64
#define PREFIX_BUFSZ  128
#define MAX_DASHES    256
#define DASHES_BUFSZ 46
#define MAX_CONTENT_WIDTH  80

FILE* fp;

static int vlogx_impl(log_level level, const char *fmt, va_list ap_in)
{
    static const char *funct_name = "logx";
    char stackbuf[LOG_INITIAL_BUFSZ];
    char *buf  = stackbuf;
    size_t bufsz = sizeof(stackbuf);

    while (1) {
        va_list ap;
        va_copy(ap, ap_in);
        int n = vsnprintf(buf, bufsz, fmt, ap);
        va_end(ap);

        if (n < 0) {
            fprintf(stderr, "%s %d - FUNCT: %s, Error: vsnprintf failed (%s)\n",
                    __FILE__, __LINE__, funct_name, strerror(errno));
            if (buf != stackbuf) free(buf);
            return -1;
        }
        if ((size_t)n < bufsz)
            break;

        size_t new_sz = bufsz * 2;
        if ((size_t)n + 1 > new_sz)
            new_sz = (size_t)n + 1;
        if (new_sz > LOG_MAX_BUFSZ) {
            fprintf(stderr, "%s %d - FUNCT: %s, Error: log line exceeds %u bytes\n",
                    __FILE__, __LINE__, funct_name, LOG_MAX_BUFSZ);
            if (buf != stackbuf) free(buf);
            return -1;
        }

        char *new_buf = (buf == stackbuf)
                        ? malloc(new_sz)
                        : realloc(buf, new_sz);
        if (!new_buf) {
            fprintf(stderr, "%s %d - FUNCT: %s, Error: out of memory\n",
                    __FILE__, __LINE__, funct_name);
            if (buf != stackbuf) free(buf);
            return -1;
        }

        if (buf == stackbuf)
            memcpy(new_buf, stackbuf, bufsz);

        buf   = new_buf;
        bufsz = new_sz;
    }

    int rc = write_buffer(buf, level);

    if (buf != stackbuf)
        free(buf);

    if (rc != 0) {
        fprintf(stderr, "%s %d - FUNCT: %s, Error: Failed writing log with rc: %d\n",
                __FILE__, __LINE__, funct_name, rc);
    }
    return rc;
}


int write_on_system_log(const char* buffer, char* program_name) {
    int rc = 0;
    if(program_name == NULL) {
        return EINVAL;
    }

#if defined(LINUX) || defined(MACOS)
    openlog(program_name, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
    // syslog(LOG_MAKEPRI(LOG_LOCAL1, LOG_NOTICE), "Program started by User %d", getuid());
    syslog(LOG_INFO, "%s", buffer);
    closelog();

    return rc;
#endif

    return rc;
}

/***
 * Return the log level
 * @param level log_level: The requested log_level in the enum
 * @return The requested log_level as a string
 */
char *get_log_level(enum log_level level) {
    switch (level) {
        case DEBUG:
            return "DEBUG";
        case TRACE:
            return "TRACE";
        case APPERR:
            return "APPER";
        case SYSERR:
            return "SYSER";
        case SYSINFO:
            return "SYSIN";
        case APPINFO:
            return "APPIN";
        default:
            fprintf(stderr, "!!! INVALID LOG LEVEL !!!\n");
            return NULL;
    }
}

/***
 * Write a buffer inside a specific FILE
 * @param buffer char[256]: The requested buffer
 * @param level log_level: The log_level requested
 * @return Return 0 if OK, else if error
 */
int write_buffer(const char* buffer, log_level level) {
    const char *funct_name = "write_buffer";
    if (!fp) {
        fprintf(stderr,
                "%s %d - FUNCT: %s, Error: NULL File pointer (errno=%d)\n",
                __FILE__, __LINE__, funct_name, errno);
        return errno;
    }

    /* 1) Costruisco il timestamp in ts[] */
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm tm_info;
    localtime_r(&tv.tv_sec, &tm_info);

    char ts[LOG_TS_BUFSZ];
    size_t ts_len = strftime(ts, sizeof(ts),
                             "%d/%m/%Y - %H:%M.%S", &tm_info);
    ts_len += snprintf(ts + ts_len,
                       sizeof(ts) - ts_len,
                       ".%03ld",
                       tv.tv_usec / 1000);

    /* 2) Preparo il prefix una volta sola */
    char prefix[PREFIX_BUFSZ];
    int prefix_len = snprintf(prefix, sizeof(prefix),
                              "[%s] - [PID:%d] - [%s] --> ",
                              get_log_level(level),
                              (int)getpid(),
                              ts);
    if (prefix_len < 0 || prefix_len >= (int)sizeof(prefix)) {
        /* fallback semplificato */
        prefix_len = snprintf(prefix, sizeof(prefix),
                              "[%s] --> ", ts);
    }

    /* 3) Preparo la stringa di dashes lunga quanto prefix_len */
    unsigned char* temp = (unsigned char*)malloc(5 * sizeof(unsigned char));
    sprintf((char*)temp, "%d", (int)getpid());
    int pidChars = 0;
    while(temp[pidChars] != '\0') pidChars++;

    char dashes[DASHES_BUFSZ + pidChars];
    int n_dashes = prefix_len < (int)sizeof(dashes) ? prefix_len : (int)sizeof(dashes)-1;
    memset(dashes, '-', n_dashes);
    dashes[n_dashes] = '\0';

    /* 4) Ciclo sul buffer: frammentazione per newline o per lunghezza */
    const char *p = buffer;
    size_t rem = strlen(buffer);
    int  chunk_no = 0;

    while (rem > 0) {
        /* cerca primo '\n' dentro i prossimi MAX_CONTENT_WIDTH caratteri */
        size_t scan_len = rem < MAX_CONTENT_WIDTH ? rem : MAX_CONTENT_WIDTH;
        const char *nl = memchr(p, '\n', scan_len);
        size_t chunk_len;
        int     has_nl;

        if (nl) {
            chunk_len = (size_t)(nl - p);
            has_nl    = 1;
        } else {
            chunk_len = scan_len;
            has_nl    = 0;
        }

        /* 5) Stampa chunk */
        if (chunk_no == 0) {
            /* prima riga: usiamo prefix */
            fprintf(fp,    "%s[%.*s]\n", prefix, (int)chunk_len, p);
            fprintf(stdout, "%s[%.*s]\n", prefix, (int)chunk_len, p);
        } else {
            /* continuazione: usiamo dashes come “prefix” */
            fprintf(fp,    "%s] --> [%.*s]\n", dashes, (int)chunk_len, p);
            fprintf(stdout, "%s] --> [%.*s]\n", dashes, (int)chunk_len, p);
        }

        /* avanzo */
        p   += chunk_len;
        rem -= chunk_len;
        if (has_nl && *p == '\n') {
            /* salto il carattere di newline originale */
            p++;
            rem--;
        }
        chunk_no++;
    }

    /* 6) Scrivo anche sul system log (se fallisce, lo denuncio ma vado avanti) */
    int rc_sys = write_on_system_log(buffer, "logger");
    if (rc_sys != 0) {
        fprintf(stderr,
                "%s %d - FUNCT: %s, Error: system log rc=%d\n",
                __FILE__, __LINE__, funct_name, rc_sys);
    }

    fflush(fp);
    fflush(stdout);
    return 0;
}

/***
 * Initialise the log, opening the file
 * @param filePath const char*: Path to the file we want to write on
 * @return Return 0 if OK, else if error
 */
int log_init(const char* filePath){
    char *funct_name = "log_init";
    if(filePath == NULL) {
        fprintf(stderr, "%s %d - FUNCT: %s, Error: File Path can't be NULL\n", __FILE__, __LINE__, funct_name);
        return EINVAL;
    }

    fp = fopen(filePath, "a");
    if (fp == NULL) {
        fprintf(stderr, "%s %d - FUNCT: %s, Error: Failed opening FP\n", __FILE__, __LINE__, funct_name);
        return EBADF;
    }

#ifdef DEBUGLOG
    write_buffer("LOG STARTED", TRACE);
#endif

    return 0;
}

/***
 * Interrupt the log, closing the file pointer
 * @return Return 0 if OK, else if error
 */
int log_stop(){

#ifdef DEBUGLOG
    write_buffer("EXITING", TRACE);
#endif

    return fclose(fp);
}

/***
 * Log general function
 * @param log log_level: The requested log level
 * @param buffer char[256]: Buffer
 * @return Return 0 if OK, else if error
 */
int logx(log_level level, const char *buffer, ...)
{
    va_list ap;
    va_start(ap, buffer);
    int rc = vlogx_impl(level, buffer, ap);
    va_end(ap);
    return rc;
}

/***
 * Log in SYSERROR level
 * @param buffer char[256]: Buffer
 * @return Return 0 if OK, else if error
 */
int syserr_log(const char* buffer, ...){
    char *funct_name = "syserr_log";
    va_list ap;
    va_start(ap, buffer);
    int rc = vlogx_impl(SYSERR, buffer, ap);
    if(rc != 0) {
        fprintf(stderr, "%s %d - FUNCT: %s, Error: Failed writing log with rc: %d\n", __FILE__, __LINE__, funct_name, rc);
        return rc;
    }
    va_end(ap);
    return rc;
}

/***
 * Log in SYSINFO level
 * @param buffer char[256]: Buffer
 * @return Return 0 if OK, else if error
 */
int sysinfo_log(const char* buffer, ...){
    char *funct_name = "sysinfo_log";
    va_list ap;
    va_start(ap, buffer);
    int rc = vlogx_impl(SYSINFO, buffer, ap);
    if(rc != 0) {
        fprintf(stderr, "%s %d - FUNCT: %s, Error: Failed writing log with rc: %d\n", __FILE__, __LINE__, funct_name, rc);
        return rc;
    }
    va_end(ap);
    return rc;
}

/***
 * Log in APPERROR level
 * @param buffer char[256]: Buffer
 * @return Return 0 if OK, else if error
 */
int apperr_log(const char* buffer, ...){
    char *funct_name = "apperr_log";
    va_list ap;
    va_start(ap, buffer);
    int rc = vlogx_impl(APPERR, buffer, ap);
    if(rc != 0) {
        fprintf(stderr, "%s %d - FUNCT: %s, Error: Failed writing log with rc: %d\n", __FILE__, __LINE__, funct_name, rc);
        return rc;
    }
    va_end(ap);
    return rc;
}

/***
 * Log in APPINFO level
 * @param buffer char[256]: Buffer
 * @return Return 0 if OK, else if error
 */
int appinfo_log(const char* buffer, ...){
    char *funct_name = "appinfo_log";
    va_list ap;
    va_start(ap, buffer);
    int rc = vlogx_impl(APPINFO, buffer, ap);
    if(rc != 0) {
        fprintf(stderr, "%s %d - FUNCT: %s, Error: Failed writing log with rc: %d\n", __FILE__, __LINE__, funct_name, rc);
        return rc;
    }
    va_end(ap);
    return rc;
}
