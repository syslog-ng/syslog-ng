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
 *
 */

#ifndef CLI_H_INCLUDED
#define CLI_H_INCLUDED

#include <glib.h>

typedef struct _CliParam
{
  gchar *cfg;
  gchar *type;
} CliParam;

CliParam *cli_param_new(gchar *type, gchar *cfg);

typedef struct _CliParamConverter
{
  gchar **raw_params;
  gchar *destination_params;
  GList *params;
  gchar *generated_config;
} CliParamConverter;

CliParamConverter *cli_param_converter_new(gchar **params, gchar *destination_params);
gboolean cli_param_converter_setup(CliParamConverter *self);
void cli_param_converter_convert(CliParamConverter *self);

#endif
