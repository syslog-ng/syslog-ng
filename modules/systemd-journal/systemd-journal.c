/*
 * Copyright (c) 2014      BalaBit S.a.r.l., Luxembourg, Luxembourg
 * Copyright (c) 2012-2014 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2012-2014 Viktor Juhasz <viktor.juhasz@balabit.com>
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

#include "messages.h"
#include "systemd-journal.h"
#include "journald-subsystem.h"
#include "systemd-journal.h"

#include <stdlib.h>

struct _SystemdJournalSourceDriver
{
  LogSrcDriver super;
  JournalReaderOptions reader_options;
  JournalReader *reader;
  Journald *journald;
};

JournalReaderOptions *
systemd_journal_get_reader_options(LogDriver *s)
{
  SystemdJournalSourceDriver *self = (SystemdJournalSourceDriver *)s;
  return &self->reader_options;
}

static gchar *
__generate_persist_name(SystemdJournalSourceDriver *self)
{
  return g_strdup_printf("journald_source_%s_%s", self->super.super.group, self->super.super.id);
}

static gboolean
__init(LogPipe *s)
{
  SystemdJournalSourceDriver *self = (SystemdJournalSourceDriver *)s;
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super.super);
  gchar *persist_name = __generate_persist_name(self);
  self->reader = journal_reader_new(cfg, self->journald);

  journal_reader_options_init(&self->reader_options, cfg, self->super.super.group);

  journal_reader_set_options((LogPipe *)self->reader, &self->super.super.super,  &self->reader_options, 0, SCS_JOURNALD, self->super.super.id, "journal");

  journal_reader_set_persist_name(self->reader, persist_name);
  g_free(persist_name);

  log_pipe_append((LogPipe *)self->reader, &self->super.super.super);
  if (!log_pipe_init((LogPipe *)self->reader, cfg))
    {
      msg_error("Error initializing journal_reader",
                NULL);
      log_pipe_unref((LogPipe *) self->reader);
      self->reader = NULL;
      return FALSE;
    }
  return TRUE;
}

static gboolean
__deinit(LogPipe *s)
{
  SystemdJournalSourceDriver *self = (SystemdJournalSourceDriver *)s;
  if (self->reader)
    {
      log_pipe_deinit((LogPipe *)self->reader);
      log_pipe_unref((LogPipe *)self->reader);
      self->reader = NULL;
    }

  log_src_driver_deinit_method(s);
  return TRUE;
}

static void
__free(LogPipe *s)
{
  SystemdJournalSourceDriver *self = (SystemdJournalSourceDriver *)s;
  journal_reader_options_destroy(&self->reader_options);
  journald_free(self->journald);
  log_src_driver_free(s);
}

LogDriver *
systemd_journal_sd_new()
{
  SystemdJournalSourceDriver *self = g_new0(SystemdJournalSourceDriver, 1);
  log_src_driver_init_instance(&self->super);
  self->super.super.super.init = __init;
  self->super.super.super.deinit = __deinit;
  self->super.super.super.free_fn = __free;
  journal_reader_options_defaults(&self->reader_options);
  self->journald = journald_new();
  return &self->super.super;
}
