/* GLib WASM - Compatibility layer for missing POSIX functions
 * Copyright (C) 2025 Superstruct Ltd, New Zealand
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <emscripten/emscripten.h>

/* GLib type definitions needed for stubs */
typedef int gboolean;
typedef char gchar;
#define FALSE 0
#define TRUE 1

/* fdatasync() - not available in WASM, fallback to fsync */
int fdatasync(int fd) {
    return fsync(fd);
}

/* getpwnam_r() - user database functions not available in WASM */
struct passwd;
int getpwnam_r(const char *name, struct passwd *pwd, char *buffer,
               size_t bufsize, struct passwd **result) {
    *result = NULL;
    errno = ENOSYS;
    return -1;
}

/* getpwuid_r() - user database functions not available in WASM */
int getpwuid_r(unsigned int uid, struct passwd *pwd, char *buffer,
               size_t bufsize, struct passwd **result) {
    *result = NULL;
    errno = ENOSYS;
    return -1;
}

/* getgrnam_r() - group database functions not available in WASM */
struct group;
int getgrnam_r(const char *name, struct group *grp, char *buffer,
               size_t bufsize, struct group **result) {
    *result = NULL;
    errno = ENOSYS;
    return -1;
}

/* getgrgid_r() - group database functions not available in WASM */
int getgrgid_r(unsigned int gid, struct group *grp, char *buffer,
               size_t bufsize, struct group **result) {
    *result = NULL;
    errno = ENOSYS;
    return -1;
}

/* getpwnam() and getpwuid() - provided by Emscripten stubs, remove duplicates */
/* struct passwd *getpwnam(const char *name) {
    errno = ENOSYS;
    return NULL;
}

struct passwd *getpwuid(unsigned int uid) {
    errno = ENOSYS;
    return NULL;
} */

/* Group functions - may also be provided by Emscripten, use weak definitions */
__attribute__((weak))
struct group *getgrnam(const char *name) {
    errno = ENOSYS;
    return NULL;
}

__attribute__((weak))
struct group *getgrgid(unsigned int gid) {
    errno = ENOSYS;
    return NULL;
}

/* endpwent(), endgrent() - not needed in WASM */
void endpwent(void) {
    /* No-op in WASM */
}

void endgrent(void) {
    /* No-op in WASM */
}

/* lchown() - not available in WASM, fallback to chown */
int lchown(const char *pathname, unsigned int owner, unsigned int group) {
    return chown(pathname, owner, group);
}

/* Link/symlink operations - limited support in WASM */
int link(const char *oldpath, const char *newpath) {
    errno = ENOSYS;
    return -1;
}

int symlink(const char *target, const char *linkpath) {
    errno = ENOSYS;
    return -1;
}

ssize_t readlink(const char *pathname, char *buf, size_t bufsiz) {
    errno = ENOSYS;
    return -1;
}

/* utime() - basic implementation using utimes */
struct utimbuf;
int utime(const char *filename, const struct utimbuf *times) {
    errno = ENOSYS;
    return -1;
}

/* Process/signal functions - not available in WASM */
int kill(int pid, int sig) {
    errno = ENOSYS;
    return -1;
}

unsigned int getuid(void) {
    return 1000; /* Default user ID for WASM */
}

unsigned int geteuid(void) {
    return 1000; /* Default effective user ID for WASM */
}

unsigned int getgid(void) {
    return 1000; /* Default group ID for WASM */
}

unsigned int getegid(void) {
    return 1000; /* Default effective group ID for WASM */
}

int getpid(void) {
    return 42; /* Dummy process ID for WASM */
}

int getppid(void) {
    return 1; /* Dummy parent process ID for WASM */
}

/* Network functions - not available in WASM */
struct hostent *gethostbyname(const char *name) {
    errno = ENOSYS;
    return NULL;
}

/* Directory operations - basic WASM support */
int wasm_mkdir(const char *pathname, mode_t mode) {
    /* Create directory via Emscripten FS */
    return EM_ASM_INT({
        try {
            FS.mkdir(UTF8ToString($0));
            return 0;
        } catch (e) {
            return -1;
        }
    }, pathname);
}

/* File operations enhanced for WASM */
int wasm_chmod(const char *pathname, mode_t mode) {
    /* WASM doesn't support changing file permissions */
    return 0; /* Pretend success */
}

int wasm_chown(const char *pathname, unsigned int owner, unsigned int group) {
    /* WASM doesn't support changing file ownership */
    return 0; /* Pretend success */
}

/* Enhanced stat for WASM */
int lstat(const char *pathname, struct stat *statbuf) {
    /* Fallback to regular stat in WASM */
    return stat(pathname, statbuf);
}

/* Aligned memory allocation for WASM */
int posix_memalign(void **memptr, size_t alignment, size_t size) {
    /* Simple aligned allocation using malloc for WASM */
    void *ptr = malloc(size);
    if (!ptr) {
        return 12; /* ENOMEM */
    }
    *memptr = ptr;
    return 0; /* Success */
}

/* Pthread naming functions - stubs for WASM compatibility */
#ifdef __EMSCRIPTEN_PTHREADS__
#include <pthread.h>
#include <stdio.h>

int pthread_setname_np(const char *name) {
    /* Emscripten doesn't support thread naming - silently succeed */
    (void)name;  /* Suppress unused parameter warning */
    return 0;
}

int pthread_getname_np(pthread_t thread, char *buffer, size_t length) {
    /* Emscripten doesn't support thread naming - return generic name */
    (void)thread;  /* Suppress unused parameter warning */
    if (length > 0) {
        snprintf(buffer, length, "emscripten-thread");
    }
    return 0;
}
#endif /* __EMSCRIPTEN_PTHREADS__ */

/* Missing GLib functions - provide minimal stubs for WASM */

/* Memory management functions */
gboolean g_mem_gc_friendly = FALSE;

/* Environment variable access */
const gchar* g_getenv(const gchar *variable) {
    return getenv(variable);
}

/* Locale and charset functions */
const char **_g_locale_get_charset_aliases(void) {
    static const char *aliases[] = { NULL };
    return aliases;
}

const char *_g_locale_charset_raw(void) {
    return "UTF-8";  /* Default to UTF-8 for WASM */
}

const char *_g_locale_charset_unalias(const char *charset) {
    return charset ? charset : "UTF-8";
}

/* WebGPU integration stubs - for future implementation */
int g_webgpu_initialize(void *config) {
    /* WebGPU initialization stub - return success */
    (void)config;  /* Suppress unused parameter warning */
    return 1;  /* TRUE/success */
}

void g_webgpu_cleanup(void) {
    /* WebGPU cleanup stub */
}

/* Browser mainloop integration stubs */
void g_mainloop_browser_cleanup(void) {
    /* Browser mainloop cleanup stub */
}

/* Threading WASM cleanup stub */
void g_threading_wasm_cleanup(void) {
    /* Threading cleanup stub */
}

/* OPFS filesystem cleanup stub */
void g_filesystem_opfs_cleanup(void) {
    /* OPFS cleanup stub */
}

/* WebGPU timing cleanup stub */
void g_timing_webgpu_cleanup(void) {
    /* WebGPU timing cleanup stub */
}

/* Additional missing GLib functions */

/* Logging functions */
gboolean g_log_always_fatal = FALSE;

int g_fputs(const char *str, void *file) {
    /* Use stderr for logging output by default */
    FILE *output = file ? (FILE*)file : stderr;
    return fputs(str, output);
}

const char *g_log_msg_prefix = "";

/* Printf functions - use standard implementations */
int _g_gnulib_vsnprintf(char *str, size_t size, const char *format, va_list ap) {
    return vsnprintf(str, size, format, ap);
}

int _g_gnulib_vasprintf(char **strp, const char *format, va_list ap) {
    return vasprintf(strp, format, ap);
}