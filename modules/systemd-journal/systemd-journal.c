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

static gboolean
__init(LogPipe *s)
{
  SystemdJournalSourceDriver *self = (SystemdJournalSourceDriver *)s;
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super.super);
  self->reader = journal_reader_new(self->journald);

  journal_reader_options_init(&self->reader_options, cfg, self->super.super.group);

  journal_reader_set_options((LogPipe *)self->reader, &self->super.super.super,  &self->reader_options, 0, SCS_JOURNALD, self->super.super.id, "journal");

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

static gboolean
__skip_old_messages(LogSrcDriver *s, GlobalConfig *cfg)
{
  SystemdJournalSourceDriver *self = (SystemdJournalSourceDriver *)s;
  gboolean result;
  if (!generate_persist_file && self->reader_options.super.read_old_records)
    {
      return TRUE;
    }
  JournalReader *reader = journal_reader_new(self->journald);
  result = journal_reader_skip_old_messages(reader, cfg);
  log_pipe_unref((LogPipe *)reader);
  return result;
}

LogDriver *
systemd_journal_sd_new()
{
  SystemdJournalSourceDriver *self = g_new0(SystemdJournalSourceDriver, 1);
  log_src_driver_init_instance(&self->super);
  self->super.super.super.init = __init;
  self->super.super.super.deinit = __deinit;
  self->super.super.super.free_fn = __free;
  self->super.skip_old_messages = __skip_old_messages;
  journal_reader_options_defaults(&self->reader_options);
  self->journald = journald_new();
  return &self->super.super;
}
