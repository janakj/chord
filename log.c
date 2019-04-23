#include "log.h"
#include <syslog.h>
#include <time.h>

/* Format of timestamps printed to standard or error outputs. */
#define TIME_FMT        "%b-%d %H:%M:%S"
#define TIME_FMT_MAXLEN 15 /* The length of the resulting string */

/* If set to 1 write messages to syslog. If 0 write them to standard and error
 * outputs. */
int log_syslog = 1;

/* Minimum logging threshold. Only messages with priority higher or equal to
 * log_threshold will be logged. */
int log_threshold = L_INF;

/* Returns text representation of current date and time. Only current month,
 * day and time down to a second are printed. This is used when logging to the
 * standard output. Returns empty string on error. */
const char *
__ltime(void)
{
    time_t t;
    struct tm *tmp;
    static char buf[TIME_FMT_MAXLEN + 1];

    t = time(NULL);
    if (!(tmp = localtime(&t))
        || strftime(buf, TIME_FMT_MAXLEN + 1, TIME_FMT, tmp) != TIME_FMT_MAXLEN)
        *buf = '\0';
    return buf;
}


void
start_logger(const char *name)
{
    if (log_syslog)
        openlog(name, LOG_CONS, LOG_DAEMON);
}


void
stop_logger(void)
{
    if (log_syslog)
        closelog();
}

/* Convert message priority to syslog priority */
inline int
__p2s(enum log_priority pri)
{
    switch(pri) {
    case L_DBG: return LOG_DEBUG;
    case L_INF: return LOG_INFO;
    case L_WRN: return LOG_WARNING;
    case L_ERR: return LOG_ERR;
    default:
        if (pri < L_DBG) return LOG_DEBUG - pri;
        else return LOG_ERR - pri + L_ERR;
        break;
    }
}
