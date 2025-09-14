/* GLib WASM - OPFS filesystem implementation
 * Copyright (C) 2025 Superstruct Ltd, New Zealand
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <glib.h>
#include <emscripten/emscripten.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

// OPFS integration using EM_JS
EM_JS(int, opfs_is_available, (), {
    return (typeof navigator.storage !== 'undefined' &&
            typeof navigator.storage.getDirectory === 'function') ? 1 : 0;
});

EM_JS(void, opfs_initialize, (), {
    if (typeof Module.opfsRoot === 'undefined') {
        navigator.storage.getDirectory().then(root => {
            Module.opfsRoot = root;
            console.log('OPFS initialized for GLib filesystem');
        }).catch(err => {
            console.warn('OPFS initialization failed:', err);
            Module.opfsRoot = null;
        });
    }
});

EM_JS(int, opfs_file_exists, (const char* filename), {
    if (!Module.opfsRoot) return 0;

    const name = UTF8ToString(filename);
    return Asyncify.handleAsync(async () => {
        try {
            await Module.opfsRoot.getFileHandle(name, { create: false });
            return 1;
        } catch (e) {
            return 0;
        }
    });
});

EM_JS(int, opfs_write_file, (const char* filename, const char* data, int length), {
    if (!Module.opfsRoot) return 0;

    const name = UTF8ToString(filename);
    const content = UTF8ToString(data);

    return Asyncify.handleAsync(async () => {
        try {
            const fileHandle = await Module.opfsRoot.getFileHandle(name, { create: true });
            const writable = await fileHandle.createWritable();
            await writable.write(content);
            await writable.close();
            return length;
        } catch (e) {
            console.error('OPFS write failed:', e);
            return -1;
        }
    });
});

EM_JS(int, opfs_read_file, (const char* filename, char* buffer, int buffer_size), {
    if (!Module.opfsRoot) return -1;

    const name = UTF8ToString(filename);

    return Asyncify.handleAsync(async () => {
        try {
            const fileHandle = await Module.opfsRoot.getFileHandle(name, { create: false });
            const file = await fileHandle.getFile();
            const text = await file.text();

            if (text.length > buffer_size - 1) {
                return -1; // Buffer too small
            }

            stringToUTF8(text, buffer, buffer_size);
            return text.length;
        } catch (e) {
            console.error('OPFS read failed:', e);
            return -1;
        }
    });
});

EM_JS(int, opfs_delete_file, (const char* filename), {
    if (!Module.opfsRoot) return -1;

    const name = UTF8ToString(filename);

    return Asyncify.handleAsync(async () => {
        try {
            await Module.opfsRoot.removeEntry(name);
            return 0;
        } catch (e) {
            console.error('OPFS delete failed:', e);
            return -1;
        }
    });
});

EM_JS(int, opfs_create_directory, (const char* dirname), {
    if (!Module.opfsRoot) return -1;

    const name = UTF8ToString(dirname);

    return Asyncify.handleAsync(async () => {
        try {
            await Module.opfsRoot.getDirectoryHandle(name, { create: true });
            return 0;
        } catch (e) {
            console.error('OPFS mkdir failed:', e);
            return -1;
        }
    });
});

// Global filesystem state
static gboolean g_filesystem_initialized = FALSE;
static gboolean g_opfs_available = FALSE;

static void g_filesystem_init(void) {
    if (g_filesystem_initialized) return;

    g_filesystem_initialized = TRUE;
    g_opfs_available = opfs_is_available();

    if (g_opfs_available) {
        opfs_initialize();
        g_message("GLib filesystem initialized with OPFS support");
    } else {
        g_message("GLib filesystem initialized with virtual filesystem only");
    }
}

// Enhanced g_file_set_contents with OPFS
gboolean g_file_set_contents(const gchar *filename,
                            const gchar *contents,
                            gssize length,
                            GError **error) {
    g_return_val_if_fail(filename != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    g_filesystem_init();

    if (length == -1) {
        length = strlen(contents);
    }

    // Try OPFS first if available
    if (g_opfs_available) {
        int result = opfs_write_file(filename, contents, length);
        if (result >= 0) {
            return TRUE;
        }
        // Fall through to virtual filesystem on OPFS failure
    }

    // Fallback to standard virtual filesystem
    FILE *file = fopen(filename, "wb");
    if (!file) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                   "Failed to open file '%s': %s", filename, strerror(errno));
        return FALSE;
    }

    size_t bytes_written = fwrite(contents, 1, length, file);
    fclose(file);

    if (bytes_written != (size_t)length) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                   "Failed to write complete contents to '%s'", filename);
        return FALSE;
    }

    return TRUE;
}

// Enhanced g_file_get_contents with OPFS
gboolean g_file_get_contents(const gchar *filename,
                            gchar **contents,
                            gsize *length,
                            GError **error) {
    g_return_val_if_fail(filename != NULL, FALSE);
    g_return_val_if_fail(contents != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    g_filesystem_init();

    // Try OPFS first if available
    if (g_opfs_available) {
        // Allocate buffer for OPFS read
        char *buffer = g_malloc(65536); // 64KB max file size for OPFS
        int bytes_read = opfs_read_file(filename, buffer, 65536);

        if (bytes_read >= 0) {
            *contents = g_realloc(buffer, bytes_read + 1);
            (*contents)[bytes_read] = '\0';
            if (length) *length = bytes_read;
            return TRUE;
        }

        g_free(buffer);
        // Fall through to virtual filesystem on OPFS failure
    }

    // Fallback to standard virtual filesystem
    FILE *file = fopen(filename, "rb");
    if (!file) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                   "Failed to open file '%s': %s", filename, strerror(errno));
        return FALSE;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size < 0) {
        fclose(file);
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                   "Failed to get size of file '%s'", filename);
        return FALSE;
    }

    // Allocate buffer and read
    *contents = g_malloc(file_size + 1);
    size_t bytes_read = fread(*contents, 1, file_size, file);
    fclose(file);

    if (bytes_read != (size_t)file_size) {
        g_free(*contents);
        *contents = NULL;
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                   "Failed to read complete contents of '%s'", filename);
        return FALSE;
    }

    (*contents)[bytes_read] = '\0';
    if (length) *length = bytes_read;

    return TRUE;
}

// Enhanced g_file_test with OPFS
gboolean g_file_test(const gchar *filename, GFileTest test) {
    g_return_val_if_fail(filename != NULL, FALSE);

    g_filesystem_init();

    if (test & G_FILE_TEST_EXISTS) {
        // Try OPFS first
        if (g_opfs_available && opfs_file_exists(filename)) {
            return TRUE;
        }

        // Fallback to virtual filesystem
        struct stat stat_buf;
        if (stat(filename, &stat_buf) == 0) {
            return TRUE;
        }
    }

    if (test & G_FILE_TEST_IS_REGULAR) {
        struct stat stat_buf;
        if (stat(filename, &stat_buf) == 0) {
            return S_ISREG(stat_buf.st_mode);
        }
    }

    if (test & G_FILE_TEST_IS_DIR) {
        struct stat stat_buf;
        if (stat(filename, &stat_buf) == 0) {
            return S_ISDIR(stat_buf.st_mode);
        }
    }

    return FALSE;
}

// Enhanced g_unlink with OPFS
int g_unlink(const gchar *filename) {
    g_return_val_if_fail(filename != NULL, -1);

    g_filesystem_init();

    // Try OPFS first
    if (g_opfs_available) {
        int result = opfs_delete_file(filename);
        if (result == 0) {
            return 0;
        }
    }

    // Fallback to virtual filesystem
    return unlink(filename);
}

// Enhanced g_mkdir_with_parents with OPFS
gboolean g_mkdir_with_parents(const gchar *pathname, gint mode) {
    g_return_val_if_fail(pathname != NULL, FALSE);

    g_filesystem_init();

    // Try OPFS first
    if (g_opfs_available) {
        // Split path and create directories recursively
        gchar **path_parts = g_strsplit(pathname, "/", -1);
        gchar *current_path = g_strdup("");

        for (gchar **part = path_parts; *part != NULL; part++) {
            if (**part == '\0') continue; // Skip empty parts

            gchar *new_path = g_build_filename(current_path, *part, NULL);
            g_free(current_path);
            current_path = new_path;

            if (opfs_create_directory(current_path) < 0) {
                // Directory might already exist, continue
            }
        }

        g_strfreev(path_parts);
        g_free(current_path);
        return TRUE;
    }

    // Fallback to standard implementation
    gchar **path_parts = g_strsplit(pathname, G_DIR_SEPARATOR_S, -1);
    gchar *current_path = g_strdup("");

    for (gchar **part = path_parts; *part != NULL; part++) {
        if (**part == '\0') continue;

        gchar *new_path = g_build_filename(current_path, *part, NULL);
        g_free(current_path);
        current_path = new_path;

        struct stat stat_buf;
        if (stat(current_path, &stat_buf) != 0) {
            if (g_mkdir(current_path, mode) != 0 && errno != EEXIST) {
                g_strfreev(path_parts);
                g_free(current_path);
                return FALSE;
            }
        }
    }

    g_strfreev(path_parts);
    g_free(current_path);
    return TRUE;
}

// Path manipulation utilities (already work with virtual filesystem)
gboolean g_path_is_absolute(const gchar *file_name) {
    g_return_val_if_fail(file_name != NULL, FALSE);

    return (file_name[0] == G_DIR_SEPARATOR ||
            (g_ascii_isalpha(file_name[0]) && file_name[1] == ':' && file_name[2] == G_DIR_SEPARATOR));
}

gchar* g_build_filename(const gchar *first_element, ...) {
    gchar *str;
    va_list args;

    g_return_val_if_fail(first_element != NULL, NULL);

    va_start(args, first_element);
    str = g_build_filename_valist(first_element, &args);
    va_end(args);

    return str;
}

gchar* g_build_filename_valist(const gchar *first_element, va_list *args) {
    gchar *str;
    GPtrArray *array;
    const gchar *element;

    g_return_val_if_fail(first_element != NULL, NULL);

    array = g_ptr_array_new();
    g_ptr_array_add(array, (gpointer)first_element);

    element = va_arg(*args, gchar*);
    while (element) {
        g_ptr_array_add(array, (gpointer)element);
        element = va_arg(*args, gchar*);
    }

    g_ptr_array_add(array, NULL);
    str = g_build_filenamev((gchar**)array->pdata);
    g_ptr_array_free(array, TRUE);

    return str;
}

gchar* g_build_filenamev(gchar **args) {
    GString *result = g_string_new("");
    gchar **arg;

    for (arg = args; *arg != NULL; arg++) {
        if (result->len > 0 && result->str[result->len - 1] != G_DIR_SEPARATOR) {
            g_string_append_c(result, G_DIR_SEPARATOR);
        }

        // Remove leading separators from subsequent elements
        const gchar *element = *arg;
        while (*element == G_DIR_SEPARATOR && result->len > 0) {
            element++;
        }

        g_string_append(result, element);
    }

    return g_string_free(result, FALSE);
}

// Cleanup function
void g_filesystem_opfs_cleanup(void) {
    // OPFS cleanup happens automatically in browser
    g_filesystem_initialized = FALSE;
    g_opfs_available = FALSE;
}