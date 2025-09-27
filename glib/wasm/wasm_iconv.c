/*
 * wasm_iconv.c - Minimal iconv implementation for WebAssembly targets.
 *
 * This stub keeps the GLib API contract for iconv-based conversions, while
 * assuming that all character data is UTF-8 (the enforced runtime encoding in
 * Discere OS).  Any unsupported conversions fail with EINVAL so that callers
 * can surface meaningful errors.
 */

#include "config.h"
#include "glib.h"
#include "wasm/wasm_iconv.h"

#include <errno.h>
#include <ctype.h>
#include <string.h>

typedef struct _WasmIconv
{
  gchar *to;
  gchar *from;
} WasmIconv;

static gchar *
canonicalize_encoding (const gchar *encoding)
{
  gsize len = strlen (encoding);
  gchar *canon = g_malloc (len + 1);
  gsize j = 0;

  for (gsize i = 0; i < len; i++)
    {
      gchar c = encoding[i];

      if (g_ascii_isalnum (c))
        canon[j++] = g_ascii_toupper (c);
    }

  canon[j] = '\0';
  return canon;
}

static gboolean
encodings_compatible (const WasmIconv *desc)
{
  /* Treat "UTF" and "UTF8" equivalently so that callers can use
   * UTF-8, UTF8, or utf8 interchangeably.
   */
  if (g_str_equal (desc->to, desc->from))
    return TRUE;

  if ((g_strcmp0 (desc->to, "UTF8") == 0 && g_strcmp0 (desc->from, "UTF") == 0) ||
      (g_strcmp0 (desc->from, "UTF8") == 0 && g_strcmp0 (desc->to, "UTF") == 0))
    return TRUE;

  return FALSE;
}

iconv_t
iconv_open (const char *to_code, const char *from_code)
{
  if (to_code == NULL || from_code == NULL)
    {
      errno = EINVAL;
      return (iconv_t) -1;
    }

  WasmIconv *desc = g_new0 (WasmIconv, 1);
  desc->to = canonicalize_encoding (to_code);
  desc->from = canonicalize_encoding (from_code);

  if (!encodings_compatible (desc))
    {
      g_free (desc->to);
      g_free (desc->from);
      g_free (desc);
      errno = EINVAL;
      return (iconv_t) -1;
    }

  return (iconv_t) desc;
}

size_t
iconv (iconv_t cd,
       char    **inbuf,
       size_t  *inbytesleft,
       char    **outbuf,
       size_t  *outbytesleft)
{
  WasmIconv *desc = (WasmIconv *) cd;

  if (desc == NULL)
    {
      errno = EINVAL;
      return (size_t) -1;
    }

  if (inbuf == NULL || *inbuf == NULL || inbytesleft == NULL)
    return 0;

  if (outbuf == NULL || *outbuf == NULL || outbytesleft == NULL)
    {
      errno = EINVAL;
      return (size_t) -1;
    }

  if (!encodings_compatible (desc))
    {
      errno = EINVAL;
      return (size_t) -1;
    }

  size_t needed = *inbytesleft;

  if (needed == 0)
    return 0;

  if (*outbytesleft < needed)
    {
      memcpy (*outbuf, *inbuf, *outbytesleft);
      *inbuf += *outbytesleft;
      *inbytesleft -= *outbytesleft;
      *outbuf += *outbytesleft;
      *outbytesleft = 0;
      errno = E2BIG;
      return (size_t) -1;
    }

  memcpy (*outbuf, *inbuf, needed);
  *inbuf += needed;
  *outbuf += needed;
  *inbytesleft = 0;
  *outbytesleft -= needed;

  return 0;
}

int
iconv_close (iconv_t cd)
{
  WasmIconv *desc = (WasmIconv *) cd;

  if (desc == NULL)
    {
      errno = EINVAL;
      return -1;
    }

  g_free (desc->to);
  g_free (desc->from);
  g_free (desc);

  return 0;
}
