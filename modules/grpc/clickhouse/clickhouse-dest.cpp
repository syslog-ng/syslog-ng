/*
 * Copyright (c) 2024 Axoflow
 * Copyright (c) 2024 Attila Szakacs <attila.szakacs@axoflow.com>
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

#include "clickhouse-dest.hpp"
#include "clickhouse-dest-worker.hpp"
#include "grpc-dest-worker.hpp"

#include "compat/cpp-start.h"
#include "messages.h"
#include "compat/cpp-end.h"

#include <map>

using syslogng::grpc::clickhouse::DestDriver;
using google::protobuf::FieldDescriptorProto;

DestDriver::DestDriver(GrpcDestDriver *s)
  : syslogng::grpc::DestDriver(s),
    schema(2, "clickhouse_message.proto", "MessageType", map_schema_type,
           &this->template_options, &this->super->super.super.super.super)
{
  this->url = "localhost:9100";
  this->enable_dynamic_headers();
}

bool
DestDriver::init()
{
  if (this->database.empty() || this->table.empty() || this->user.empty())
    {
      msg_error("Error initializing ClickHouse destination, database(), table() and user() are mandatory options",
                log_pipe_location_tag(&this->super->super.super.super.super));
      return false;
    }

  std::string quoted_table;
  if (!this->quote_identifier(this->table, quoted_table))
    return false;

  this->query = "INSERT INTO " + quoted_table + " FORMAT Protobuf";

  if (!this->schema.init())
    return false;

  if (this->schema.empty())
    {
      msg_error("Error initializing ClickHouse destination, schema() or protobuf-schema() is empty",
                log_pipe_location_tag(&this->super->super.super.super.super));
      return false;
    }

  return syslogng::grpc::DestDriver::init();
}

const gchar *
DestDriver::generate_persist_name()
{
  static gchar persist_name[1024];

  LogPipe *s = &this->super->super.super.super.super;
  if (s->persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "clickhouse.%s", s->persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "clickhouse(%s,%s,%s)",
               this->url.c_str(), this->database.c_str(), this->table.c_str());

  return persist_name;
}

const gchar *
DestDriver::format_stats_key(StatsClusterKeyBuilder *kb)
{
  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("driver", "clickhouse"));
  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("url", this->url.c_str()));
  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("database", this->database.c_str()));
  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("table", this->table.c_str()));

  return nullptr;
}

LogThreadedDestWorker *
DestDriver::construct_worker(int worker_index)
{
  GrpcDestWorker *worker = grpc_dw_new(this->super, worker_index);
  worker->cpp = new DestWorker(worker);
  return &worker->super;
}

bool
DestDriver::map_schema_type(const std::string &type_in, google::protobuf::FieldDescriptorProto::Type &type_out)
{
  using fd = google::protobuf::FieldDescriptorProto;
  /* TODO: E2E test these. */

  /*
   * https://clickhouse.com/docs/en/interfaces/schema-inference#protobuf
   * https://clickhouse.com/docs/en/sql-reference/data-types
   */

  static const std::map<std::string, FieldDescriptorProto::Type> mapping =
  {
    /* https://clickhouse.com/docs/en/sql-reference/data-types/int-uint */

    { "INT8",               static_cast<FieldDescriptorProto::Type>(fd::TYPE_INT32) },
    { "TINYINT",            static_cast<FieldDescriptorProto::Type>(fd::TYPE_INT32) },
    { "INT1",               static_cast<FieldDescriptorProto::Type>(fd::TYPE_INT32) },
    { "TINYINT SIGNED",     static_cast<FieldDescriptorProto::Type>(fd::TYPE_INT32) },
    { "INT1 SIGNED",        static_cast<FieldDescriptorProto::Type>(fd::TYPE_INT32) },
    { "INT16",              static_cast<FieldDescriptorProto::Type>(fd::TYPE_INT32) },
    { "SMALLINT",           static_cast<FieldDescriptorProto::Type>(fd::TYPE_INT32) },
    { "SMALLINT SIGNED",    static_cast<FieldDescriptorProto::Type>(fd::TYPE_INT32) },
    { "INT32",              static_cast<FieldDescriptorProto::Type>(fd::TYPE_INT32) },
    { "INT",                static_cast<FieldDescriptorProto::Type>(fd::TYPE_INT32) },
    { "INTEGER",            static_cast<FieldDescriptorProto::Type>(fd::TYPE_INT32) },
    { "MEDIUMINT",          static_cast<FieldDescriptorProto::Type>(fd::TYPE_INT32) },
    { "MEDIUMINT SIGNED",   static_cast<FieldDescriptorProto::Type>(fd::TYPE_INT32) },
    { "INT SIGNED",         static_cast<FieldDescriptorProto::Type>(fd::TYPE_INT32) },
    { "INTEGER SIGNED",     static_cast<FieldDescriptorProto::Type>(fd::TYPE_INT32) },

    { "INT64",              static_cast<FieldDescriptorProto::Type>(fd::TYPE_INT64) },
    { "BIGINT",             static_cast<FieldDescriptorProto::Type>(fd::TYPE_INT64) },
    { "SIGNED",             static_cast<FieldDescriptorProto::Type>(fd::TYPE_INT64) },
    { "BIGINT SIGNED",      static_cast<FieldDescriptorProto::Type>(fd::TYPE_INT64) },
    { "TIME",               static_cast<FieldDescriptorProto::Type>(fd::TYPE_INT64) },

    { "UINT8",              static_cast<FieldDescriptorProto::Type>(fd::TYPE_BOOL) },
    { "TINYINT UNSIGNED",   static_cast<FieldDescriptorProto::Type>(fd::TYPE_BOOL) },
    { "INT1 UNSIGNED",      static_cast<FieldDescriptorProto::Type>(fd::TYPE_BOOL) },
    { "UINT16",             static_cast<FieldDescriptorProto::Type>(fd::TYPE_UINT32) },
    { "SMALLINT UNSIGNED",  static_cast<FieldDescriptorProto::Type>(fd::TYPE_UINT32) },
    { "UINT32",             static_cast<FieldDescriptorProto::Type>(fd::TYPE_UINT32) },
    { "MEDIUMINT UNSIGNED", static_cast<FieldDescriptorProto::Type>(fd::TYPE_UINT32) },
    { "INT UNSIGNED",       static_cast<FieldDescriptorProto::Type>(fd::TYPE_UINT32) },
    { "INTEGER UNSIGNED",   static_cast<FieldDescriptorProto::Type>(fd::TYPE_UINT32) },

    { "UINT64",             static_cast<FieldDescriptorProto::Type>(fd::TYPE_UINT64) },
    { "UNSIGNED",           static_cast<FieldDescriptorProto::Type>(fd::TYPE_UINT64) },
    { "BIGINT UNSIGNED",    static_cast<FieldDescriptorProto::Type>(fd::TYPE_UINT64) },
    { "BIT",                static_cast<FieldDescriptorProto::Type>(fd::TYPE_UINT64) },
    { "SET",                static_cast<FieldDescriptorProto::Type>(fd::TYPE_UINT64) },

    /* https://clickhouse.com/docs/en/sql-reference/data-types/float */

    { "FLOAT32",            static_cast<FieldDescriptorProto::Type>(fd::TYPE_FLOAT) },
    { "FLOAT",              static_cast<FieldDescriptorProto::Type>(fd::TYPE_FLOAT) },
    { "REAL",               static_cast<FieldDescriptorProto::Type>(fd::TYPE_FLOAT) },
    { "SINGLE",             static_cast<FieldDescriptorProto::Type>(fd::TYPE_FLOAT) },
    { "FLOAT64",            static_cast<FieldDescriptorProto::Type>(fd::TYPE_DOUBLE) },
    { "DOUBLE",             static_cast<FieldDescriptorProto::Type>(fd::TYPE_DOUBLE) },
    { "DOUBLE PRECISION",   static_cast<FieldDescriptorProto::Type>(fd::TYPE_DOUBLE) },

    /* https://clickhouse.com/docs/en/sql-reference/data-types/decimal */

    /* Skipped. */

    /* https://clickhouse.com/docs/en/sql-reference/data-types/boolean */

    { "BOOL",               static_cast<FieldDescriptorProto::Type>(fd::TYPE_BOOL) },

    /* https://clickhouse.com/docs/en/sql-reference/data-types/string */

    { "STRING",                          static_cast<FieldDescriptorProto::Type>(fd::TYPE_STRING) },
    { "LONGTEXT",                        static_cast<FieldDescriptorProto::Type>(fd::TYPE_STRING) },
    { "MEDIUMTEXT",                      static_cast<FieldDescriptorProto::Type>(fd::TYPE_STRING) },
    { "TINYTEXT",                        static_cast<FieldDescriptorProto::Type>(fd::TYPE_STRING) },
    { "TEXT",                            static_cast<FieldDescriptorProto::Type>(fd::TYPE_STRING) },
    { "LONGBLOB",                        static_cast<FieldDescriptorProto::Type>(fd::TYPE_STRING) },
    { "MEDIUMBLOB",                      static_cast<FieldDescriptorProto::Type>(fd::TYPE_STRING) },
    { "TINYBLOB",                        static_cast<FieldDescriptorProto::Type>(fd::TYPE_STRING) },
    { "BLOB",                            static_cast<FieldDescriptorProto::Type>(fd::TYPE_STRING) },
    { "VARCHAR",                         static_cast<FieldDescriptorProto::Type>(fd::TYPE_STRING) },
    { "CHAR",                            static_cast<FieldDescriptorProto::Type>(fd::TYPE_STRING) },
    { "CHAR LARGE OBJECT",               static_cast<FieldDescriptorProto::Type>(fd::TYPE_STRING) },
    { "CHAR VARYING",                    static_cast<FieldDescriptorProto::Type>(fd::TYPE_STRING) },
    { "CHARACTER LARGE OBJECT",          static_cast<FieldDescriptorProto::Type>(fd::TYPE_STRING) },
    { "CHARACTER VARYING",               static_cast<FieldDescriptorProto::Type>(fd::TYPE_STRING) },
    { "NCHAR LARGE OBJECT",              static_cast<FieldDescriptorProto::Type>(fd::TYPE_STRING) },
    { "NCHAR VARYING",                   static_cast<FieldDescriptorProto::Type>(fd::TYPE_STRING) },
    { "NATIONAL CHARACTER LARGE OBJECT", static_cast<FieldDescriptorProto::Type>(fd::TYPE_STRING) },
    { "NATIONAL CHARACTER VARYING",      static_cast<FieldDescriptorProto::Type>(fd::TYPE_STRING) },
    { "NATIONAL CHAR VARYING",           static_cast<FieldDescriptorProto::Type>(fd::TYPE_STRING) },
    { "NATIONAL CHARACTER",              static_cast<FieldDescriptorProto::Type>(fd::TYPE_STRING) },
    { "NATIONAL CHAR",                   static_cast<FieldDescriptorProto::Type>(fd::TYPE_STRING) },
    { "BINARY LARGE OBJECT",             static_cast<FieldDescriptorProto::Type>(fd::TYPE_STRING) },
    { "BINARY VARYING",                  static_cast<FieldDescriptorProto::Type>(fd::TYPE_STRING) },

    /* https://clickhouse.com/docs/en/sql-reference/data-types/date */
    /* https://clickhouse.com/docs/en/sql-reference/data-types/date32 */
    /* https://clickhouse.com/docs/en/sql-reference/data-types/datetime */
    /* https://clickhouse.com/docs/en/sql-reference/data-types/datetime64 */

    { "DATE",               static_cast<FieldDescriptorProto::Type>(fd::TYPE_UINT32) },
    { "DATE32",             static_cast<FieldDescriptorProto::Type>(fd::TYPE_INT32) },
    { "DATETIME",           static_cast<FieldDescriptorProto::Type>(fd::TYPE_UINT32) },
    { "DATETIME64",         static_cast<FieldDescriptorProto::Type>(fd::TYPE_UINT64) },

    /* https://clickhouse.com/docs/en/sql-reference/data-types/object-data-type */

    /* Deprecated, skipped. */

    /* https://clickhouse.com/docs/en/sql-reference/data-types/uuid */

    { "UUID",               static_cast<FieldDescriptorProto::Type>(fd::TYPE_STRING) },

    /* https://clickhouse.com/docs/en/sql-reference/data-types/enum */

    { "ENUM",               static_cast<FieldDescriptorProto::Type>(fd::TYPE_STRING) },
    { "ENUM8",              static_cast<FieldDescriptorProto::Type>(fd::TYPE_STRING) },
    { "ENUM16",             static_cast<FieldDescriptorProto::Type>(fd::TYPE_STRING) },

    /* https://clickhouse.com/docs/en/sql-reference/data-types/lowcardinality */
    /* https://clickhouse.com/docs/en/sql-reference/data-types/array */
    /* https://clickhouse.com/docs/en/sql-reference/data-types/map */
    /* https://clickhouse.com/docs/en/sql-reference/data-types/simpleaggregatefunction */
    /* https://clickhouse.com/docs/en/sql-reference/data-types/aggregatefunction */
    /* https://clickhouse.com/docs/en/sql-reference/data-types/nested-data-structures/nested */
    /* https://clickhouse.com/docs/en/sql-reference/data-types/tuple */
    /* https://clickhouse.com/docs/en/sql-reference/data-types/nullable */

    /* Skipped. */

    /* https://clickhouse.com/docs/en/sql-reference/data-types/ipv4 */
    /* https://clickhouse.com/docs/en/sql-reference/data-types/ipv6 */

    { "IPV4",               static_cast<FieldDescriptorProto::Type>(fd::TYPE_STRING) },
    { "IPV6",               static_cast<FieldDescriptorProto::Type>(fd::TYPE_STRING) },

    /* https://clickhouse.com/docs/en/sql-reference/data-types/geo */
    /* https://clickhouse.com/docs/en/sql-reference/data-types/special-data-types/expression */
    /* https://clickhouse.com/docs/en/sql-reference/data-types/special-data-types/set */
    /* https://clickhouse.com/docs/en/sql-reference/data-types/special-data-types/nothing */
    /* https://clickhouse.com/docs/en/sql-reference/data-types/special-data-types/interval */

    /* Skipped. */
  };

  /* default */
  if (type_in.empty())
    {
      type_out = fd::TYPE_STRING;
      return true;
    }

  std::string type_upper = type_in;
  std::transform(type_upper.begin(), type_upper.end(), type_upper.begin(), [](auto c)
  {
    return ::toupper(c);
  });

  try
    {
      type_out = mapping.at(type_upper);
    }
  catch (const std::out_of_range &)
    {
      return false;
    }

  return true;
}

bool
DestDriver::quote_identifier(const std::string &identifier, std::string &quoted_identifier)
{
  /* https://clickhouse.com/docs/en/sql-reference/syntax#identifiers */

  bool has_backtick = identifier.find('`') != std::string::npos;
  bool has_double_quotes = identifier.find('"') != std::string::npos;

  if (has_backtick && has_double_quotes)
    {
      msg_error("Error quoting identifier, identifier contains both backtick and double quotes",
                log_pipe_location_tag(&this->super->super.super.super.super),
                evt_tag_str("identifier", identifier.c_str()));
      return false;
    }

  char quote_char = has_backtick ? '"' : '`';
  quoted_identifier.assign(quote_char + identifier + quote_char);
  return true;
}


/* C Wrappers */

DestDriver *
clickhouse_dd_get_cpp(GrpcDestDriver *self)
{
  return (DestDriver *) self->cpp;
}

void
clickhouse_dd_set_database(LogDriver *d, const gchar *database)
{
  GrpcDestDriver *self = (GrpcDestDriver *) d;
  DestDriver *cpp = clickhouse_dd_get_cpp(self);
  cpp->set_database(database);
}

void
clickhouse_dd_set_table(LogDriver *d, const gchar *table)
{
  GrpcDestDriver *self = (GrpcDestDriver *) d;
  DestDriver *cpp = clickhouse_dd_get_cpp(self);
  cpp->set_table(table);
}

void
clickhouse_dd_set_user(LogDriver *d, const gchar *user)
{
  GrpcDestDriver *self = (GrpcDestDriver *) d;
  DestDriver *cpp = clickhouse_dd_get_cpp(self);
  cpp->set_user(user);
}

void
clickhouse_dd_set_password(LogDriver *d, const gchar *password)
{
  GrpcDestDriver *self = (GrpcDestDriver *) d;
  DestDriver *cpp = clickhouse_dd_get_cpp(self);
  cpp->set_password(password);
}

LogDriver *
clickhouse_dd_new(GlobalConfig *cfg)
{
  GrpcDestDriver *self = grpc_dd_new(cfg, "clickhouse");
  self->cpp = new DestDriver(self);
  return &self->super.super.super;
}
