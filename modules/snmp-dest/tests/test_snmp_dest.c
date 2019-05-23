/*
 * Copyright (c) 2019 Balabit
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
 *
 */

#include <criterion/criterion.h>
#include <criterion/parameterized.h>
#include <glib/gstdio.h>
#include <unistd.h>
#include "apphook.h"
#include "snmpdest.h"
#include "logpipe.h"

static SNMPDestDriver *snmp_driver;
static GlobalConfig *cfg;

static void
_init(void)
{
  app_startup();
  cfg = cfg_new_snippet();
  snmp_driver = (SNMPDestDriver *)snmpdest_dd_new(cfg);
}

static void
_teardown(void)
{
  log_pipe_unref((LogPipe *)&snmp_driver->super.super);
  app_shutdown();
  cfg_free(cfg);
}

TestSuite(test_snmp_dest, .init = _init, .fini = _teardown);

Test(test_snmp_dest, set_version)
{
  LogDriver *driver = (LogDriver *)snmp_driver;

  snmpdest_dd_set_version(driver, "v2c");
  cr_assert_str_eq(snmpdest_dd_get_version(driver), "v2c");
}

Test(test_snmp_dest, set_host)
{
  LogDriver *driver = (LogDriver *)snmp_driver;

  snmpdest_dd_set_host(driver, "127.0.0.1");
  cr_assert_str_eq(snmp_driver->host, "127.0.0.1");
}

Test(test_snmp_dest, set_port)
{
  LogDriver *driver = (LogDriver *)snmp_driver;

  snmpdest_dd_set_port(driver, 161);
  cr_assert_eq(snmp_driver->port, 161);
}

Test(test_snmp_dest, set_community)
{
  LogDriver *driver = (LogDriver *)snmp_driver;

  snmpdest_dd_set_community(driver, "my_community");
  cr_assert_str_eq(snmp_driver->community, "my_community");
}

Test(test_snmp_dest, set_engine_id)
{
  LogDriver *driver = (LogDriver *)snmp_driver;

  snmpdest_dd_set_engine_id(driver, "engine_id");
  cr_assert_str_eq(snmp_driver->engine_id, "engine_id");
}

Test(test_snmp_dest, set_auth_username)
{
  LogDriver *driver = (LogDriver *)snmp_driver;

  snmpdest_dd_set_auth_username(driver, "auth_username");
  cr_assert_str_eq(snmp_driver->auth_username, "auth_username");
}

Test(test_snmp_dest, set_auth_algo)
{
  LogDriver *driver = (LogDriver *)snmp_driver;

  snmpdest_dd_set_auth_algorithm(driver, "auth_algorithm");
  cr_assert_str_eq(snmp_driver->auth_algorithm, "auth_algorithm");
  cr_assert_not(snmpdest_dd_check_auth_algorithm(snmp_driver->auth_algorithm));

  snmpdest_dd_set_auth_algorithm(driver, "SHA");
  cr_assert(snmpdest_dd_check_auth_algorithm(snmp_driver->auth_algorithm));
}

Test(test_snmp_dest, set_auth_password)
{
  LogDriver *driver = (LogDriver *)snmp_driver;

  snmpdest_dd_set_auth_password(driver, "password");
  cr_assert_str_eq(snmp_driver->auth_password, "password");
}

Test(test_snmp_dest, set_enc_algo)
{
  LogDriver *driver = (LogDriver *)snmp_driver;

  snmpdest_dd_set_enc_algorithm(driver, "enc_algorithm");
  cr_assert_str_eq(snmp_driver->enc_algorithm, "enc_algorithm");
  cr_assert_not(snmpdest_dd_check_enc_algorithm(snmp_driver->enc_algorithm));

  snmpdest_dd_set_enc_algorithm(driver, "AES");
  cr_assert(snmpdest_dd_check_enc_algorithm(snmp_driver->enc_algorithm));
}

Test(test_snmp_dest, set_enc_password)
{
  LogDriver *driver = (LogDriver *)snmp_driver;

  snmpdest_dd_set_enc_password(driver, "enc_password");
  cr_assert_str_eq(snmp_driver->enc_password, "enc_password");
}

Test(test_snmp_dest, set_transport)
{
  LogDriver *driver = (LogDriver *)snmp_driver;

  snmpdest_dd_set_transport(driver, "transport");
  cr_assert_str_eq(snmp_driver->transport, "transport");
}

Test(test_snmp_dest, set_time_zone)
{
  LogDriver *driver = (LogDriver *)snmp_driver;

  snmpdest_dd_set_time_zone(driver, "time_zone");
  cr_assert_str_eq(snmp_driver->template_options.time_zone[LTZ_LOCAL], "time_zone");
}

Test(test_snmp_dest, check_required_params)
{
  LogDriver *driver = (LogDriver *)snmp_driver;
  gchar err_msg[128];

  /* check parameters of an empty driver struct */
  cr_assert_not(snmpdest_check_required_params(driver, err_msg));

  /* setup snmp driver with valid parameters */
  snmpdest_dd_set_snmp_obj(driver, cfg, ".1.3.6.1.4.1.18372.3.1.1.1.1.1.0", "Octetstring", "admin");
  snmpdest_dd_set_trap_obj(driver, cfg, ".1.3.6.1.6.3.1.1.4.1.0", "Objectid", ".1.3.6.1.4.1.18372.3.1.1.1.2.1");
  snmpdest_dd_set_host(driver, "127.0.0.1");
  snmpdest_dd_set_version(driver, "v2c");
  cr_assert(snmpdest_check_required_params(driver, err_msg),"check required param failed %s", err_msg);

  /* for v3 version the engine_id shall be set as well*/
  snmpdest_dd_set_version(driver, "v3");
  snmpdest_dd_set_engine_id(driver, "engine_id");
  cr_assert(snmpdest_check_required_params(driver, err_msg));
}

typedef struct _snmp_obj_test_param
{
  const gchar *objectid;
  const gchar *type;
  const gchar *value;
  gboolean  expected_result;
} SnmpObjTestParam;

ParameterizedTestParameters(test_snmp_dest, test_set_snmp_obj)
{
  static SnmpObjTestParam parser_params[] =
  {
    {
      .objectid = ".1.3.6.1.4.1.18372.3.1.1.1.1.3.0",
      .type = "integer",
      .value = "123",
      .expected_result = TRUE
    },
    {
      .objectid = ".1.3.6.1.4.1.18372.3.1.1.1.1.3.0",
      .type = "timeticks",
      .value = "0",
      .expected_result = TRUE
    },
    {
      .objectid = ".1.3.6.1.4.1.18372.3.1.1.1.1.1.0",
      .type = "octetstring",
      .value = "Test SNMP trap",
      .expected_result = TRUE
    },
    {
      .objectid = ".1.3.6.1.4.1.18372.3.1.1.1.1.3.0",
      .type = "counter32",
      .value = "1234567",
      .expected_result = TRUE
    },
    {
      .objectid = ".1.3.6.1.4.1.18372.3.1.1.1.1.3.0",
      .type = "ipaddress",
      .value = "127.0.0.1",
      .expected_result = TRUE
    },
    {
      .objectid = ".1.3.6.1.6.3.1.1.4.1.0",
      .type = "objectid",
      .value = ".1.3.6.1.4.1.18372.3.1.1.1.2.1",
      .expected_result = TRUE
    },
    {
      .objectid = "my_object_id",
      .type = "pacalporkolt",
      .value = "krumpli",
      .expected_result = FALSE
    },
  };
  return cr_make_param_array(SnmpObjTestParam, parser_params, G_N_ELEMENTS(parser_params));
}

ParameterizedTest(SnmpObjTestParam *param, test_snmp_dest, test_set_snmp_obj)
{
  LogDriver *driver = (LogDriver *)snmp_driver;

  gboolean result = snmpdest_dd_set_snmp_obj(driver, cfg, param->objectid, param->type, param->value);

  cr_assert_eq(result, param->expected_result);
}
