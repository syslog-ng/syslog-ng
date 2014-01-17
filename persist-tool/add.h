/*
 * Copyright (c) 2002-2014 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2014 Viktor Juhasz
 * Copyright (c) 2014 Viktor Tusa
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

#ifndef ADD_H
#define ADD_H 1

#include "syslog-ng.h"
#include "mainloop.h"
#include "persist-state.h"
#include "plugin.h"
#include "persistable-state-presenter.h"
#include "cfg.h"
#include "persist-tool.h"

gchar *persist_state_dir;

static GOptionEntry add_options[] =
{
    { "output-dir", 'o', 0, G_OPTION_ARG_STRING, &persist_state_dir,"The directory where persist file is located. The name of the persist file stored in this directory must be syslog-ng.persist", "<directory>" },
    { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL }
};

gint add_main(int argc, char *argv[]);

#endif
