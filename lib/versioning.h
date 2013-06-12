/*
 * Copyright (c) 2002-2011 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
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

#ifndef __VERSIONING_H__
#define __VERSIONING_H__

/* version references for major syslog-ng OSE versions. All error messages
 * should reference the syslog-ng version number through these macros, in order
 * to make it relatively simple to explain PE/OSE version numbers to users. */

#define VERSION_3_0 "syslog-ng PE 3.0"
#define VERSION_3_1 "syslog-ng PE 4.0"
#define VERSION_3_2 "syslog-ng PE 4.0"
#define VERSION_3_3 "syslog-ng PE 4.1"

/* changes introduced in the PE branch of syslog-ng.
 *
 **************************************************
 *    SHOULD BE AVOIDED COMPLETELY IF POSSIBLE.   *
 **************************************************
 *
 * When porting such an error message to the OSE the version number should
 * change to the "integrated-to" version.  When rebasing PE to that OSE
 * major the error message should be "patched" to include the original PE
 * version the change was introduced in.
 */
#define VERSION_PE_4_1 "syslog-ng PE 4.1"
#define VERSION_PE_4_2 "syslog-ng PE 4.2"
#define VERSION_PE_5_0 "syslog-ng PE 5.0"

#define VERSION_VALUE   0x0500

gint get_version_value(gint version);

#endif
