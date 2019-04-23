#ifndef _STR_H_
#define _STR_H_

#include <string.h>
#include <stdarg.h>
#include <assert.h>

#define STR_NULL       {NULL, 0}
#define STR_INIT(s)    {s, sizeof(s) - 1}

#define STR_FMT(ps)   (int)((ps)->len), (ps)->s
#define STR_ARG(ps)   ((ps) ? (ps)->s : ""), ((ps) ? (ps)->len : 0)
#define STR_EMPTY(ps) (!((ps)->s && (ps)->len))

typedef struct str str;
typedef struct sbuf sbuf;


struct str {
    char* s;
    size_t len;
};


struct sbuf {
    str buffer;
    str data;
};

str   *str_trim (str *s);
str   *str_triml(str *s);
str   *str_trimt(str *s);
str   *str_unquote(str *s);
int    str_cmp  (str *x, str *y);
str   *str_wrap (str *s, const char *c);

sbuf  *str_new    (size_t size);
sbuf  *str_dup    (const char *c, size_t len);

void   str_free   (sbuf *b);
sbuf  *str_reset  (sbuf *b);
sbuf  *str_resize (sbuf *b, size_t new);
char  *str_get    (sbuf *b);
char  *str_getmem (sbuf *b, size_t len);
size_t str_memleft(sbuf *b);
size_t str_len    (sbuf *b);

sbuf  *str_add    (sbuf *b, const char *s);
sbuf  *str_addc   (sbuf *b, int c);
sbuf  *str_addl   (sbuf *b, const char *s, size_t len);
sbuf  *str_adds   (sbuf *b, str *s);
sbuf  *str_addv   (sbuf *b, const char *fmt, va_list args);
sbuf  *str_addf   (sbuf *b, const char *fmt, ...)
    __attribute__ ((format (printf, 2, 3)));

sbuf  *str_print  (sbuf *b, const char *s);
sbuf  *str_printl (sbuf *b, const char *s, size_t len);
sbuf  *str_prints (sbuf *b, str *s);
sbuf  *str_printv (sbuf *b, const char *fmt, va_list args);
sbuf  *str_printf (sbuf *b, const char *fmt, ...)
    __attribute__ ((format (printf, 2, 3)));

static inline str *
str_null(str *s)
{
    if (s) {
        s->s = NULL;
        s->len = 0;
    }
    return s;
}

static inline sbuf *
str_dups(str *s)
{
    assert(s);
    return str_dup(s->s, s->len);
}

static inline sbuf *
str_dupl(const char *s, size_t len)
{
    assert(s);
    return str_dup(s, len);
}

static inline sbuf *
str_dupz(const char *c)
{
    assert(c);
    return str_dup(c, strlen(c));
}

#endif /* _STR_H_ */
