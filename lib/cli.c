/*
 * Copyright (c) 2016-2017 Noémi Ványi
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
 */


#include "cli.h"
#include "messages.h"
#include "uuid.h"

#include <stdlib.h>
#include <string.h>

static const char DELIMITER[] = " ";
static const char LOG_PATH_TEMPLATE[] = "%s(%s);";
static const char DRIVER_TEMPLATE[] = "%s %s { %s};\n";
static const char DRIVER_NAME_TEMPLATE[] = "%s%s";
static const char CFG_FILE_TEMPLATE[] =
  "@version: %s\n"
  "@include scl.conf\n"
  "source s_stdin { stdin(); };\n"
  "destination d_stdout { stdout(); };\n"
  "%s"
  "log {source(s_stdin);%s destination(d_stdout);};";


static gchar *
_generate_name(gchar *driver_type)
{
  gchar driver_uuid[37];

  uuid_gen_random(driver_uuid, sizeof(driver_uuid));

  return g_strdup_printf(DRIVER_NAME_TEMPLATE, driver_type, driver_uuid);
}

CliParam *
cli_param_new(gchar *type, gchar *cfg)
{
  CliParam *self = g_new0(CliParam, 1);
  self->type = type;
  self->cfg = cfg;
  self->name = _generate_name(type);
  return self;
}

static gchar *
_get_config_of_driver(CliParam *driver)
{
  return g_strdup_printf(DRIVER_TEMPLATE, driver->type, driver->name, driver->cfg);
}

static gchar *
_get_logpath_of_driver(CliParam *driver)
{
  return g_strdup_printf(LOG_PATH_TEMPLATE, driver->type, driver->name);
}

static gchar *
_concatenate_strings(GList *elements)
{
  GList *element;
  gchar *concatenated = "";

  for (element = elements; element != NULL; element = element->next)
    concatenated = g_strconcat(concatenated, " ", element->data, NULL);

  return concatenated;
}

static gchar *
_get_config_lines_of_generated_snippets(GList *driver_configs, GList *log_paths)
{
  gchar *drivers_line, *log_paths_line;

  drivers_line = _concatenate_strings(driver_configs);
  log_paths_line = _concatenate_strings(log_paths);

  return g_strdup_printf(CFG_FILE_TEMPLATE, SYSLOG_NG_VERSION, drivers_line, log_paths_line);
}

void
cli_param_converter_convert(CliParamConverter *self)
{
  GList *current_param;
  GList *driver_configs = NULL;
  GList *logpaths = NULL;

  for (current_param = self->params; current_param != NULL; current_param = current_param->next)
    {
      CliParam *cli_param = (CliParam *) current_param->data;
      gchar *driver_config = _get_config_of_driver(cli_param);
      gchar *driver_logpath = _get_logpath_of_driver(cli_param);

      driver_configs = g_list_append(driver_configs, driver_config);
      logpaths = g_list_append(logpaths, driver_logpath);
    }
  self->generated_config = _get_config_lines_of_generated_snippets(driver_configs, logpaths);
}

static inline gboolean
_more_tokens(gchar *token)
{
  return (token != NULL);
}

static inline gboolean
_is_driver_type_empty(gchar *driver_type)
{
  if (driver_type == NULL)
    return TRUE;
  return FALSE;
}

static void
_parse_driver_data(gchar *token, gchar **driver_type, gchar **driver_config)
{
  if (_is_driver_type_empty(*driver_type))
    *driver_type = token;
  else
    *driver_config = g_strconcat(*driver_config, token, NULL);
}

static inline gboolean
_is_driver_ending(gchar *token)
{
  size_t token_length = strlen(token);
  return (token[token_length - 1] == ';');
}

static inline gboolean
_is_driver_config_empty(gchar *driver_config)
{
  if (driver_config[0] == '\0')
    return TRUE;
  return FALSE;
}

static inline gboolean
_is_driver_data_incomplete(gchar *driver_type, gchar *driver_config)
{
  if (_is_driver_type_empty(driver_type) || _is_driver_config_empty(driver_config))
    return TRUE;
  return FALSE;
}

static void
_add_new_cli_param(CliParamConverter *self, gchar *driver_type, gchar *driver_config)
{
  self->params = g_list_append(self->params, cli_param_new(driver_type, g_strdup(driver_config)));
  driver_type = NULL;
  driver_config = "";
}

static gboolean
_parse_raw_parameter(CliParamConverter *self, gchar *raw_param)
{
  gchar *token;
  gchar *driver_config = "";
  gchar *driver_type = NULL;

  if (!_is_driver_ending(raw_param))
    return FALSE;

  token = strtok(raw_param, DELIMITER);
  while (_more_tokens(token))
    {
      _parse_driver_data(token, &driver_type, &driver_config);

      if (_is_driver_ending(token) && _is_driver_data_incomplete(driver_type, driver_config))
        return FALSE;

      if (_is_driver_ending(token))
        _add_new_cli_param(self, driver_type, driver_config);

      token = strtok(NULL, DELIMITER);
    }

  return TRUE;
}

gboolean
cli_param_converter_setup(CliParamConverter *self)
{
  guint i;

  for (i = 0; self->raw_params[i]; i++)
    {
      gchar *raw_param = g_strdup(self->raw_params[i]);

      if (!_parse_raw_parameter(self, raw_param))
        return FALSE;
    }
  return TRUE;
}

CliParamConverter *
cli_param_converter_new(gchar **args)
{
  CliParamConverter *self = g_new0(CliParamConverter, 1);
  self->raw_params = args;
  self->params = NULL;
  self->generated_config = NULL;
  return self;
}
