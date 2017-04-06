/*
 * Copyright (c) 2010-2014 Balabit
 * Copyright (c) 2010-2014 Viktor Juhasz <viktor.juhasz@balabit.com>
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


#ifndef SYSTEMD_JOURNAL_H_
#define SYSTEMD_JOURNAL_H_

#include "driver.h"
#include "logsource.h"
#include "journal-reader.h"

typedef struct _SystemdJournalSourceDriver SystemdJournalSourceDriver;

LogDriver *systemd_journal_sd_new(GlobalConfig *cfg);

LogSourceOptions *systemd_journal_get_source_options(LogDriver *s);
JournalReaderOptions *systemd_journal_get_reader_options(LogDriver *s);

#endif /* SYSTEMD_JOURNAL_H_ */
