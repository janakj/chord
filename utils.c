#define _GNU_SOURCE
#include "utils.h"
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "defs.h"


ssize_t
safe_read(int fd, void *buf, size_t n)
{
    ssize_t rc;

    do {
        rc = read(fd, buf, n);
    } while (rc < 0 && errno == EINTR);
    return rc;
}


/* FIXME: The function implementation is inefficient because it reads the
 * input char by char. We should switch to a buffered reader here instead.
 * This is not an issue for the moment because the function is mostly used in
 * non-critical paths.
 */
ssize_t
readl(int fd, sbuf *dst, size_t maxlen)
{
    int rc;
    char *p;
    ssize_t left;

    assert(dst);

    left = maxlen - str_len(dst);
    while(left > 0) {
        p = str_getmem(dst, 1);
        rc = safe_read(fd, p, 1);
        if (rc <= 0) return rc;

        dst->data.len++;
        if (*p == '\n') return 1;
        left--;
    }
    return 0;
}


ssize_t
readn(int fd, sbuf *dst, size_t n)
{
    int rc;
    char *p;
    ssize_t left;

    assert(dst);

    left = n - str_len(dst);
    while(left > 0) {
        p = str_getmem(dst, left);
        rc = safe_read(fd, p, left);
        if (rc <= 0) return rc;
        dst->data.len += rc;
        left -= rc;
    }
    return n;
}


int
load_text_file(sbuf *buf, const char *fn)
{
    int fd, rc;
    char *p;

    assert(buf);
    assert(fn);

    fd = open(fn, O_RDONLY);
    if (fd < 0) goto error;

    while(1) {
        p = str_getmem(buf, 64);
        rc = safe_read(fd, p, 64);
        if (rc < 0) goto error;
        buf->data.len += rc;
        if (rc < 64) break;
    }
    str_trim(&buf->data);
    close(fd);
    return 0;
error:
    if (fd >=0) close(fd);
    return -1;
}


void
chr2hex(char *buf, char c)
{
    static char hex[] = "0123456789abcdef";

    assert(buf);
    buf[0] = hex[(c & 0xf0) >> 4];
    buf[1] = hex[c & 0x0f];
}


char *
mac2str(const char *mac, size_t len, char sep)
{
    int i;
    char *p;
    static char buf[8 * 2 + 8 - 1 + 1];

    assert(mac);
    if (len != 6 && len != 8) return NULL;
    if (!sep) sep = ':';

    for(i = 0, p = buf; i < len; i++) {
        if (i) *p++ = sep;
        chr2hex(p, mac[i]);
        p += 2;
    }
    *p = '\0';
    return buf;
}


smtime
now(void)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec * 1000 + now.tv_usec / 1000;
}


void *
xcalloc(size_t nmemb, size_t size)
{
    void *p;

    p = calloc(nmemb, size);
    if (nmemb && size && !p) abort();
    return p;
}


void *
xmalloc(size_t size)
{
    void *p;

    p = malloc(size);
    if (size && !p) abort();
    return p;
}


void *
xrealloc(void *ptr, size_t size)
{
    void *p;

    p = realloc(ptr, size);
    if (size && !p) abort();
    return p;
}


void
xfree(void *ptr)
{
    free(ptr);
}


char *
xstrdup(const char *s)
{
    char *p;

    p = strdup(s);
    if (s && !p) abort();
    return p;
}


char *
xstrndup(const char *s, size_t len)
{
    char *p;
    p = strndup(s, len);
    if (s && !p) abort();
    return p;
}


void
encode_base64(char *dst, const unsigned char *src, size_t len)
{
    static unsigned char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    assert(src != NULL && dst != NULL);

    while (len > 2) {
        *dst++ = alphabet[(src[0] >> 2) & 0x3f];
        *dst++ = alphabet[((src[0] & 3) << 4) | (src[1] >> 4)];
        *dst++ = alphabet[((src[1] & 0x0f) << 2) | (src[2] >> 6)];
        *dst++ = alphabet[src[2] & 0x3f];
        src += 3;
        len -= 3;
    }
    if (!len) return;

    *dst++ = alphabet[(src[0] >> 2) & 0x3f];
    if (len == 1) {
        *dst++ = alphabet[(src[0] & 3) << 4];
        *dst++ = '=';
    } else {
        *dst++ = alphabet[((src[0] & 3) << 4) | (src[1] >> 4)];
        *dst++ = alphabet[(src[1] & 0x0f) << 2];
    }
    *dst++ = '=';
}


static size_t
iovec_len(const struct iovec* io, int count)
{
    int     i;
    size_t acc = 0;

    for(i = 0; i < count; i++) acc += io[i].iov_len;
    return acc;
}


ssize_t
safe_writev(int fd, struct iovec *iov, int iovcnt)
{
    unsigned int row, col;
    ssize_t rc, len, left;

    row = 0, col = 0;
    len = iovec_len(iov, iovcnt);
    left = len;

    while(1) {
        rc = writev(fd, iov + row, iovcnt - row);
        if (rc < 0 && errno == EINTR)
            continue;

        /* Undo any modifications done to the iovec structure in the previous
         * iteration. */
        if (col) {
            iov[row].iov_base -= col;
            iov[row].iov_len += col;
            col = 0;
        }

        if (rc < 0) return rc;

        left -= rc;
        if (!left) break;

        /* Adjust the iovec structure for the next call */
        while(rc > 0) {
            if ((size_t)rc >= iov[row].iov_len) {
                rc -= iov[row].iov_len;
                row++;
            } else {
                col = rc;
                iov[row].iov_base += col;
                iov[row].iov_len -= col;
                break;
            }
        }
    }

    return len;
}


int
make_nonblocking(int fd)
{
    int flags;

    flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return flags;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}


/* FIXME: Check for more invalid UTF-8 sequences */
int
chr2utf8(char *buf, size_t *len, uint16_t c)
{
    if (c <= 0x7f) {
        buf[0] = (char)c;
        *len = 1;
    } else if (c <= 0x7ff) {
        buf[0] = (char)(0xc0 | (c >> 6));
        buf[1] = (char)(0x80 | (c & 0x3f));
        *len = 2;
    } else {
        if (c == 0xfffe || c == 0xffff) return -1;
        buf[0] = (char)(0xe0 | (c >> 12));
        buf[1] = (char)(0x80 | ((c >> 6) & 0x3f));
        buf[2] = (char)(0x80 | (c & 0x3f));
        *len = 3;
    }
    return 0;
}

#ifdef __OS_darwin

/* FIXME: The following functions are not very efficient */

uint64_t
htobe64(uint64_t in)
{
#ifdef SMOB_LITTLE_ENDIAN
    return (uint64_t)htonl((in & 0xffffffff00000000UL) >> 32UL)
        + ((uint64_t)htonl(in & 0x00000000ffffffffUL) << 32UL);
#else
    return in;
#endif
}


uint64_t
be64toh(uint64_t in)
{
#ifdef SMOB_LITTLE_ENDIAN
    return (uint64_t)ntohl((in & 0xffffffff00000000UL) >> 32UL)
        + ((uint64_t)ntohl(in & 0x00000000ffffffffUL) << 32UL);
#else
    return in;
#endif
}


uint64_t
htole64(uint64_t in)
{
#ifdef SMOB_BIG_ENDIAN
    return (uint64_t)ntohl((in & 0xffffffff00000000UL) >> 32UL)
        + ((uint64_t)ntohl(in & 0x00000000ffffffffUL) << 32L);
#else
    return in;
#endif
}


uint64_t
le64toh(uint64_t in)
{
#ifdef SMOB_BIG_ENDIAN
    return (uint64_t)htonl((in & 0xffffffff00000000UL) >> 32UL)
        + ((uint64_t)htonl(in & 0x00000000ffffffffL) << 32UL);
#else
    return in;
#endif
}

#endif /* __OS_darwin */



int
make_path(const char *path, mode_t mode)
{
    int len;
    char *str, *p, *dir;

    assert(path && *path);

    len = strlen(path);
    str = p = xstrndup(path, len + 1);
    do {
        dir = memchr(p, '/', len - (p - str));
        if (dir) *dir = '\0';
        if (strlen(str) == 0) goto skip;
        if (strcmp(str, ".") == 0) goto skip;
        if (strcmp(str, "..") == 0) goto skip;

        if (mkdir(str, mode) == -1) {
            if (errno != EEXIST) {
                xfree(str);
                return -1;
            }
        }
    skip:
        if (dir) {
            *dir = '/';
            p = dir + 1;
        }
    } while (dir != NULL);

    xfree(str);
    return 0;
}


int
ms2iso(char *buf, size_t size, uint64_t ms)
{
    int rv;
    size_t len;
    struct tm t;
    time_t sec;

    sec = ms / 1000;
    memset(&t, 0, sizeof(struct tm));
    gmtime_r(&sec, &t);

    rv = strftime(buf, size, "%FT%T", &t);
    if (rv == 0) return 0;
    size -= rv;
    len = rv;

    if (ms % 1000) {
        rv = snprintf(buf + len, size, ".%03lldZ", ms % 1000);
        if (rv < 0) return rv;
        if (rv >= size) return -1;
        return len + rv;
    } else {
        if (size < 2) return -1;
        buf[len++] = 'Z';
        buf[len] = '\0';
        return len;
    }
}
