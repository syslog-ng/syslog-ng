/*
 * Copyright (c) 2002-2016 Balabit
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
 *   msg_warning("WARNING: foo got changed to bar in " VERSION_3_4");
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

#define PRODUCT_DOCUMENTATION "https://www.syslog-ng.com/technical-documents/list/syslog-ng-open-source-edition"
#define PRODUCT_CONTACT "https://lists.balabit.hu/mailman/listinfo/syslog-ng"

#define VERSION_3_0 "syslog-ng 3.0"
#define VERSION_3_1 "syslog-ng 3.1"
#define VERSION_3_2 "syslog-ng 3.2"
#define VERSION_3_3 "syslog-ng 3.3"
#define VERSION_3_4 "syslog-ng 3.4"
#define VERSION_3_5 "syslog-ng 3.5"
#define VERSION_3_6 "syslog-ng 3.6"
#define VERSION_3_7 "syslog-ng 3.7"
#define VERSION_3_8 "syslog-ng 3.8"
#define VERSION_3_9 "syslog-ng 3.9"
#define VERSION_3_10 "syslog-ng 3.10"
#define VERSION_3_11 "syslog-ng 3.11"
#define VERSION_3_12 "syslog-ng 3.12"
#define VERSION_3_13 "syslog-ng 3.13"
#define VERSION_3_14 "syslog-ng 3.14"
#define VERSION_3_15 "syslog-ng 3.15"
#define VERSION_3_16 "syslog-ng 3.16"
#define VERSION_3_17 "syslog-ng 3.17"
#define VERSION_3_18 "syslog-ng 3.18"
#define VERSION_3_19 "syslog-ng 3.19"
#define VERSION_3_20 "syslog-ng 3.20"
#define VERSION_3_21 "syslog-ng 3.21"
#define VERSION_3_22 "syslog-ng 3.22"
#define VERSION_3_23 "syslog-ng 3.23"
#define VERSION_3_24 "syslog-ng 3.24"
#define VERSION_3_25 "syslog-ng 3.25"
#define VERSION_3_26 "syslog-ng 3.26"
#define VERSION_3_27 "syslog-ng 3.27"
#define VERSION_3_28 "syslog-ng 3.28"
#define VERSION_3_29 "syslog-ng 3.29"
#define VERSION_3_30 "syslog-ng 3.30"
#define VERSION_3_31 "syslog-ng 3.31"
#define VERSION_3_32 "syslog-ng 3.32"
#define VERSION_3_33 "syslog-ng 3.33"
#define VERSION_3_34 "syslog-ng 3.34"
#define VERSION_3_35 "syslog-ng 3.35"

#define VERSION_4_0 "syslog-ng 4.0"

/* VERSION_VALUE_* references versions as integers to be compared against stuff like cfg->user_version */
/* VERSION_STR_* references versions as strings to be shown to the user */

#define VERSION_VALUE_3_0  0x0300
#define VERSION_VALUE_3_1  0x0301
#define VERSION_VALUE_3_2  0x0302
#define VERSION_VALUE_3_3  0x0303
#define VERSION_VALUE_3_4  0x0304
#define VERSION_VALUE_3_5  0x0305
#define VERSION_VALUE_3_6  0x0306
#define VERSION_VALUE_3_7  0x0307
#define VERSION_VALUE_3_8  0x0308
#define VERSION_VALUE_3_9  0x0309
#define VERSION_VALUE_3_10 0x030a
#define VERSION_VALUE_3_11 0x030b
#define VERSION_VALUE_3_12 0x030c
#define VERSION_VALUE_3_13 0x030d
#define VERSION_VALUE_3_14 0x030e
#define VERSION_VALUE_3_15 0x030f
#define VERSION_VALUE_3_16 0x0310
#define VERSION_VALUE_3_17 0x0311
#define VERSION_VALUE_3_18 0x0312
#define VERSION_VALUE_3_19 0x0313
#define VERSION_VALUE_3_20 0x0314
#define VERSION_VALUE_3_21 0x0315
#define VERSION_VALUE_3_22 0x0316
#define VERSION_VALUE_3_23 0x0317
#define VERSION_VALUE_3_24 0x0318
#define VERSION_VALUE_3_25 0x0319
#define VERSION_VALUE_3_26 0x031a
#define VERSION_VALUE_3_27 0x031b
#define VERSION_VALUE_3_28 0x031c
#define VERSION_VALUE_3_29 0x031d
#define VERSION_VALUE_3_30 0x031e
#define VERSION_VALUE_3_31 0x031f
#define VERSION_VALUE_3_32 0x0320
#define VERSION_VALUE_3_33 0x0321
#define VERSION_VALUE_3_34 0x0322
#define VERSION_VALUE_3_35 0x0323

/* these are defined to allow 4.0 related changes to be introduced while we
 * are still producing 3.x releases. */
#define VERSION_VALUE_3_LAST  0x0363
#define VERSION_VALUE_4_0     0x0400

/* config version code, in the same format as GlobalConfig->version */
#define VERSION_VALUE_CURRENT   VERSION_VALUE_3_35
#define VERSION_STR_CURRENT     "3.35"
#define VERSION_PRODUCT_CURRENT VERSION_3_35

/* this value points to the last syslog-ng version where we changed the
 * meaning of any setting in the configuration file.  Basically, it is the
 * highest value passed to any cfg_is_config_version_older() call.
 */
#define VERSION_VALUE_LAST_SEMANTIC_CHANGE  VERSION_VALUE_3_35
#define VERSION_STR_LAST_SEMANTIC_CHANGE    "3.35"

#define version_convert_from_user(v)  (v)

#include "pe-versioning.h"
#endif
