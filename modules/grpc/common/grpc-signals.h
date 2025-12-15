/*
 * Copyright (c) 2025 Databricks
 * Copyright (c) 2025 David Tosovic <david.tosovic@databricks.com>
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

#ifndef SIGNAL_SLOT_CONNECTOR_GRPC_SIGNALS_H_INCLUDED
#define SIGNAL_SLOT_CONNECTOR_GRPC_SIGNALS_H_INCLUDED

#include "signal-slot-connector/signal-slot-connector.h"
#include <glib.h>

typedef struct _GrpcMetadataRequestSignalData GrpcMetadataRequestSignalData;
typedef struct _GrpcResponseReceivedSignalData GrpcResponseReceivedSignalData;

typedef enum
{
  GRPC_SLOT_SUCCESS,
  GRPC_SLOT_RESOLVED,
  GRPC_SLOT_CRITICAL_ERROR,
  GRPC_SLOT_PLUGIN_ERROR,
} GrpcSlotResultType;

struct _GrpcMetadataRequestSignalData
{
  GrpcSlotResultType result;
  GList *metadata_list;
};

struct _GrpcResponseReceivedSignalData
{
  GrpcSlotResultType result;
  gint error_code;
};

#define signal_grpc_metadata_request SIGNAL(grpc, metadata_request, GrpcMetadataRequestSignalData *)

#define signal_grpc_response_received SIGNAL(grpc, response_received, GrpcResponseReceivedSignalData *)

#endif

