/*
 * Copyright (c) 2024 Axoflow
 * Copyright (c) 2023-2024 Attila Szakacs <attila.szakacs@axoflow.com>
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

#ifndef GRPC_PARSER_H
#define GRPC_PARSER_H

#define GRPC_KEYWORDS \
  { "insecure",                  KW_INSECURE }, \
  { "tls",                       KW_TLS }, \
  { "key_file",                  KW_KEY_FILE }, \
  { "cert_file",                 KW_CERT_FILE }, \
  { "ca_file",                   KW_CA_FILE }, \
  { "auth",                      KW_AUTH }, \
  { "alts",                      KW_ALTS }, \
  { "url",                       KW_URL }, \
  { "target_service_accounts",   KW_TARGET_SERVICE_ACCOUNTS }, \
  { "adc",                       KW_ADC }, \
  { "service_account",           KW_SERVICE_ACCOUNT }, \
  { "key",                       KW_KEY }, \
  { "token_validity_duration",   KW_TOKEN_VALIDITY_DURATION }, \
  { "compression",               KW_COMPRESSION }, \
  { "batch_bytes",               KW_BATCH_BYTES }, \
  { "channel_args",              KW_CHANNEL_ARGS }, \
  { "headers",                   KW_HEADERS }, \
  { "schema",                    KW_SCHEMA }, \
  { "protobuf_schema",           KW_PROTOBUF_SCHEMA }, \
  { "keep_alive",                KW_KEEP_ALIVE }, \
  { "time",                      KW_TIME }, \
  { "timeout",                   KW_TIMEOUT }, \
  { "max_pings_without_data",    KW_MAX_PINGS_WITHOUT_DATA }, \
  { "port",                      KW_PORT }, \
  { "peer_verify",               KW_PEER_VERIFY }, \
  { "optional_untrusted",        KW_OPTIONAL_UNTRUSTED }, \
  { "optional_trusted",          KW_OPTIONAL_TRUSTED }, \
  { "required_untrusted",        KW_REQUIRED_UNTRUSTED }, \
  { "required_trusted",          KW_REQUIRED_TRUSTED }, \
  { "concurrent_requests",       KW_CONCURRENT_REQUESTS }, \
  { "response_action",           KW_RESPONSE_ACTION }, \
  { "disconnect",                KW_DISCONNECT }, \
  { "drop",                      KW_DROP }, \
  { "retry",                     KW_RETRY }, \
  { "success",                   KW_SUCCESS }, \
  { "ok",                        KW_OK }, \
  { "unavailable",               KW_UNAVAILABLE }, \
  { "cancelled",                 KW_CANCELLED }, \
  { "deadline_exceeded",         KW_DEADLINE_EXCEEDED }, \
  { "aborted",                   KW_ABORTED }, \
  { "out_of_range",              KW_OUT_OF_RANGE }, \
  { "data_loss",                 KW_DATA_LOSS }, \
  { "unknown",                   KW_UNKNOWN }, \
  { "invalid_argument",          KW_INVALID_ARGUMENT }, \
  { "not_found",                 KW_NOT_FOUND }, \
  { "already_exists",            KW_ALREADY_EXISTS }, \
  { "permission_denied",         KW_PERMISSION_DENIED }, \
  { "unauthenticated",           KW_UNAUTHENTICATED }, \
  { "failed_precondition",       KW_FAILED_PRECONDITION }, \
  { "unimplemented",             KW_UNIMPLEMENTED }, \
  { "internal",                  KW_INTERNAL }, \
  { "resource_exhausted",        KW_RESOURCE_EXHAUSTED }

#endif
