/*
 * Copyright (c) 2023 Attila Szakacs
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
 * C++ compat helper headers: "compat/cpp-start.h" and "compat/cpp-end.h"
 *
 * Starting your C++ header with #include "compat/cpp-start.h" and ending it
 * with #include "compat/cpp-end.h" helps you with two things:
 *   - You will be able to include C headers.
 *   - C files will be able to include your C++ header.
 *
 * Limitations:
 *   - Obviously you can only write code in your C++ header which is
 *     syntactically correct in a C code, so you cannot define classes, etc.
 *     Do these in either your .cpp file or in a separate .hpp file, which
 *     does not get included in your C code.
 *   - As we are not preparing each C header to be includable in C++ code,
 *     but we are trying to include them with these helpers on the call site,
 *     we cannot be sure that any additional C header include will work.
 *     This adds a level of uncertainty to developing a plugin in C++.
 *
 * If you find a C header that cannot be imported to C++ with these helpers,
 * please try to fix those headers first.  If it is not possible, extend this
 * file with a workaround so others do not run into that issue again.
 *
 * Best practices:
 *   - Minimize the C++ code in your plugin, if you can write and build/link
 *     something as C code, do so, for example boilerplates: parser.{c,h}
 *     and plugin.c.  This minimizes the chance of including an incompatible
 *     C header.
 *   - Build/link the C++ code as C++ separately and link to that from your
 *     C lib.  Adding -lstdc++ to LIBADD is neccessary in this case as it is
 *     automatically added while linking the C++ object itself, but not when
 *     linking to the C++ object from a C lib.
 *   - In your C++ code it is not possible to derive from a C class in the
 *     usual C way by adding a super field and filling its free_fn, because
 *     we do our own reference counting and freeing logic and we cannot rely
 *     on the casting trick of struct packing.  You can create a struct
 *     in the usual C inheritance way and have a pointer to your C++ class
 *     in it and you can store a pointer to this struct in your C++ class as
 *     super, so you can access its fields. You can set the necessary virtual
 *     functions with wrapper functions of your real C++ functions in the
 *     ctor of the C style "class" == struct.
 *
 * You can see an example usage of the C++ plugin support at:
 *      modules/examples/sources/random-choice-generator
 */

#ifdef __cplusplus

#define this this_

extern "C" {

#endif
