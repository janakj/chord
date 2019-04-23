#ifndef _CHORD_DEFS_H_
#define _CHORD_DEFS_H_

/*
 * GNU gcc defines __BYTE_ORDER__
 * clang defines __LITTLE_ENDIAN__ or __BIG_ENDIAN__
 */

#if defined __BYTE_ORDER__
#  if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#    define SMOB_LITTLE_ENDIAN 1
#  elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#    define SMOB_BIG_ENDIAN 1
#  else
#    error "Unknown byte order."
#  endif
#elif defined __LITTLE_ENDIAN__
#  define SMOB_LITTLE_ENDIAN 1
#elif defined __BIG_ENDIAN__
#  define SMOB_BIG_ENDIAN 1
#else
#  error "Unknown byte order."
#endif

#endif /* _CHORD_DEFS_H_ */
