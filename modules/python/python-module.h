/*
 * Copyright (c) 2015 Balabit
 * Copyright (c) 2015 Balazs Scheidler <balazs.scheidler@balabit.com>
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */
#ifndef PYTHON_MODULE_H_INCLUDED
#define PYTHON_MODULE_H_INCLUDED 1

/*
 * There are some Python related things to keep in mind when working on the
 * Python module.  The Python header unfortunately defines a few standard
 * related conditionals (_POSIX_C_SOURCE and _XOPEN_SOURCE) in Python.h that
 * it shouldn't, causing two issues potentially:
 *
 *   1) warnings, as these defines are usually defined by our configure
 *      script/config.h as well
 *
 *   2) triggers POSIX and XOPEN specific interfaces/incompatibilies to be
 *      enabled in system headers.  This could cause incompatibilities
 *      between C code that include Python.h and C code that doesn't.
 *      Imagine the case when a new struct member is added based on the
 *      preprocessor defines above.  In one part of the code that member
 *      wouldn't exist, in others it would.
 *
 *      Fortunately this rarely causes issues in practice for a number of reasons:
 *        a) syslog-ng is compiled with the same as _GNU_SOURCE is defined
 *           that implies the other two.<
 *
 *        b) system libraries are usually designed in a way that struct members
 *           are never removed/added but rather their name is changed (for
 *           example by prefixing them with a pair of underscores)
 *
 * Our workaround here is to check whether Python.h inclusion comes first
 * and if it's not we error out during compilation.  Within the Python
 * module of syslog-ng we should keep the convention to make sure that
 * python-module.h is the first included file in any C modules.
 *
 * python-module.h also makes sure that syslog-ng.h is included (so that
 * proper ordering happens), so you can omit syslog-ng.h from all
 * python-module headers.
 */

#if defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE)
#error "_POSIX_C_SOURCE or _XOPEN_SOURCE is already defined, python-module.h should be included first. Check out the comment in python-module.h for more information"
#endif

#include "compat/compat-python.h"
#endif
