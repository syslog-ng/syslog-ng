/*
 * Copyright (c) 2024 Attila Szakacs
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

#ifndef OTEL_LOGMSG_HANDLES_HPP
#define OTEL_LOGMSG_HANDLES_HPP

#include "syslog-ng.h"

#include "otel-logmsg-handles.h"

#include "compat/cpp-start.h"
#include "logmsg/logmsg.h"
#include "compat/cpp-end.h"

namespace syslogng {
namespace grpc {
namespace otel {
namespace logmsg_handle {

extern NVHandle RAW_TYPE;
extern NVHandle TYPE;

extern NVHandle RAW_RESOURCE;
extern NVHandle RAW_RESOURCE_SCHEMA_URL;
extern NVHandle RESOURCE_DROPPED_ATTRIBUTES_COUNT;
extern NVHandle RESOURCE_SCHEMA_URL;

extern NVHandle RAW_SCOPE;
extern NVHandle RAW_SCOPE_SCHEMA_URL;
extern NVHandle SCOPE_DROPPED_ATTRIBUTES_COUNT;
extern NVHandle SCOPE_NAME;
extern NVHandle SCOPE_SCHEMA_URL;
extern NVHandle SCOPE_VERSION;

extern NVHandle RAW_LOG;
extern NVHandle LOG_BODY;
extern NVHandle LOG_DROPPED_ATTRIBUTES_COUNT;
extern NVHandle LOG_FLAGS;
extern NVHandle LOG_OBSERVED_TIME_UNIX_NANO;
extern NVHandle LOG_SEVERITY_NUMBER;
extern NVHandle LOG_SEVERITY_TEXT;
extern NVHandle LOG_SPAN_ID;
extern NVHandle LOG_TIME_UNIX_NANO;
extern NVHandle LOG_TRACE_ID;

extern NVHandle RAW_METRIC;
extern NVHandle METRIC_DATA_EXPONENTIAL_HISTOGRAM_AGGREGATION_TEMPORALITY;
extern NVHandle METRIC_DATA_HISTOGRAM_AGGREGATION_TEMPORALITY;
extern NVHandle METRIC_DATA_SUM_AGGREGATION_TEMPORALITY;
extern NVHandle METRIC_DATA_SUM_IS_MONOTONIC;
extern NVHandle METRIC_DATA_TYPE;
extern NVHandle METRIC_DESCRIPTION;
extern NVHandle METRIC_NAME;
extern NVHandle METRIC_UNIT;

extern NVHandle RAW_SPAN;
extern NVHandle SPAN_DROPPED_ATTRIBUTES_COUNT;
extern NVHandle SPAN_DROPPED_EVENTS_COUNT;
extern NVHandle SPAN_DROPPED_LINKS_COUNT;
extern NVHandle SPAN_END_TIME_UNIX_NANO;
extern NVHandle SPAN_KIND;
extern NVHandle SPAN_NAME;
extern NVHandle SPAN_PARENT_SPAN_ID;
extern NVHandle SPAN_SPAN_ID;
extern NVHandle SPAN_START_TIME_UNIX_NANO;
extern NVHandle SPAN_STATUS_CODE;
extern NVHandle SPAN_STATUS_MESSAGE;
extern NVHandle SPAN_TRACE_ID;
extern NVHandle SPAN_TRACE_STATE;

}
}
}
}

#endif
