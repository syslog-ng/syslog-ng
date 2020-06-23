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

#include "cfg-grammar-internal.h"

LogDriver *last_driver;
LogParser *last_parser;
LogSourceOptions *last_source_options;
LogProtoServerOptions *last_proto_server_options;
LogProtoClientOptions *last_proto_client_options;
LogReaderOptions *last_reader_options;
LogWriterOptions *last_writer_options;
MsgFormatOptions *last_msg_format_options;
FilePermOptions *last_file_perm_options;
LogTemplateOptions *last_template_options;
LogTemplate *last_template;
CfgArgs *last_block_args;
ValuePairs *last_value_pairs;
ValuePairsTransformSet *last_vp_transset;
LogMatcherOptions *last_matcher_options;
HostResolveOptions *last_host_resolve_options;
StatsOptions *last_stats_options;
DNSCacheOptions *last_dns_cache_options;
LogRewrite *last_rewrite;
CfgArgs *last_block_args;
DNSCacheOptions *last_dns_cache_options;
