/* glibconfig.h
 * This is a generated file for GLib WASM build.
 */

#ifndef __GLIBCONFIG_H__
#define __GLIBCONFIG_H__

#include <glib/gmacros.h>
#include <limits.h>
#include <float.h>

/* Static compilation for WASM */
#define GLIB_STATIC_COMPILATION 1
#define GOBJECT_STATIC_COMPILATION 1
#define GIO_STATIC_COMPILATION 1
#define GMODULE_STATIC_COMPILATION 1

/* System printf functions */
#define GLIB_USING_SYSTEM_PRINTF 1

G_BEGIN_DECLS

/* Basic type limits */
#define G_MINFLOAT	FLT_MIN
#define G_MAXFLOAT	FLT_MAX
#define G_MINDOUBLE	DBL_MIN
#define G_MAXDOUBLE	DBL_MAX
#define G_MINSHORT	SHRT_MIN
#define G_MAXSHORT	SHRT_MAX
#define G_MAXUSHORT	USHRT_MAX
#define G_MININT	INT_MIN
#define G_MAXINT	INT_MAX
#define G_MAXUINT	UINT_MAX
#define G_MINLONG	LONG_MIN
#define G_MAXLONG	LONG_MAX
#define G_MAXULONG	ULONG_MAX

/* Basic integer types */
typedef signed char gint8;
typedef unsigned char guint8;

typedef signed short gint16;
typedef unsigned short guint16;

#define G_GINT16_MODIFIER "h"
#define G_GINT16_FORMAT "hi"
#define G_GUINT16_FORMAT "hu"

typedef signed int gint32;
typedef unsigned int guint32;

#define G_GINT32_MODIFIER ""
#define G_GINT32_FORMAT "i"
#define G_GUINT32_FORMAT "u"

/* 64-bit integers */
#define G_HAVE_GINT64 1

typedef signed long long gint64;
typedef unsigned long long guint64;

#define G_GINT64_CONSTANT(val)	(val##LL)
#define G_GUINT64_CONSTANT(val)	(val##ULL)

#define G_GINT64_MODIFIER "ll"
#define G_GINT64_FORMAT "lli"
#define G_GUINT64_FORMAT "llu"

/* Size types for WASM (32-bit pointers) */
#define GLIB_SIZEOF_VOID_P 4
#define GLIB_SIZEOF_LONG   4
#define GLIB_SIZEOF_SIZE_T 4
#define GLIB_SIZEOF_SSIZE_T 4

typedef signed long gssize;
typedef unsigned long gsize;
#define G_GSIZE_MODIFIER "l"
#define G_GSSIZE_MODIFIER "l"
#define G_GSIZE_FORMAT "lu"
#define G_GSSIZE_FORMAT "li"

#define G_MAXSIZE	G_MAXULONG
#define G_MINSSIZE	G_MINLONG
#define G_MAXSSIZE	G_MAXLONG

/* Offset type */
typedef gint64 goffset;
#define G_MINOFFSET	G_MININT64
#define G_MAXOFFSET	G_MAXINT64

#define G_GOFFSET_MODIFIER      G_GINT64_MODIFIER
#define G_GOFFSET_FORMAT        G_GINT64_FORMAT
#define G_GOFFSET_CONSTANT(val) G_GINT64_CONSTANT(val)

/* Poll format */
#define G_POLLFD_FORMAT "%d"

/* Pointer conversion */
#define GPOINTER_TO_INT(p)	((gint)  (gintptr) (p))
#define GPOINTER_TO_UINT(p)	((guint) (guintptr) (p))
#define GINT_TO_POINTER(i)	((gpointer) (gintptr) (i))
#define GUINT_TO_POINTER(u)	((gpointer) (guintptr) (u))

/* Integer pointer types */
typedef signed int gintptr;
typedef unsigned int guintptr;

#define G_GINTPTR_MODIFIER      ""
#define G_GINTPTR_FORMAT        "i"
#define G_GUINTPTR_FORMAT       "u"

/* GLib version - matches CMake project version */
#define GLIB_MAJOR_VERSION 2
#define GLIB_MINOR_VERSION 85
#define GLIB_MICRO_VERSION 4

/* OS detection - WASM environment */
#define G_OS_UNIX 1

/* Variable argument support */
#define G_VA_COPY va_copy
#define G_HAVE_ISO_VARARGS 1
#define G_HAVE_GNUC_VARARGS 1

/* Threading support - WASM with Emscripten pthreads */
#define G_THREADS_ENABLED 1
#define G_THREADS_IMPL_POSIX 1

/* Atomic operations */
#define G_ATOMIC_LOCK_FREE 1

/* Byte order - little endian for WASM */
#define G_LITTLE_ENDIAN 1234
#define G_BIG_ENDIAN    4321
#define G_BYTE_ORDER    G_LITTLE_ENDIAN

/* Endianness conversion macros */
#define GINT16_TO_LE(val)	((gint16) (val))
#define GUINT16_TO_LE(val)	((guint16) (val))
#define GINT16_TO_BE(val)	((gint16) GUINT16_SWAP_LE_BE (val))
#define GUINT16_TO_BE(val)	(GUINT16_SWAP_LE_BE (val))

#define GINT32_TO_LE(val)	((gint32) (val))
#define GUINT32_TO_LE(val)	((guint32) (val))
#define GINT32_TO_BE(val)	((gint32) GUINT32_SWAP_LE_BE (val))
#define GUINT32_TO_BE(val)	(GUINT32_SWAP_LE_BE (val))

#define GINT64_TO_LE(val)	((gint64) (val))
#define GUINT64_TO_LE(val)	((guint64) (val))
#define GINT64_TO_BE(val)	((gint64) GUINT64_SWAP_LE_BE (val))
#define GUINT64_TO_BE(val)	(GUINT64_SWAP_LE_BE (val))

#define GLONG_TO_LE(val)	((glong) GINT32_TO_LE (val))
#define GULONG_TO_LE(val)	((gulong) GUINT32_TO_LE (val))
#define GLONG_TO_BE(val)	((glong) GINT32_TO_BE (val))
#define GULONG_TO_BE(val)	((gulong) GUINT32_TO_BE (val))
#define GINT_TO_LE(val)		((gint) GINT32_TO_LE (val))
#define GUINT_TO_LE(val)	((guint) GUINT32_TO_LE (val))
#define GINT_TO_BE(val)		((gint) GINT32_TO_BE (val))
#define GUINT_TO_BE(val)	((guint) GUINT32_TO_BE (val))
#define GSIZE_TO_LE(val)	((gsize) GUINT32_TO_LE (val))
#define GSSIZE_TO_LE(val)	((gssize) GINT32_TO_LE (val))
#define GSIZE_TO_BE(val)	((gsize) GUINT32_TO_BE (val))
#define GSSIZE_TO_BE(val)	((gssize) GINT32_TO_BE (val))

/* Poll constants */
#define GLIB_SYSDEF_POLLIN   =1
#define GLIB_SYSDEF_POLLOUT  =4
#define GLIB_SYSDEF_POLLPRI  =2
#define GLIB_SYSDEF_POLLHUP  =16
#define GLIB_SYSDEF_POLLERR  =8
#define GLIB_SYSDEF_POLLNVAL =32

/* Module suffix */
#define G_MODULE_SUFFIX "wasm"

/* Process ID type */
typedef int GPid;
#define G_PID_FORMAT "i"

/* Socket constants - not available in WASM */
#define GLIB_SYSDEF_AF_UNIX 1
#define GLIB_SYSDEF_AF_INET 2
#define GLIB_SYSDEF_AF_INET6 10

#define GLIB_SYSDEF_MSG_OOB 1
#define GLIB_SYSDEF_MSG_PEEK 2
#define GLIB_SYSDEF_MSG_DONTROUTE 4

/* Path separators */
#define G_DIR_SEPARATOR '/'
#define G_DIR_SEPARATOR_S "/"
#define G_SEARCHPATH_SEPARATOR ':'
#define G_SEARCHPATH_SEPARATOR_S ":"

G_END_DECLS

#endif /* __GLIBCONFIG_H__ */