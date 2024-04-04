/*
 * Copyright (c) 2002-2014 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
 * Copyright (c) 2020 Balázs Scheidler <bazsi77@gmail.com>
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
#ifndef CFG_GRAMMAR_INTERNAL_H_INCLUDED
#define CFG_GRAMMAR_INTERNAL_H_INCLUDED 1

#include "syslog-ng.h"

#include "driver.h"
#include "logreader.h"
#include "logwriter.h"
#include "logmatcher.h"
#include "parser/parser-expr.h"
#include "filter/filter-expr.h"
#include "value-pairs/value-pairs.h"
#include "rewrite/rewrite-expr.h"
#include "logproto/logproto.h"
#include "afinter.h"
#include "str-utils.h"
#include "logscheduler-pipe.h"

#include "filter/filter-expr-parser.h"
#include "filter/filter-pipe.h"
#include "filterx/filterx-parser.h"
#include "filterx/filterx-expr.h"
#include "filterx/filterx-pipe.h"
#include "parser/parser-expr-parser.h"
#include "rewrite/rewrite-expr-parser.h"
#include "block-ref-parser.h"
#include "template/user-function.h"
#include "cfg-block.h"
#include "cfg-path.h"
#include "multi-line/multi-line-factory.h"
#include "metrics/metrics-template.h"

#include "logthrsource/logthrfetcherdrv.h"
#include "logthrdest/logthrdestdrv.h"

#include "stats/stats.h"
#include "healthcheck/healthcheck-stats.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/* uses struct declarations instead of the typedefs to avoid having to
 * include logreader/logwriter/driver.h, which defines the typedefs.  This
 * is to avoid including unnecessary dependencies into grammars that are not
 * themselves reader/writer based */

extern LogSourceOptions *last_source_options;
extern LogReaderOptions *last_reader_options;
extern LogProtoServerOptions *last_proto_server_options;
extern LogProtoClientOptions *last_proto_client_options;
extern LogWriterOptions *last_writer_options;
extern FilePermOptions *last_file_perm_options;
extern MsgFormatOptions *last_msg_format_options;
extern LogDriver *last_driver;
extern LogSchedulerOptions *last_scheduler_options;
extern LogParser *last_parser;
extern FilterExprNode *last_filter_expr;
extern LogTemplateOptions *last_template_options;
extern ValuePairs *last_value_pairs;
extern ValuePairsTransformSet *last_vp_transset;
extern LogMatcherOptions *last_matcher_options;
extern HostResolveOptions *last_host_resolve_options;
extern StatsOptions *last_stats_options;
extern HealthCheckStatsOptions *last_healthcheck_options;
extern LogRewrite *last_rewrite;
extern CfgArgs *last_block_args;
extern DNSCacheOptions *last_dns_cache_options;
extern MultiLineOptions *last_multi_line_options;
extern MetricsTemplate *last_metrics_template;


#endif
