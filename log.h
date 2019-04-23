#ifndef _LOG_H_
#define _LOG_H_

#include <syslog.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

enum log_priority {
    L_DBG = 0,
    L_INF,
    L_WRN,
    L_ERR
};

/* Format of location information. Includes the pid of the process, filename,
 * function name, and line number. */
#define LOC_FMT     "[%d:%s:%s:%d]"
#define LOC_FMT_LEN (sizeof(LOC_FMT) - 1)

#define DEBUGGING (log_threshold <= L_DBG)

/* Make sure __func__ macro is defined. This macro is available in C99, but
 * older gcc versions provided __FUNCTION__. */
#if __STDC_VERSION__ < 199901L
#  if __GNUC__ >= 2
#    define __func__ __FUNCTION__
#  else
#    define __func__ "?"
#  endif
#endif

#ifdef __PLATFORM_android
#  include <android/log.h>
#  define _SYSLOG(pri, pfx, m, a...)                                    \
    if (DEBUGGING) {                                                    \
        __android_log_print(pri + ANDROID_LOG_DEBUG, NAME, "[%s:%s:%d] "\
                            m, __FILE__, __func__, __LINE__, ## a);     \
    } else {                                                            \
        __android_log_print(pri + ANDROID_LOG_DEBUG, NAME, m, ## a);    \
    }
#else
#  define _SYSLOG(pri, pfx, m, a...)                                    \
    if (DEBUGGING) {                                                    \
        syslog(__p2s(pri), LOC_FMT pfx m, getpid(), __FILE__, __func__, \
               __LINE__, ## a);                                         \
    } else {                                                            \
        syslog(__p2s(pri), "[%d]" pfx m, getpid(), ## a);               \
    }
#endif

/* Define NO_LOG globally if you want to compile the program without any
 * logging code. This is useful for profiling. */
#ifdef NO_LOG
#  define _LOG(pri, f, pfx, m, a...)
#else
#  define _LOG(pri, f, pfx, m, a...)                                    \
do {                                                                    \
    if (log_threshold > (pri)) break;                                   \
    if (log_syslog) {                                                   \
        _SYSLOG(pri, pfx, m, ## a);                                     \
    } else {                                                            \
        if (DEBUGGING) {                                                \
            fprintf(f, "%s " LOC_FMT pfx m "\n", __ltime(), getpid(),   \
                    __FILE__, __func__, __LINE__, ## a);                \
        } else {                                                        \
            fprintf(f, "%s [%d]" pfx m "\n", __ltime(), getpid(), ## a);\
        }                                                               \
    }                                                                   \
} while(0)
#endif

#define LOG(p, m, a...) _LOG((p),   stdout, " ",          m, ## a)
#define DBG(m, a...)    _LOG(L_DBG, stdout, " DEBUG: ",   m, ## a)
#define INF(m, a...)    _LOG(L_INF, stdout, " ",          m, ## a)
#define WRN(m, a...)    _LOG(L_WRN, stderr, " WARNING: ", m, ## a)
#define ERR(m, a...)    _LOG(L_ERR, stderr, " ERROR: ",   m, ## a)

extern int log_syslog;
extern int log_threshold;

void start_logger(const char *name);
void stop_logger (void);

/* Convert message priority to syslog priority */
int __p2s(enum log_priority pri);
const char *__ltime(void);

#endif /* _LOG_H_ */
