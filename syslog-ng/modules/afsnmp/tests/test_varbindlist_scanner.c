/*
 * Copyright (c) 2017 Balabit
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 */

#include <criterion/criterion.h>
#include "varbindlist-scanner.h"

#include "apphook.h"
#include "scratch-buffers.h"

#define SIZE_OF_ARRAY(array) (sizeof(array) / sizeof((array)[0]))

typedef struct
{
  const gchar *key;
  const gchar *type;
  const gchar *value;
} TestVarBind;

static void
_expect_no_more_tokens(VarBindListScanner *scanner)
{
  cr_expect_not(varbindlist_scanner_scan_next(scanner));
}

static void
_expect_next_key_type_value(VarBindListScanner *scanner, const gchar *key, const gchar *type,
                            const gchar *value)
{
  cr_expect(varbindlist_scanner_scan_next(scanner));
  cr_expect_str_eq(varbindlist_scanner_get_current_key(scanner), key);
  cr_expect_str_eq(varbindlist_scanner_get_current_type(scanner), type);
  cr_expect_str_eq(varbindlist_scanner_get_current_value(scanner), value);
}

static VarBindListScanner *
create_scanner(void)
{
  VarBindListScanner *scanner = varbindlist_scanner_new();

  return scanner;
}

static void
expect_varbindlist(const gchar *input, TestVarBind *expected, gsize number_of_expected)
{
  VarBindListScanner *scanner = create_scanner();
  varbindlist_scanner_input(scanner, input);

  for (int i=0; i < number_of_expected; i++)
    _expect_next_key_type_value(scanner, expected[i].key, expected[i].type, expected[i].value);

  _expect_no_more_tokens(scanner);

  varbindlist_scanner_free(scanner);
}

Test(varbindlist_scanner, test_spaces_as_separator)
{
  const gchar *input = "iso.3.6.1.6.3.1.1.4.1.0 = OID: iso.3.6.1.4.1.18372.3.2.1.1.2.2       "
                       "iso.3.6.1.4.1.18372.3.2.1.1.1.6 = STRING: \"svc/w4joHeFNzpFNrC8u9umJhc/ssh_4eyes_user_subjects:3/ssh\"";

  TestVarBind expected[] =
  {
    {"iso.3.6.1.6.3.1.1.4.1.0", "OID", "iso.3.6.1.4.1.18372.3.2.1.1.2.2"},
    {"iso.3.6.1.4.1.18372.3.2.1.1.1.6", "STRING", "svc/w4joHeFNzpFNrC8u9umJhc/ssh_4eyes_user_subjects:3/ssh"}
  };

  expect_varbindlist(input, expected, SIZE_OF_ARRAY(expected));
}

Test(varbindlist_scanner, test_tabs_and_spaces_as_separator)
{
  const gchar *input = "\t iso.3.6.1.6.3.1.1.4.1.0 = OID: iso.3.6.1.4.1.18372.3.2.1.1.2.2\t"
                       "iso.3.6.1.4.1.18372.3.2.1.1.1.6 = STRING: \"svc/w4joHeFNzpFNrC8u9umJhc/ssh_4eyes_user_subjects:3/ssh\"\t\t"
                       "iso.1.2 = INTEGER: 40 \t"
                       "iso.3.4 = INTEGER: 30\t "
                       "iso.5.6 = INTEGER: 20  \t\t "
                       "iso.7.8 = INTEGER: 10";

  TestVarBind expected[] =
  {
    {"iso.3.6.1.6.3.1.1.4.1.0", "OID", "iso.3.6.1.4.1.18372.3.2.1.1.2.2"},
    {"iso.3.6.1.4.1.18372.3.2.1.1.1.6", "STRING", "svc/w4joHeFNzpFNrC8u9umJhc/ssh_4eyes_user_subjects:3/ssh"},
    {"iso.1.2", "INTEGER", "40"},
    {"iso.3.4", "INTEGER", "30"},
    {"iso.5.6", "INTEGER", "20"},
    {"iso.7.8", "INTEGER", "10"}
  };

  expect_varbindlist(input, expected, SIZE_OF_ARRAY(expected));
}

Test(varbindlist_scanner, test_key_representations)
{
  const gchar *input = ".1.3.6.1.2.1.1.3.0 = STRING: \"\"\t"
                       "IP-MIB::ipForwarding.0 = INTEGER: 0\t"
                       "sysUpTime.0 = Timeticks: 1:15:09:27.63\t"
                       "SNMP-VIEW-BASED-ACM-MIB::vacmSecurityModel.0.3.119.101.115 = xxx";

  TestVarBind expected[] =
  {
    { ".1.3.6.1.2.1.1.3.0", "STRING", "" },
    { "IP-MIB::ipForwarding.0", "INTEGER", "0" },
    { "sysUpTime.0", "Timeticks", "1:15:09:27.63" },
    { "SNMP-VIEW-BASED-ACM-MIB::vacmSecurityModel.0.3.119.101.115", "", "xxx" }
  };

  expect_varbindlist(input, expected, SIZE_OF_ARRAY(expected));
}

Test(varbindlist_scanner, test_all_varbind_type)
{
  const gchar *input =
    ".iso.org.dod.internet.mgmt.mib-2.system.sysUpTime.0 = Timeticks: (875496867) 101 days, 7:56:08.67\t"
    "iso.3.6.1.6.3.1.1.4.1.0 = OID: iso.3.6.1.4.1.8072.2.3.0.1\t"
    "iso.3.6.1.4.1.8072.2.3.2.1 = INTEGER: 60\t"
    "SNMP-VIEW-BASED-ACM-MIB::vacmSecurityModel.0.3.119.101.115 = STRING: \"random string\"\t"
    "iso.3.2.2 = Gauge32: 22\t"
    "iso.3.1.1 = Counter32: 11123123 \t"
    "iso.3.5.3 = Hex-STRING: A0 BB CC DD EF\t"
    "iso.3.8.8 = NULL \t"
    "iso.2.1.1 = Timeticks: (34234234) 3 days, 23:05:42.34\t"
    "SNMP-VIEW-BASED-ACM-MIB::vacmSecurityModel.0.wes = IpAddress: 192.168.1.0";

  TestVarBind expected[] =
  {
    { ".iso.org.dod.internet.mgmt.mib-2.system.sysUpTime.0", "Timeticks", "(875496867) 101 days, 7:56:08.67" },
    { "iso.3.6.1.6.3.1.1.4.1.0", "OID", "iso.3.6.1.4.1.8072.2.3.0.1" },
    { "iso.3.6.1.4.1.8072.2.3.2.1", "INTEGER", "60" },
    { "SNMP-VIEW-BASED-ACM-MIB::vacmSecurityModel.0.3.119.101.115", "STRING", "random string" },
    { "iso.3.2.2", "Gauge32", "22" },
    { "iso.3.1.1", "Counter32", "11123123" },
    { "iso.3.5.3", "Hex-STRING", "A0 BB CC DD EF" },
    { "iso.3.8.8", "", "NULL" },
    { "iso.2.1.1", "Timeticks", "(34234234) 3 days, 23:05:42.34" },
    { "SNMP-VIEW-BASED-ACM-MIB::vacmSecurityModel.0.wes", "IpAddress", "192.168.1.0" }
  };

  expect_varbindlist(input, expected, SIZE_OF_ARRAY(expected));
}

Test(varbindlist_scanner, test_separator_inside_of_quoted_value)
{
  const gchar *input =
    "iso.1.2.3 = STRING: \"quoted = string \t innerkey='innervalue'\" \t"
    "iso.3.8.8 = NULL\t";

  TestVarBind expected[] =
  {
    { "iso.1.2.3", "STRING", "quoted = string \t innerkey='innervalue'" },
    { "iso.3.8.8", "", "NULL" }
  };

  expect_varbindlist(input, expected, SIZE_OF_ARRAY(expected));
}

Test(varbindlist_scanner, test_multiline_quouted_value)
{
  const gchar *input =
    "iso.3.6.1.4.1.18372.3.2.1.1.1.6 = STRING: \"multi \n line\r\nvalue\" \t"
    "iso.3.8.8 = NULL";

  TestVarBind expected[] =
  {
    { "iso.3.6.1.4.1.18372.3.2.1.1.1.6", "STRING", "multi \n line\r\nvalue" },
    { "iso.3.8.8", "", "NULL" }
  };

  expect_varbindlist(input, expected, SIZE_OF_ARRAY(expected));
}

static void
setup(void)
{
  app_startup();
}

static void
teardown(void)
{
  scratch_buffers_explicit_gc();
  app_shutdown();
}

TestSuite(varbindlist_scanner, .init = setup, .fini = teardown);
