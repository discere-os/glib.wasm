/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __G_VERSION_MACROS_H__
#define __G_VERSION_MACROS_H__

#if !defined(__GLIB_H_INSIDE__) && !defined(GLIB_COMPILATION)
#error "Only <glib.h> can be included directly."
#endif

/* Version boundaries checks */

#define G_ENCODE_VERSION(major, minor) ((major) << 16 | (minor) << 8)

/* Available versions */
#define GLIB_VERSION_2_26 (G_ENCODE_VERSION (2, 26))
#define GLIB_VERSION_2_28 (G_ENCODE_VERSION (2, 28))
#define GLIB_VERSION_2_30 (G_ENCODE_VERSION (2, 30))
#define GLIB_VERSION_2_32 (G_ENCODE_VERSION (2, 32))
#define GLIB_VERSION_2_34 (G_ENCODE_VERSION (2, 34))
#define GLIB_VERSION_2_36 (G_ENCODE_VERSION (2, 36))
#define GLIB_VERSION_2_38 (G_ENCODE_VERSION (2, 38))
#define GLIB_VERSION_2_40 (G_ENCODE_VERSION (2, 40))
#define GLIB_VERSION_2_42 (G_ENCODE_VERSION (2, 42))
#define GLIB_VERSION_2_44 (G_ENCODE_VERSION (2, 44))
#define GLIB_VERSION_2_46 (G_ENCODE_VERSION (2, 46))
#define GLIB_VERSION_2_48 (G_ENCODE_VERSION (2, 48))
#define GLIB_VERSION_2_50 (G_ENCODE_VERSION (2, 50))
#define GLIB_VERSION_2_52 (G_ENCODE_VERSION (2, 52))
#define GLIB_VERSION_2_54 (G_ENCODE_VERSION (2, 54))
#define GLIB_VERSION_2_56 (G_ENCODE_VERSION (2, 56))
#define GLIB_VERSION_2_58 (G_ENCODE_VERSION (2, 58))
#define GLIB_VERSION_2_60 (G_ENCODE_VERSION (2, 60))
#define GLIB_VERSION_2_62 (G_ENCODE_VERSION (2, 62))
#define GLIB_VERSION_2_64 (G_ENCODE_VERSION (2, 64))
#define GLIB_VERSION_2_66 (G_ENCODE_VERSION (2, 66))
#define GLIB_VERSION_2_68 (G_ENCODE_VERSION (2, 68))
#define GLIB_VERSION_2_70 (G_ENCODE_VERSION (2, 70))
#define GLIB_VERSION_2_72 (G_ENCODE_VERSION (2, 72))
#define GLIB_VERSION_2_74 (G_ENCODE_VERSION (2, 74))
#define GLIB_VERSION_2_76 (G_ENCODE_VERSION (2, 76))
#define GLIB_VERSION_2_78 (G_ENCODE_VERSION (2, 78))
#define GLIB_VERSION_2_80 (G_ENCODE_VERSION (2, 80))
#define GLIB_VERSION_2_82 (G_ENCODE_VERSION (2, 82))
#define GLIB_VERSION_2_84 (G_ENCODE_VERSION (2, 84))
#define GLIB_VERSION_2_86 (G_ENCODE_VERSION (2, 86))

/**
 * GLIB_VERSION_CUR_STABLE:
 *
 * A macro that evaluates to the current stable version of GLib, in a format
 * that can be used by the C pre-processor.
 *
 * During an unstable development cycle, this evaluates to the next stable
 * (unreleased) version which will be the result of the development cycle.
 *
 * Since: 2.32
 */
#if (GLIB_MINOR_VERSION % 2)
#define GLIB_VERSION_CUR_STABLE (G_ENCODE_VERSION (GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION + 1))
#else
#define GLIB_VERSION_CUR_STABLE (G_ENCODE_VERSION (GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION))
#endif

/**
 * GLIB_VERSION_PREV_STABLE:
 *
 * A macro that evaluates to the previous stable version of GLib, in a format
 * that can be used by the C pre-processor.
 *
 * During an unstable development cycle, this evaluates to the most recent
 * released stable release, which preceded this development cycle.
 *
 * Since: 2.32
 */
#if (GLIB_MINOR_VERSION % 2)
#define GLIB_VERSION_PREV_STABLE (G_ENCODE_VERSION (GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION - 1))
#else
#define GLIB_VERSION_PREV_STABLE (G_ENCODE_VERSION (GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION - 2))
#endif

/**
 * GLIB_VERSION_MIN_REQUIRED:
 *
 * A macro that should be defined by the user prior to including
 * the glib.h header.
 * The definition should be one of the predefined GLib version
 * macros: %GLIB_VERSION_2_26, %GLIB_VERSION_2_28,...
 *
 * This macro defines the earliest version of GLib that the package is
 * required to be able to compile against.
 *
 * If the compiler is configured to warn about the use of deprecated
 * functions, then using functions that were deprecated in version
 * %GLIB_VERSION_MIN_REQUIRED or earlier will cause warnings (but
 * using functions deprecated in later releases will not).
 *
 * Since: 2.32
 */
/* If the package sets GLIB_VERSION_MIN_REQUIRED to some future
 * GLIB_VERSION_X_Y value that we don't know about, it will compare as
 * 0 in preprocessor tests.
 */
#ifndef GLIB_VERSION_MIN_REQUIRED
#define GLIB_VERSION_MIN_REQUIRED (GLIB_VERSION_CUR_STABLE)
#elif GLIB_VERSION_MIN_REQUIRED == 0
#undef GLIB_VERSION_MIN_REQUIRED
#define GLIB_VERSION_MIN_REQUIRED (GLIB_VERSION_CUR_STABLE + 2)
#endif

/**
 * GLIB_VERSION_MAX_ALLOWED:
 *
 * A macro that should be defined by the user prior to including
 * the glib.h header.
 * The definition should be one of the predefined GLib version
 * macros: %GLIB_VERSION_2_26, %GLIB_VERSION_2_28,...
 *
 * This macro defines the latest version of the GLib API that the
 * package is allowed to make use of.
 *
 * If the compiler is configured to warn about the use of deprecated
 * functions, then using functions added after version
 * %GLIB_VERSION_MAX_ALLOWED will cause warnings.
 *
 * Unless you are using GLIB_CHECK_VERSION() or the like to compile
 * different code depending on the GLib version, then this should be
 * set to the same value as %GLIB_VERSION_MIN_REQUIRED.
 *
 * Since: 2.32
 */
#if !defined(GLIB_VERSION_MAX_ALLOWED) || (GLIB_VERSION_MAX_ALLOWED == 0)
#undef GLIB_VERSION_MAX_ALLOWED
#define GLIB_VERSION_MAX_ALLOWED (GLIB_VERSION_CUR_STABLE)
#endif

/* sanity checks */
#if GLIB_VERSION_MIN_REQUIRED > GLIB_VERSION_CUR_STABLE
#error "GLIB_VERSION_MIN_REQUIRED must be <= GLIB_VERSION_CUR_STABLE"
#endif
#if GLIB_VERSION_MAX_ALLOWED < GLIB_VERSION_MIN_REQUIRED
#error "GLIB_VERSION_MAX_ALLOWED must be >= GLIB_VERSION_MIN_REQUIRED"
#endif
#if GLIB_VERSION_MIN_REQUIRED < GLIB_VERSION_2_26
#error "GLIB_VERSION_MIN_REQUIRED must be >= GLIB_VERSION_2_26"
#endif

#endif /*  __G_VERSION_MACROS_H__ */