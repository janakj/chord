#include "str.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "utils.h"
#include "log.h"

#define SBUF_BUF_END(b) ((b)->buffer.s + (b)->buffer.len)
#define SBUF_STR_END(b) ((b)->data.s + (b)->data.len)

size_t
str_memleft(sbuf *b)
{
    assert(b);
    return SBUF_BUF_END(b) - SBUF_STR_END(b) - 1;
}


sbuf*
str_new(size_t size)
{
    sbuf *t;

    t = xmalloc(sizeof(*t));
    t->buffer.s = xmalloc(size + 1);
    t->data.s = t->buffer.s;
    t->data.len = 0;
    t->buffer.len = size + 1;
    return t;
}


sbuf*
str_dup(const char *s, size_t len)
{
    sbuf *b;

    b = str_new(len);
    if (len) {
        str_addl(b, s, len);
    }
    return b;
}


void
str_free(sbuf *b)
{
    if (!b) return;
    if (b->buffer.s) xfree(b->buffer.s);
    xfree(b);
}


sbuf *
str_reset(sbuf *b)
{
    assert(b);
    b->data.s = b->buffer.s;
    b->data.len = 0;
    return b;
}


char *
str_get(sbuf *b)
{
    assert(b);
    b->data.s[b->data.len] = '\0';
    return b->data.s;
}


/* TODO: review this function */
char *
str_getmem(sbuf *b, size_t len)
{
    size_t left;

    assert(b);
    left = str_memleft(b);
    if (left < len) str_resize(b, b->buffer.len + len - left);
    return SBUF_STR_END(b);
}


sbuf *
str_resize(sbuf *b, size_t new)
{
    char *tmp;

    assert(b);
    assert(new);
    tmp = xrealloc(b->buffer.s, new + 1);
    b->data.s = tmp + (b->data.s - b->buffer.s);
    b->buffer.s = tmp;
    b->buffer.len = new + 1;
    return b;
}


size_t
str_len(sbuf *b)
{
    assert(b);
    return b->data.len;
}


static int
is_wsp(char c)
{
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}


str *
str_triml(str *s)
{
    assert(s);
    while(s->len && is_wsp(*s->s)) {
        s->s++;
        s->len--;
    }
    return s;
}


str *
str_trimt(str *s)
{
    assert(s);
    while(s->len && is_wsp(s->s[s->len - 1])) {
        s->len--;
    }
    return s;
}


str *
str_trim(str *s)
{
    assert(s);
    return str_trimt(str_triml(s));
}


str *
str_unquote(str *s)
{
    assert(s);

    s = str_trim(s);
    if (s->len >= 2 && s->s[0] == '"' && s->s[s->len - 1] == '"') {
        s->s++;
        s->len -= 2;
    }
    return s;
}


sbuf *
str_add(sbuf *b, const char *s)
{
    assert(b);
    assert(s);

    return str_addl(b, s, strlen(s));
}


sbuf *
str_addc(sbuf *b, int c)
{
    char *p;

    assert(b);
    p = str_getmem(b, 1);
    *p = (char)c;
    b->data.len += 1;
    return b;
}


sbuf *
str_addl(sbuf *b, const char *s, size_t len)
{
    char *p;

    assert(b);
    assert(s);

    p = str_getmem(b, len);
    memcpy(p, s, len);
    b->data.len += len;
    return b;
}


sbuf *
str_adds(sbuf *b, str *s)
{
    assert(b);
    assert(s);
    return str_addl(b, s->s, s->len);
}


sbuf *
str_addv(sbuf *b, const char *fmt, va_list args)
{
    va_list a;
    char *buf;
    int len, rc;

    assert(b);
    assert(fmt);

    len = strlen(fmt) + 32;
    buf = str_getmem(b, len);

    while(1) {
        va_copy(a, args);
        rc = vsnprintf(buf, len, fmt, a);
        va_end(a);
        if (rc >= 0 && rc < len) {
            b->data.len += rc;
            break;
        } else if (rc >= 0) {
            len = rc + 1;
            buf = str_getmem(b, rc + 1);
        } else {
            ERR("Error in vsnprintf");
            break;
        }
    }
    return b;
}


sbuf *
str_addf(sbuf *b, const char *fmt, ...)
{
    va_list ap;

    assert(b);
    assert(fmt);
    va_start(ap, fmt);
    b = str_addv(b, fmt, ap);
    va_end(ap);
    return b;
}


sbuf *
str_print(sbuf *b, const char *s)
{
    assert(b);
    assert(s);
    return str_printl(b, s, strlen(s));
}


sbuf *
str_printl(sbuf *b, const char *s, size_t len)
{
    assert(b);
    assert(s);
    str_reset(b);
    return str_addl(b, s, len);
}


sbuf *
str_prints(sbuf *b, str *s)
{
    assert(b);
    assert(s);
    return str_printl(b, s->s, s->len);
}


sbuf *
str_printv(sbuf *b, const char *fmt, va_list args)
{
    assert(b);
    assert(fmt);
    str_reset(b);
    return str_addv(b, fmt, args);
}


sbuf *
str_printf(sbuf *b, const char *fmt, ...)
{
    va_list ap;

    assert(b);
    assert(fmt);
    va_start(ap, fmt);
    b = str_printv(b, fmt, ap);
    va_end(ap);
    return b;
}


int
str_cmp(str *x, str *y)
{
    assert(x);
    assert(y);
    if (x->len != y->len) return x->len - y->len;
    return memcmp(x->s, y->s, x->len);
}


str *
str_wrap(str *s, const char *c)
{
    if (!s) return NULL;
    s->s = (char *)c;
    s->len = c ? strlen(c) : 0;
    return s;
}
