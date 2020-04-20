/*
 * Copyright (c) 2018 Balabit
 * Copyright (c) 2018 László Várady <laszlo.varady@balabit.com>
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

#ifndef MSG_GENERATOR_SOURCE_H
#define MSG_GENERATOR_SOURCE_H

#include "cfg.h"
#include "msg-generator-source-options.h"

typedef struct MsgGeneratorSource MsgGeneratorSource;

MsgGeneratorSource *msg_generator_source_new(GlobalConfig *cfg);
gboolean msg_generator_source_init(MsgGeneratorSource *self);
gboolean msg_generator_source_deinit(MsgGeneratorSource *self);
void msg_generator_source_free(MsgGeneratorSource *self);

void msg_generator_source_set_options(MsgGeneratorSource *self, MsgGeneratorSourceOptions *options,
                                      const gchar *stats_id, const gchar *stats_instance, gboolean threaded,
                                      gboolean pos_tracked, LogExprNode *expr_node);


#endif
