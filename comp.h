#ifndef _COMP_H_
#define _COMP_H_

#include <stdlib.h>

int comp_shrink(char **dst, size_t *dlen, char *packet, size_t len);

int comp_expand(char **dst, size_t *dlen, char *packet, size_t len);

int comp_init();

void comp_cleanup();

#endif /* _COMP_H_ */
