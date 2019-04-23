#ifndef _UTILS_H_
#define _UTILS_H_

#include <unistd.h>
#include <sys/time.h>
#include <stdint.h>
#include <sys/uio.h>
#include <ctype.h>
#include "str.h"

#define ARRAY_SIZE(a) (sizeof((a)) / sizeof(*(a)))

#define MAX_TIME INT64_MAX

#define IS_HEX(c)                   \
    (((c) >= '0' && (c) <= '9')     \
     || ((c) >= 'a' && (c) <= 'f')  \
     || ((c) >= 'a' && (c) <= 'F'))

#define HEX2CHR(c) (((c) <= '9') ? ((c) - '0') : (tolower((c)) - 'a' + 10))

#define SEC 1000

typedef int64_t smtime;

smtime now (void);


/* Read a line terminated by LF from the file descriptor fd into the buffer
 * 'dst' provided by the caller. The resulting string will contain the
 * terminating LF. On blocking sockets the function blocks until an entire
 * line is available. On non-blocking sockets you need to call the function
 * repeatedly until it returns either 0 (EOF) or 1 (LF found). Negative return
 * values are those returned by read(2) and the function sets errno.
 *
 * The parameter maxlen, if not 0, limits the maximum size of the resulting
 * buffer. You should always provide this parameter when reading from network
 * sockets to prevent memory depletion attacks due to enlarging dst. Readl
 * returns 0 (EOF) if the limit has been reached. Value 0 disables length
 * checking.
 */
ssize_t readl(int fd, sbuf *dst, size_t maxlen);

/* Like readl, but returns 1 only after n characters have been read. This
 * function is useful for reading messages with a known length. Returns n when
 * a complete message has been received. Returns codes <= 0 are those of
 * read(2).
 */
ssize_t readn(int fd, sbuf *dst, size_t n);

int   load_text_file (sbuf *buf, const char *fn);
char *mac2str        (const char *mac, size_t len, char sep);
void  encode_base64  (char *dst, const unsigned char *src, size_t len);

void *xcalloc (size_t nmemb, size_t size);
void *xmalloc (size_t size);
void *xrealloc(void *ptr, size_t size);
void  xfree   (void *ptr);

char *xstrdup (const char *s);
char *xstrndup(const char *s, size_t len);

ssize_t safe_read  (int fd, void *buf, size_t n);
ssize_t safe_writev(int fd, struct iovec *iov, int iovcnt);
int     make_nonblocking(int fd);

void chr2hex (char *buf, char c);
int  chr2utf8(char *buf, size_t *len, uint16_t c);

#ifdef __OS_darwin
uint64_t htobe64(uint64_t host_64bits);
uint64_t be64toh(uint64_t be_64bits);
uint64_t htole64(uint64_t host_64bits);
uint64_t le64toh(uint64_t le_64bits);
#endif

#ifdef __PLATFORM_android
/* Android bionic libc appears to follow the original OpenBSD naming
 * convention and puts 64 at the end of the function name, unlike glibc where
 * it is in the middle of the name. */
#define be64toh betoh64
#define le64toh letoh64
#endif

/* Make a directory path recursively. The function returns -1 on error and
 * sets errno. */
int make_path(const char *path, mode_t mode);

#define ISOMS_MAXLEN (sizeof("2013-02-02T23:59:59.987+0100") - 1)
/* Convert number of ms since the Epoch into the ISO representation */
int ms2iso(char *buf, size_t len, uint64_t ms);

#endif /* _UTILS_H_ */
