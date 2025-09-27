#ifndef GLIB_WASM_ICONV_H
#define GLIB_WASM_ICONV_H

#include <stddef.h>

typedef struct _WasmIconv *iconv_t;

iconv_t iconv_open (const char *to_code, const char *from_code);
size_t  iconv      (iconv_t cd,
                    char **inbuf,
                    size_t *inbytesleft,
                    char **outbuf,
                    size_t *outbytesleft);
int     iconv_close(iconv_t cd);

#endif /* GLIB_WASM_ICONV_H */
