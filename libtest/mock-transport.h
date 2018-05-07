/*
 * Copyright (c) 2012 Balabit
 * Copyright (c) 2012 Balázs Scheidler
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

#ifndef MOCK_TRANSPORT_H_INCLUDED
#define MOCK_TRANSPORT_H_INCLUDED 1

#include "transport/logtransport.h"

typedef struct _LogTransportMock LogTransportMock;

/* macro to be used when injecting an error in the I/O stream */
#define LTM_INJECT_ERROR_LENGTH -2
#define LTM_INJECT_ERROR(err)   (GUINT_TO_POINTER(err)), LTM_INJECT_ERROR_LENGTH
/* macro to be used at the end of the I/O stream */
#define LTM_EOF                 NULL, 0
#define LTM_PADDING   "padd", -1, "padd", -1, "padd", -1, "padd", -1, "padd", -1

LogTransport *
log_transport_mock_stream_new(const gchar *read_buffer1, gssize read_buffer_length1, ...);

LogTransport *
log_transport_mock_endless_stream_new(const gchar *read_buffer1, gssize read_buffer_length1, ...);

LogTransport *
log_transport_mock_records_new(const gchar *read_buffer1, gssize read_buffer_length1, ...);

LogTransport *
log_transport_mock_endless_records_new(const gchar *read_buffer1, gssize read_buffer_length1, ...);

void
log_transport_mock_inject_data(LogTransportMock *self, const gchar *buffer, gssize length);

gpointer
log_transport_mock_get_user_data(LogTransportMock *self);
void
log_transport_mock_set_user_data(LogTransportMock *self, gpointer user_data);

gssize
log_transport_mock_read_from_write_buffer(LogTransportMock *self, gchar *buffer, gsize len);

#endif
