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
  /* TODO: E2E test these. */

  /*
   * https://clickhouse.com/docs/en/interfaces/schema-inference#protobuf
   * https://clickhouse.com/docs/en/sql-reference/data-types
   */

  static const std::map<std::string, google::protobuf::FieldDescriptorProto::Type> mapping =
  {
    /* https://clickhouse.com/docs/en/sql-reference/data-types/int-uint */

    { "INT8",               FieldDescriptorProto::TYPE_INT32 },
    { "TINYINT",            FieldDescriptorProto::TYPE_INT32 },
    { "INT1",               FieldDescriptorProto::TYPE_INT32 },
    { "TINYINT SIGNED",     FieldDescriptorProto::TYPE_INT32 },
    { "INT1 SIGNED",        FieldDescriptorProto::TYPE_INT32 },
    { "INT16",              FieldDescriptorProto::TYPE_INT32 },
    { "SMALLINT",           FieldDescriptorProto::TYPE_INT32 },
    { "SMALLINT SIGNED",    FieldDescriptorProto::TYPE_INT32 },
    { "INT32",              FieldDescriptorProto::TYPE_INT32 },
    { "INT",                FieldDescriptorProto::TYPE_INT32 },
    { "INTEGER",            FieldDescriptorProto::TYPE_INT32 },
    { "MEDIUMINT",          FieldDescriptorProto::TYPE_INT32 },
    { "MEDIUMINT SIGNED",   FieldDescriptorProto::TYPE_INT32 },
    { "INT SIGNED",         FieldDescriptorProto::TYPE_INT32 },
    { "INTEGER SIGNED",     FieldDescriptorProto::TYPE_INT32 },

    { "INT64",              FieldDescriptorProto::TYPE_INT64 },
    { "BIGINT",             FieldDescriptorProto::TYPE_INT64 },
    { "SIGNED",             FieldDescriptorProto::TYPE_INT64 },
    { "BIGINT SIGNED",      FieldDescriptorProto::TYPE_INT64 },
    { "TIME",               FieldDescriptorProto::TYPE_INT64 },

    { "UINT8",              FieldDescriptorProto::TYPE_BOOL },
    { "TINYINT UNSIGNED",   FieldDescriptorProto::TYPE_BOOL },
    { "INT1 UNSIGNED",      FieldDescriptorProto::TYPE_BOOL },
    { "UINT16",             FieldDescriptorProto::TYPE_UINT32 },
    { "SMALLINT UNSIGNED",  FieldDescriptorProto::TYPE_UINT32 },
    { "UINT32",             FieldDescriptorProto::TYPE_UINT32 },
    { "MEDIUMINT UNSIGNED", FieldDescriptorProto::TYPE_UINT32 },
    { "INT UNSIGNED",       FieldDescriptorProto::TYPE_UINT32 },
    { "INTEGER UNSIGNED",   FieldDescriptorProto::TYPE_UINT32 },

    { "UINT64",             FieldDescriptorProto::TYPE_UINT64 },
    { "UNSIGNED",           FieldDescriptorProto::TYPE_UINT64 },
    { "BIGINT UNSIGNED",    FieldDescriptorProto::TYPE_UINT64 },
    { "BIT",                FieldDescriptorProto::TYPE_UINT64 },
    { "SET",                FieldDescriptorProto::TYPE_UINT64 },

    /* https://clickhouse.com/docs/en/sql-reference/data-types/float */

    { "FLOAT32",            FieldDescriptorProto::TYPE_FLOAT },
    { "FLOAT",              FieldDescriptorProto::TYPE_FLOAT },
    { "REAL",               FieldDescriptorProto::TYPE_FLOAT },
    { "SINGLE",             FieldDescriptorProto::TYPE_FLOAT },
    { "FLOAT64",            FieldDescriptorProto::TYPE_DOUBLE },
    { "DOUBLE",             FieldDescriptorProto::TYPE_DOUBLE },
    { "DOUBLE PRECISION",   FieldDescriptorProto::TYPE_DOUBLE },

    /* https://clickhouse.com/docs/en/sql-reference/data-types/decimal */

    /* Skipped. */

    /* https://clickhouse.com/docs/en/sql-reference/data-types/boolean */

    { "BOOL",               FieldDescriptorProto::TYPE_BOOL },

    /* https://clickhouse.com/docs/en/sql-reference/data-types/string */

    { "STRING",                          FieldDescriptorProto::TYPE_STRING },
    { "LONGTEXT",                        FieldDescriptorProto::TYPE_STRING },
    { "MEDIUMTEXT",                      FieldDescriptorProto::TYPE_STRING },
    { "TINYTEXT",                        FieldDescriptorProto::TYPE_STRING },
    { "TEXT",                            FieldDescriptorProto::TYPE_STRING },
    { "LONGBLOB",                        FieldDescriptorProto::TYPE_STRING },
    { "MEDIUMBLOB",                      FieldDescriptorProto::TYPE_STRING },
    { "TINYBLOB",                        FieldDescriptorProto::TYPE_STRING },
    { "BLOB",                            FieldDescriptorProto::TYPE_STRING },
    { "VARCHAR",                         FieldDescriptorProto::TYPE_STRING },
    { "CHAR",                            FieldDescriptorProto::TYPE_STRING },
    { "CHAR LARGE OBJECT",               FieldDescriptorProto::TYPE_STRING },
    { "CHAR VARYING",                    FieldDescriptorProto::TYPE_STRING },
    { "CHARACTER LARGE OBJECT",          FieldDescriptorProto::TYPE_STRING },
    { "CHARACTER VARYING",               FieldDescriptorProto::TYPE_STRING },
    { "NCHAR LARGE OBJECT",              FieldDescriptorProto::TYPE_STRING },
    { "NCHAR VARYING",                   FieldDescriptorProto::TYPE_STRING },
    { "NATIONAL CHARACTER LARGE OBJECT", FieldDescriptorProto::TYPE_STRING },
    { "NATIONAL CHARACTER VARYING",      FieldDescriptorProto::TYPE_STRING },
    { "NATIONAL CHAR VARYING",           FieldDescriptorProto::TYPE_STRING },
    { "NATIONAL CHARACTER",              FieldDescriptorProto::TYPE_STRING },
    { "NATIONAL CHAR",                   FieldDescriptorProto::TYPE_STRING },
    { "BINARY LARGE OBJECT",             FieldDescriptorProto::TYPE_STRING },
    { "BINARY VARYING",                  FieldDescriptorProto::TYPE_STRING },

    /* https://clickhouse.com/docs/en/sql-reference/data-types/date */
    /* https://clickhouse.com/docs/en/sql-reference/data-types/date32 */
    /* https://clickhouse.com/docs/en/sql-reference/data-types/datetime */
    /* https://clickhouse.com/docs/en/sql-reference/data-types/datetime64 */

    { "DATE",               FieldDescriptorProto::TYPE_UINT32 },
    { "DATE32",             FieldDescriptorProto::TYPE_INT32 },
    { "DATETIME",           FieldDescriptorProto::TYPE_UINT32 },
    { "DATETIME64",         FieldDescriptorProto::TYPE_UINT64 },

    /* https://clickhouse.com/docs/en/sql-reference/data-types/object-data-type */

    /* Deprecated, skipped. */

    /* https://clickhouse.com/docs/en/sql-reference/data-types/uuid */

    { "UUID",               FieldDescriptorProto::TYPE_STRING },

    /* https://clickhouse.com/docs/en/sql-reference/data-types/enum */

    { "ENUM",               FieldDescriptorProto::TYPE_STRING },
    { "ENUM8",              FieldDescriptorProto::TYPE_STRING },
    { "ENUM16",             FieldDescriptorProto::TYPE_STRING },

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

    { "IPV4",               FieldDescriptorProto::TYPE_STRING },
    { "IPV6",               FieldDescriptorProto::TYPE_STRING },

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
      type_out = FieldDescriptorProto::TYPE_STRING;
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
