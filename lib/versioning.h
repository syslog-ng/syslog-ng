/*
 * Copyright (c) 2002-2012 BalaBit IT Ltd, Budapest, Hungary
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

/* The aim of this header file is to make it easy to track version
 * identification in code when porting code from the PE tree to the OSE tree
 * or vice versa.  Error and warning messages should always use these macros
 * instead of inlining a concrete version number.  Runtime checks should
 * always use the get_version_value() function to convert OSE version codes
 * to PE ones.
 *
 ****************************************************************************
 * The code _should_ always use OSE version numbering, otherwise it'd break *
 * and confuse the user when code gets ported between trees.                *
 ****************************************************************************
 *
 * Versions in error messages
 * ==========================
 *
 * Whenever using an error message that needs to state a version number, it
 * should use one of the VERSION_X_Y macros, X_Y identifying the OSE version
 * number where the incompatible change was applied.
 *
 * Example:
 *
 *   msg_warning("WARNING: foo got changed to bar in " VERSION_3_4", NULL);
 *
 * Incompatible changes in non-OSE products
 * ========================================
 *
 * If you introduce an incompatible change in a private tree (but don't do
 * that), you should explicitly define a macro that identifies your version,
 * e.g.  if you have a product named foo, which is based on syslog-ng 3.3
 * and you decide to change the interpretation of a config syntax in your
 * 1.0 release, you should create a macro:
 *
 *    VERSION_FOO_1_0   "foo 1.0"
 *
 * This would be changed once the code gets integrated into syslog-ng, to
 * the "integrated-to" release.  That way, users that always used OSE gets
 * the version number properly.
 *
 * Rebasing to a tree that already has your incompatible change
 * ============================================================
 *
 * Assuming your change makes it to the OSE tree, the FOO specific version
 * references will be changed to OSE version macros, which could be a
 * problem for you when rebase your product, since your rebased product will
 * claim that the incompatible change happened _after_ it really happened.
 * The best solution is not to introduce incompatible changes or do that in
 * the OSE tree first.  If you have to something like this, make sure that
 * you take care about these at rebase time.
 *
 * Version number barrier
 * ======================
 *
 * For now there's an explicit barrier in version numbering between PE and
 * OSE, which is "4.0"; everything below "4.0" is an OSE version number,
 * everything ahead is a PE one.  This will change once there's sufficient
 * gap between the two (e.g.  PE 4.0 is long forgotten), in that case
 * OSE can also use version 4 and later.
 *
 * If there's another similar products built on syslog-ng (e.g. someone
 * other than BB creates such a product, the same scheme can be used by them
 * too).
 */

/* version references for major syslog-ng OSE versions. All error messages
 * should reference the syslog-ng version number through these macros, in order
 * to make it relatively simple to explain PE/OSE version numbers to users. */

#define VERSION_3_0 "syslog-ng 3.0"
#define VERSION_3_1 "syslog-ng 3.1"
#define VERSION_3_2 "syslog-ng 3.2"
#define VERSION_3_3 "syslog-ng 3.3"
#define VERSION_3_4 "syslog-ng 3.4"
#define VERSION_3_5 "syslog-ng 3.5"
#define VERSION_3_6 "syslog-ng 3.6"
#define VERSION_3_7 "syslog-ng 3.7"

/* config version code, in the same format as GlobalConfig->version */
#define VERSION_VALUE   0x0307
#define VERSION_CURRENT VERSION_3_7
#define VERSION_CURRENT_VER_ONLY "3.7"

#define version_convert_from_user(v)  (v)

#endif
