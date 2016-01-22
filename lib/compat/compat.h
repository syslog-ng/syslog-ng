/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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

/*
 * The compat subdirectory is a placeholder for missing functionality on
 * various platforms.  Here are the rules to follow when a specific platform
 * lacks a given functionality.
 *
 * NOTE: compat is the place for simple functions and fixups that should
 * have been defined by the system but isn't for some reason (old version,
 * weird platform etc).  It is not the place for extensive logic, complex
 * implementations and so on.  It is a strong indicator that we are doing
 * something wrong if a .c file in compat is more than a 100 lines.  For
 * those, a proper syslog-ng style API with several implementations is the
 * way to go.
 *
 * NOTE/2: please don't implement dummy, empty functions for things that
 * make no sense on a specific platforms.  In that case, please modify the
 * call-site instead.  Whenever you read a call-site of code that resides in
 * compat it shouldn't misguide you that it doesn't do anything.  For these
 * changing the call-site is better.
 *
 * File organization, call sites:
 *
 *  1) Add a header for that subsystem under lib/compat, this should in turn
 *     include "compat/compat.h" first, and then add all headers required by
 *     the interface itself.
 *
 *  2) compat.h will take care about including <config.h> no code under
 *     compat should do the same.
 *
 *  3) Add the implementation to one or more .c files. If the functions are
 *     unrelated, it's preferred to have a separate .c file for each, with a
 *     single header.  In other cases you can simply use the same name for
 *     the .c as you did for the .h
 *
 *  4) call sites should include compat/XXXX.h, preferably _instead_ of the
 *     system header that defines the given functionality. The compat header
 *     will include the required system headers anyway, no need to do that
 *     multiple times.
 *
 *  5) compat.h shouldn't be included by call-sites directly.
 *
 * If a function is missing on a set of platforms, they should be named as
 * POSIX named them (if they are standard), or if not, they should be named
 * as on Linux.
 */

#ifndef COMPAT_COMPAT_H_INCLUDED
#define COMPAT_COMPAT_H_INCLUDED

#include <syslog-ng-config.h>

#endif
