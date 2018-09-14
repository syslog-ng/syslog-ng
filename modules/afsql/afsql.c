/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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

#include "afsql.h"

#include "logqueue.h"
#include "template/templates.h"
#include "messages.h"
#include "string-list.h"
#include "str-format.h"
#include "seqnum.h"
#include "stats/stats-registry.h"
#include "apphook.h"
#include "timeutils.h"
#include "mainloop-worker.h"
#include "str-utils.h"

#include <string.h>
#include <errno.h>
#include <openssl/md5.h>

static gboolean dbi_initialized = FALSE;
static const char *s_oracle = "oracle";
static const char *s_freetds = "freetds";
static dbi_inst dbi_instance;

#define MAX_FAILED_ATTEMPTS 3

void
afsql_dd_add_dbd_option(LogDriver *s, const gchar *name, const gchar *value)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;

  g_hash_table_insert(self->dbd_options, g_strdup(name), g_strdup(value));
}

void
afsql_dd_add_dbd_option_numeric(LogDriver *s, const gchar *name, gint value)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;

  g_hash_table_insert(self->dbd_options_numeric, g_strdup(name), GINT_TO_POINTER(value));
}

void
afsql_dd_set_type(LogDriver *s, const gchar *type)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;

  g_free(self->type);
  if (strcmp(type, "mssql") == 0)
    type = s_freetds;
  self->type = g_strdup(type);
}

void
afsql_dd_set_host(LogDriver *s, const gchar *host)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;

  g_free(self->host);
  self->host = g_strdup(host);
}

gboolean
afsql_dd_check_port(const gchar *port)
{
  /* only digits (->numbers) are allowed */
  int len = strlen(port);
  for (int i = 0; i < len; ++i)
    if (port[i] < '0' || port[i] > '9')
      return FALSE;
  return TRUE;
}

void
afsql_dd_set_port(LogDriver *s, const gchar *port)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;

  g_free(self->port);
  self->port = g_strdup(port);
}

void
afsql_dd_set_user(LogDriver *s, const gchar *user)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;

  g_free(self->user);
  self->user = g_strdup(user);
}

void
afsql_dd_set_password(LogDriver *s, const gchar *password)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;

  g_free(self->password);
  self->password = g_strdup(password);
}

void
afsql_dd_set_database(LogDriver *s, const gchar *database)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;

  g_free(self->database);
  self->database = g_strdup(database);
}

void
afsql_dd_set_table(LogDriver *s, const gchar *table)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;

  log_template_compile(self->table, table, NULL);
}

void
afsql_dd_set_columns(LogDriver *s, GList *columns)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;

  string_list_free(self->columns);
  self->columns = columns;
}

void
afsql_dd_set_indexes(LogDriver *s, GList *indexes)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;

  string_list_free(self->indexes);
  self->indexes = indexes;
}

void
afsql_dd_set_values(LogDriver *s, GList *values)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;

  string_list_free(self->values);
  self->values = values;
}

void
afsql_dd_set_null_value(LogDriver *s, const gchar *null)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;

  if (self->null_value)
    g_free(self->null_value);
  self->null_value = g_strdup(null);
}

void
afsql_dd_set_retries(LogDriver *s, gint num_retries)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;
  self->num_retries = num_retries;
}

void
afsql_dd_set_ignore_tns_config(LogDriver *s, const gboolean ignore_tns_config)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;

  self->ignore_tns_config = ignore_tns_config;
}

void
afsql_dd_set_flush_lines(LogDriver *s, gint flush_lines)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;

  self->flush_lines = flush_lines;
}

void
afsql_dd_set_flush_timeout(LogDriver *s, gint flush_timeout)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;

  self->flush_timeout = flush_timeout;
}

void
afsql_dd_set_session_statements(LogDriver *s, GList *session_statements)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;

  self->session_statements = session_statements;
}

void
afsql_dd_set_flags(LogDriver *s, gint flags)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;

  self->flags = flags;
}

void
afsql_dd_set_create_statement_append(LogDriver *s, const gchar *create_statement_append)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;

  g_free(self->create_statement_append);
  self->create_statement_append = g_strdup(create_statement_append);
}

/**
 * afsql_dd_run_query:
 *
 * Run an SQL query on the connected database.
 *
 * NOTE: This function can only be called from the database thread.
 **/
static gboolean
afsql_dd_run_query(AFSqlDestDriver *self, const gchar *query, gboolean silent, dbi_result *result)
{
  dbi_result db_res;

  msg_debug("Running SQL query",
            evt_tag_str("query", query));

  db_res = dbi_conn_query(self->dbi_ctx, query);
  if (!db_res)
    {
      const gchar *dbi_error;

      if (!silent)
        {
          dbi_conn_error(self->dbi_ctx, &dbi_error);
          msg_error("Error running SQL query",
                    evt_tag_str("type", self->type),
                    evt_tag_str("host", self->host),
                    evt_tag_str("port", self->port),
                    evt_tag_str("user", self->user),
                    evt_tag_str("database", self->database),
                    evt_tag_str("error", dbi_error),
                    evt_tag_str("query", query));
        }
      return FALSE;
    }
  if (result)
    *result = db_res;
  else
    dbi_result_free(db_res);
  return TRUE;
}

/**
 * afsql_dd_handle_transaction_error:
 *
 * Handle errors inside during a SQL transaction (e.g. INSERT or COMMIT failures).
 *
 * NOTE: This function can only be called from the database thread.
 **/
static void
afsql_dd_handle_transaction_error(AFSqlDestDriver *self)
{
  log_queue_rewind_backlog_all(self->queue);
  self->flush_lines_queued = 0;
}

/**
 * afsql_dd_commit_transaction:
 *
 * Commit SQL transaction.
 *
 * NOTE: This function can only be called from the database thread.
 **/
static gboolean
afsql_dd_commit_transaction(AFSqlDestDriver *self)
{
  gboolean success;

  if (!self->transaction_active)
    return TRUE;

  success = afsql_dd_run_query(self, "COMMIT", FALSE, NULL);
  if (success)
    {
      log_queue_ack_backlog(self->queue, self->flush_lines_queued);
      self->flush_lines_queued = 0;
      self->transaction_active = FALSE;
    }
  else
    {
      msg_error("SQL transaction commit failed, rewinding backlog and starting again");
      afsql_dd_handle_transaction_error(self);
    }
  return success;
}

/**
 * afsql_dd_begin_transaction:
 *
 * Begin SQL transaction.
 *
 * NOTE: This function can only be called from the database thread.
 **/
static gboolean
afsql_dd_begin_transaction(AFSqlDestDriver *self)
{
  gboolean success = TRUE;
  const char *s_begin = "BEGIN";
  if (!strcmp(self->type, s_freetds))
    {
      /* the mssql requires this command */
      s_begin = "BEGIN TRANSACTION";
    }

  if (strcmp(self->type, s_oracle) != 0)
    {
      /* oracle db has no BEGIN TRANSACTION command, it implicitly starts one, after every commit. */
      success = afsql_dd_run_query(self, s_begin, FALSE, NULL);
    }

  self->transaction_active = success;

  return success;
}

static gboolean
afsql_dd_rollback_transaction(AFSqlDestDriver *self)
{
  if (!self->transaction_active)
    return TRUE;

  self->transaction_active = FALSE;

  return afsql_dd_run_query(self, "ROLLBACK", FALSE, NULL);
}

static gboolean
afsql_dd_begin_new_transaction(AFSqlDestDriver *self)
{
  if (self->transaction_active)
    {
      if (!afsql_dd_commit_transaction(self))
        {
          afsql_dd_rollback_transaction(self);
          return FALSE;
        }
    }

  return afsql_dd_begin_transaction(self);
}

static gboolean _sql_identifier_is_valid_char(gchar c)
{
  return ((c == '.') ||
          (c == '_') ||
          (c >= '0' && c <= '9') ||
          (g_ascii_tolower(c) >= 'a' && g_ascii_tolower(c) <= 'z'));
}

static gboolean
_is_sql_identifier_sanitized(const gchar *token)
{
  gint i;

  for (i = 0; token[i]; i++)
    {
      if (!_sql_identifier_is_valid_char(token[i]))
        return FALSE;
    }

  return TRUE;
}

static void
_sanitize_sql_identifier(gchar *token)
{
  gint i;

  for (i = 0; token[i]; i++)
    {
      if (!_sql_identifier_is_valid_char(token[i]))
        token[i] = '_';
    }
}

/**
 * afsql_dd_create_index:
 *
 * This function creates an index for the column specified and returns
 * TRUE to indicate success.
 *
 * NOTE: This function can only be called from the database thread.
 **/
static gboolean
afsql_dd_create_index(AFSqlDestDriver *self, const gchar *table, const gchar *column)
{
  GString *query_string;
  gboolean success = TRUE;

  query_string = g_string_sized_new(64);

  if (strcmp(self->type, s_oracle) == 0)
    {
      /* NOTE: oracle index indentifier length is max 30 characters
       * so we use the first 30 characters of the table_column md5 hash */
      if ((strlen(table) + strlen(column)) > 25)
        {

          guchar hash[MD5_DIGEST_LENGTH];
          gchar hash_str[31];
          gchar *cat = g_strjoin("_", table, column, NULL);

          MD5((guchar *)cat, strlen(cat), hash);
          g_free(cat);

          format_hex_string(hash, sizeof(hash), hash_str, sizeof(hash_str));
          hash_str[0] = 'i';
          g_string_printf(query_string, "CREATE INDEX %s ON %s (%s)",
                          hash_str, table, column);
        }
      else
        g_string_printf(query_string, "CREATE INDEX %s_%s_idx ON %s (%s)",
                        table, column, table, column);
    }
  else
    g_string_printf(query_string, "CREATE INDEX %s_%s_idx ON %s (%s)",
                    table, column, table, column);
  if (!afsql_dd_run_query(self, query_string->str, FALSE, NULL))
    {
      msg_error("Error adding missing index",
                evt_tag_str("table", table),
                evt_tag_str("column", column));
      success = FALSE;
    }
  g_string_free(query_string, TRUE);
  return success;
}

static inline gboolean
_is_table_syslogng_conform(AFSqlDestDriver *self, const gchar *table)
{
  return (g_hash_table_lookup(self->syslogng_conform_tables, table) != NULL);
}

static inline void
_remember_table_as_syslogng_conform(AFSqlDestDriver *self, const gchar *table)
{
  g_hash_table_insert(self->syslogng_conform_tables, g_strdup(table), GUINT_TO_POINTER(TRUE));
}

static gboolean
_is_table_present(AFSqlDestDriver *self, const gchar *table, dbi_result *metadata)
{
  gboolean res = FALSE;
  GString *query_string;

  if (!afsql_dd_begin_new_transaction(self))
    {
      msg_error("Starting new transaction has failed");

      return FALSE;
    }

  query_string = g_string_sized_new(32);
  g_string_printf(query_string, "SELECT * FROM %s WHERE 0=1", table);
  res = afsql_dd_run_query(self, query_string->str, TRUE, metadata);
  g_string_free(query_string, TRUE);

  afsql_dd_commit_transaction(self);

  return res;
}

static gboolean
_ensure_table_is_syslogng_conform(AFSqlDestDriver *self, dbi_result db_res, const gchar *table)
{
  gboolean success = TRUE;
  gboolean new_transaction_started = FALSE;
  gint i;
  GString *query_string = g_string_sized_new(32);

  for (i = 0; success && (i < self->fields_len); i++)
    {
      if (dbi_result_get_field_idx(db_res, self->fields[i].name) == 0)
        {
          GList *l;
          if (!new_transaction_started)
            {
              if (!afsql_dd_begin_new_transaction(self))
                {
                  msg_error("Starting new transaction for modifying(ALTER) table has failed",
                            evt_tag_str("table", table));
                  success = FALSE;
                  break;
                }
              new_transaction_started = TRUE;
            }
          /* field does not exist, add this column */
          g_string_printf(query_string, "ALTER TABLE %s ADD %s %s", table, self->fields[i].name, self->fields[i].type);
          if (!afsql_dd_run_query(self, query_string->str, FALSE, NULL))
            {
              msg_error("Error adding missing column, giving up",
                        evt_tag_str("table", table),
                        evt_tag_str("column", self->fields[i].name));
              success = FALSE;
              break;
            }
          for (l = self->indexes; l; l = l->next)
            {
              if (strcmp((gchar *) l->data, self->fields[i].name) == 0)
                {
                  /* this is an indexed column, create index */
                  afsql_dd_create_index(self, table, self->fields[i].name);
                }
            }
        }
    }

  if (new_transaction_started && ( !success || !afsql_dd_commit_transaction(self)))
    {
      afsql_dd_rollback_transaction(self);
      success = FALSE;
    }

  g_string_free(query_string, TRUE);

  return success;
}

static gboolean
_table_create_indexes(AFSqlDestDriver *self, const gchar *table)
{
  gboolean success = TRUE;
  GList *l;

  if (!afsql_dd_begin_new_transaction(self))
    {
      msg_error("Starting new transaction for table creation has failed",
                evt_tag_str("table", table));
      return FALSE;
    }

  for (l = self->indexes; l && success; l = l->next)
    {
      success = afsql_dd_create_index(self, table, (gchar *) l->data);
    }

  if (!success || !afsql_dd_commit_transaction(self))
    {
      afsql_dd_rollback_transaction(self);
    }

  return success;
}

static gboolean
_table_create(AFSqlDestDriver *self, const gchar *table)
{
  gint i;
  GString *query_string = g_string_sized_new(32);
  gboolean success = FALSE;
  if (!afsql_dd_begin_new_transaction(self))
    {
      msg_error("Starting new transaction for table creation has failed",
                evt_tag_str("table", table));
      return FALSE;
    }

  g_string_printf(query_string, "CREATE TABLE %s (", table);
  for (i = 0; i < self->fields_len; i++)
    {
      g_string_append_printf(query_string, "%s %s", self->fields[i].name, self->fields[i].type);
      if (i != self->fields_len - 1)
        g_string_append(query_string, ", ");
    }
  g_string_append(query_string, ")");
  if (self->create_statement_append)
    g_string_append(query_string, self->create_statement_append);
  if (afsql_dd_run_query(self, query_string->str, FALSE, NULL))
    {
      success = TRUE;
    }
  else
    {
      msg_error("Error creating table, giving up",
                evt_tag_str("table", table));
    }

  if (!success || !afsql_dd_commit_transaction(self))
    {
      afsql_dd_rollback_transaction(self);
    }

  g_string_free(query_string, TRUE);

  return success;
}

/**
 * afsql_dd_validate_table:
 *
 * Check if the given table exists in the database. If it doesn't
 * create it, if it does, check if all the required fields are
 * present and create them if they don't.
 *
 * NOTE: This function can only be called from the database thread.
 **/
static gboolean
afsql_dd_ensure_table_is_syslogng_conform(AFSqlDestDriver *self, GString *table)
{
  dbi_result db_res = NULL;
  gboolean success = FALSE;

  if (self->flags & AFSQL_DDF_DONT_CREATE_TABLES)
    return TRUE;

  _sanitize_sql_identifier(table->str);

  if (_is_table_syslogng_conform(self, table->str))
    return TRUE;

  if (_is_table_present(self, table->str, &db_res))
    {
      /* table exists, check structure */
      success = _ensure_table_is_syslogng_conform(self, db_res, table->str);
      if (db_res)
        dbi_result_free(db_res);
    }
  else
    {
      /* table does not exist, create it */
      success = _table_create(self, table->str) && _table_create_indexes(self, table->str);
    }

  if (success)
    {
      /* we have successfully created/altered the destination table, record this information */
      _remember_table_as_syslogng_conform(self, table->str);
    }

  return success;
}


/**
 * afsql_dd_suspend:
 * timeout: in milliseconds
 *
 * This function is assumed to be called from the database thread
 * only!
 **/
static void
afsql_dd_suspend(AFSqlDestDriver *self)
{
  self->db_thread_suspended = TRUE;
  g_get_current_time(&self->db_thread_suspend_target);
  g_time_val_add(&self->db_thread_suspend_target, self->time_reopen * 1000 * 1000); /* the timeout expects microseconds */
}

static void
afsql_dd_disconnect(AFSqlDestDriver *self)
{
  dbi_conn_close(self->dbi_ctx);
  self->dbi_ctx = NULL;
  g_hash_table_remove_all(self->syslogng_conform_tables);
}

static void
afsql_dd_set_dbd_opt(gpointer key, gpointer value, gpointer user_data)
{
  dbi_conn_set_option((dbi_conn)user_data, (gchar *)key, (gchar *)value);
}

static void
afsql_dd_set_dbd_opt_numeric(gpointer key, gpointer value, gpointer user_data)
{
  dbi_conn_set_option_numeric((dbi_conn)user_data, (gchar *)key,
                              GPOINTER_TO_INT(value));
}

static gboolean
afsql_dd_ensure_initialized_connection(AFSqlDestDriver *self)
{
  if (self->dbi_ctx)
    return TRUE;

  self->dbi_ctx = dbi_conn_new_r(self->type, dbi_instance);

  if (!self->dbi_ctx)
    {
      msg_error("No such DBI driver",
                evt_tag_str("type", self->type));
      return FALSE;
    }

  dbi_conn_set_option(self->dbi_ctx, "host", self->host);

  if (strcmp(self->type, "mysql"))
    dbi_conn_set_option(self->dbi_ctx, "port", self->port);
  else
    dbi_conn_set_option_numeric(self->dbi_ctx, "port", atoi(self->port));

  dbi_conn_set_option(self->dbi_ctx, "username", self->user);
  dbi_conn_set_option(self->dbi_ctx, "password", self->password);
  dbi_conn_set_option(self->dbi_ctx, "dbname", self->database);
  dbi_conn_set_option(self->dbi_ctx, "encoding", self->encoding);
  dbi_conn_set_option(self->dbi_ctx, "auto-commit", self->flags & AFSQL_DDF_EXPLICIT_COMMITS ? "false" : "true");

  /* database specific hacks */
  dbi_conn_set_option(self->dbi_ctx, "sqlite_dbdir", "");
  dbi_conn_set_option(self->dbi_ctx, "sqlite3_dbdir", "");

  if (strcmp(self->type, s_oracle) == 0)
    dbi_conn_set_option_numeric(self->dbi_ctx, "oracle_ignore_tns_config", self->ignore_tns_config);

  /* Set user-specified options */
  g_hash_table_foreach(self->dbd_options, afsql_dd_set_dbd_opt, self->dbi_ctx);
  g_hash_table_foreach(self->dbd_options_numeric, afsql_dd_set_dbd_opt_numeric, self->dbi_ctx);

  if (dbi_conn_connect(self->dbi_ctx) < 0)
    {
      const gchar *dbi_error;

      dbi_conn_error(self->dbi_ctx, &dbi_error);

      msg_error("Error establishing SQL connection",
                evt_tag_str("type", self->type),
                evt_tag_str("host", self->host),
                evt_tag_str("port", self->port),
                evt_tag_str("username", self->user),
                evt_tag_str("database", self->database),
                evt_tag_str("error", dbi_error));

      return FALSE;
    }

  if (self->session_statements != NULL)
    {
      GList *l;

      for (l = self->session_statements; l; l = l->next)
        {
          if (!afsql_dd_run_query(self, (gchar *) l->data, FALSE, NULL))
            {
              msg_error("Error executing SQL connection statement",
                        evt_tag_str("statement", (gchar *) l->data));

              return FALSE;
            }
        }
    }

  return TRUE;
}

static GString *
afsql_dd_ensure_accessible_database_table(AFSqlDestDriver *self, LogMessage *msg)
{
  GString *table = g_string_sized_new(32);

  log_template_format(self->table, msg, &self->template_options, LTZ_LOCAL, 0, NULL, table);

  if (!afsql_dd_ensure_table_is_syslogng_conform(self, table))
    {
      /* If validate table is FALSE then close the connection and wait time_reopen time (next call) */
      msg_error("Error checking table, disconnecting from database, trying again shortly",
                evt_tag_int("time_reopen", self->time_reopen));
      g_string_free(table, TRUE);
      return NULL;
    }

  return table;
}

static GString *
afsql_dd_build_insert_command(AFSqlDestDriver *self, LogMessage *msg, GString *table)
{
  GString *insert_command = g_string_sized_new(256);
  GString *value = g_string_sized_new(512);
  gint i, j;

  g_string_printf(insert_command, "INSERT INTO %s (", table->str);

  for (i = 0; i < self->fields_len; i++)
    {
      if ((self->fields[i].flags & AFSQL_FF_DEFAULT) == 0 && self->fields[i].value != NULL)
        {
          g_string_append(insert_command, self->fields[i].name);

          j = i + 1;
          while (j < self->fields_len && (self->fields[j].flags & AFSQL_FF_DEFAULT) == AFSQL_FF_DEFAULT)
            j++;

          if (j < self->fields_len)
            g_string_append(insert_command, ", ");
        }
    }

  g_string_append(insert_command, ") VALUES (");

  for (i = 0; i < self->fields_len; i++)
    {
      gchar *quoted;

      if ((self->fields[i].flags & AFSQL_FF_DEFAULT) == 0 && self->fields[i].value != NULL)
        {
          log_template_format(self->fields[i].value, msg, &self->template_options, LTZ_SEND, self->seq_num, NULL, value);
          if (self->null_value && strcmp(self->null_value, value->str) == 0)
            {
              g_string_append(insert_command, "NULL");
            }
          else
            {
              dbi_conn_quote_string_copy(self->dbi_ctx, value->str, &quoted);
              if (quoted)
                {
                  g_string_append(insert_command, quoted);
                  free(quoted);
                }
              else
                {
                  g_string_append(insert_command, "''");
                }
            }

          j = i + 1;
          while (j < self->fields_len && (self->fields[j].flags & AFSQL_FF_DEFAULT) == AFSQL_FF_DEFAULT)
            j++;
          if (j < self->fields_len)
            g_string_append(insert_command, ", ");
        }
    }

  g_string_append(insert_command, ")");

  g_string_free(value, TRUE);

  return insert_command;
}

static inline gboolean
afsql_dd_is_transaction_handling_enabled(const AFSqlDestDriver *self)
{
  return self->flush_lines_queued != -1;
}

static inline gboolean
afsql_dd_should_begin_new_transaction(const AFSqlDestDriver *self)
{
  return self->flush_lines_queued == 0;
}

static inline gboolean
afsql_dd_should_commit_transaction(const AFSqlDestDriver *self)
{
  return afsql_dd_is_transaction_handling_enabled(self) && self->flush_lines_queued == self->flush_lines;
}

static inline void
afsql_dd_rollback_msg(AFSqlDestDriver *self, LogMessage *msg, LogPathOptions *path_options)
{
  if (self->flags & AFSQL_DDF_EXPLICIT_COMMITS)
    log_queue_rewind_backlog(self->queue, 1);
  else
    log_queue_push_head(self->queue, msg, path_options);
}

static inline gboolean
afsql_dd_handle_insert_row_error_depending_on_connection_availability(AFSqlDestDriver *self,
    LogMessage *msg,
    LogPathOptions *path_options)
{
  const gchar *dbi_error, *error_message;

  if (dbi_conn_ping(self->dbi_ctx) == 1)
    {
      afsql_dd_rollback_msg(self, msg, path_options);
      return TRUE;
    }

  if (afsql_dd_is_transaction_handling_enabled(self))
    {
      error_message = "SQL connection lost in the middle of a transaction,"
                      " rewinding backlog and starting again";
      afsql_dd_handle_transaction_error(self);
    }
  else
    {
      error_message = "Error, no SQL connection after failed query attempt";
      afsql_dd_rollback_msg(self, msg, path_options);
    }

  dbi_conn_error(self->dbi_ctx, &dbi_error);
  msg_error(error_message,
            evt_tag_str("type", self->type),
            evt_tag_str("host", self->host),
            evt_tag_str("port", self->port),
            evt_tag_str("username", self->user),
            evt_tag_str("database", self->database),
            evt_tag_str("error", dbi_error));

  return FALSE;
}

/**
 * afsql_dd_insert_db:
 *
 * This function is running in the database thread
 *
 * Returns: FALSE to indicate that the connection should be closed and
 * this destination suspended for time_reopen() time.
 **/
static gboolean
afsql_dd_insert_db(AFSqlDestDriver *self)
{
  GString *table = NULL;
  GString *insert_command = NULL;
  LogMessage *msg;
  gboolean success = TRUE;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

  if (!afsql_dd_ensure_initialized_connection(self))
    return FALSE;

  /* connection established, try to insert a message */
  msg = log_queue_pop_head(self->queue, &path_options);
  if (!msg)
    return TRUE;

  msg_set_context(msg);

  table = afsql_dd_ensure_accessible_database_table(self, msg);

  if (!table)
    {
      success = FALSE;
      goto out;
    }

  if (afsql_dd_should_begin_new_transaction(self) && !afsql_dd_begin_transaction(self))
    {
      success = FALSE;
      goto out;
    }

  insert_command = afsql_dd_build_insert_command(self, msg, table);
  success = afsql_dd_run_query(self, insert_command->str, FALSE, NULL);

  if (success && self->flush_lines_queued != -1)
    {
      self->flush_lines_queued++;

      if (afsql_dd_should_commit_transaction(self) && !afsql_dd_commit_transaction(self))
        {
          /* Assuming that in case of error, the queue is rewound by afsql_dd_commit_transaction() */
          afsql_dd_rollback_transaction(self);

          g_string_free(insert_command, TRUE);
          msg_set_context(NULL);

          success = FALSE;
        }
    }

out:

  if (table != NULL)
    g_string_free(table, TRUE);

  if (insert_command != NULL)
    g_string_free(insert_command, TRUE);

  msg_set_context(NULL);

  if (success)
    {
      log_msg_ack(msg, &path_options, AT_PROCESSED);
      log_msg_unref(msg);
      step_sequence_number(&self->seq_num);
      self->failed_message_counter = 0;
    }
  else
    {
      if (self->failed_message_counter < self->num_retries)
        {
          if (!afsql_dd_handle_insert_row_error_depending_on_connection_availability(self, msg, &path_options))
            return FALSE;

          self->failed_message_counter++;
        }
      else
        {
          msg_error("Multiple failures while inserting this record into the database, message dropped",
                    evt_tag_int("attempts", self->num_retries));
          stats_counter_inc(self->dropped_messages);
          log_msg_drop(msg, &path_options, AT_PROCESSED);
          self->failed_message_counter = 0;
          success = TRUE;
        }
    }

  return success;
}

static void
afsql_dd_message_became_available_in_the_queue(gpointer user_data)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) user_data;

  g_mutex_lock(self->db_thread_mutex);
  g_cond_signal(self->db_thread_wakeup_cond);
  g_mutex_unlock(self->db_thread_mutex);
}

/* assumes that db_thread_mutex is held */
static void
afsql_dd_wait_for_suspension_wakeup(AFSqlDestDriver *self)
{
  /* we got suspended, probably because of a connection error,
   * during this time we only get wakeups if we need to be
   * terminated. */
  if (!self->db_thread_terminate)
    g_cond_timed_wait(self->db_thread_wakeup_cond, self->db_thread_mutex, &self->db_thread_suspend_target);
  self->db_thread_suspended = FALSE;
}

/**
 * afsql_dd_database_thread:
 *
 * This is the thread inserting records into the database.
 **/
static void
afsql_dd_database_thread(gpointer arg)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) arg;

  msg_verbose("Database thread started",
              evt_tag_str("driver", self->super.super.id));
  while (!self->db_thread_terminate)
    {
      main_loop_worker_run_gc();
      g_mutex_lock(self->db_thread_mutex);
      if (self->db_thread_suspended)
        {
          afsql_dd_wait_for_suspension_wakeup(self);
          /* we loop back to check if the thread was requested to terminate */
        }
      else if (!log_queue_check_items(self->queue, NULL, afsql_dd_message_became_available_in_the_queue, self, NULL))
        {
          /* we have nothing to INSERT into the database, let's wait we get some new stuff */

          if (self->flush_lines_queued > 0)
            {
              if (!afsql_dd_commit_transaction(self))
                {
                  if (!afsql_dd_rollback_transaction(self))
                    {
                      afsql_dd_disconnect(self);
                      afsql_dd_suspend(self);
                    }
                  g_mutex_unlock(self->db_thread_mutex);
                  continue;
                }
            }
          else if (!self->db_thread_terminate)
            {
              g_cond_wait(self->db_thread_wakeup_cond, self->db_thread_mutex);
            }

          /* we loop back to check if the thread was requested to terminate */
        }
      g_mutex_unlock(self->db_thread_mutex);

      if (self->db_thread_terminate)
        break;

      if (!afsql_dd_insert_db(self))
        {
          afsql_dd_disconnect(self);
          afsql_dd_suspend(self);
        }
    }

  while (log_queue_get_length(self->queue) > 0)
    {
      if (!afsql_dd_insert_db(self))
        {
          goto exit;
        }
    }

  if (self->flush_lines_queued > 0)
    {
      /* we can't do anything with the return value here. if commit isn't
       * successful, we get our backlog back, but we have no chance
       * submitting that back to the SQL engine.
       */

      if (!afsql_dd_commit_transaction(self))
        afsql_dd_rollback_transaction(self);
    }
exit:
  afsql_dd_disconnect(self);

  msg_verbose("Database thread finished",
              evt_tag_str("driver", self->super.super.id));
}

static void
afsql_dd_stop_thread(gpointer s)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;
  g_mutex_lock(self->db_thread_mutex);
  self->db_thread_terminate = TRUE;
  g_cond_signal(self->db_thread_wakeup_cond);
  g_mutex_unlock(self->db_thread_mutex);
}

static void
afsql_dd_start_thread(AFSqlDestDriver *self)
{
  self->db_thread_terminate = FALSE;
  main_loop_create_worker_thread(afsql_dd_database_thread, afsql_dd_stop_thread, self, &self->worker_options);
}


static const gchar *
afsql_dd_format_stats_instance(AFSqlDestDriver *self)
{
  static gchar persist_name[64];

  g_snprintf(persist_name, sizeof(persist_name),
             "%s,%s,%s,%s,%s",
             self->type, self->host, self->port, self->database, self->table->template);
  return persist_name;
}

static inline const gchar *
afsql_dd_format_persist_name(const LogPipe *s)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *)s;
  static gchar persist_name[256];

  if (s->persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "afsql_dd.%s", s->persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "afsql_dd(%s,%s,%s,%s,%s)", self->type,
               self->host, self->port, self->database, self->table->template);

  return persist_name;
}

static inline gchar *
afsql_dd_format_persist_sequence_number(AFSqlDestDriver *self)
{
  static gchar persist_name[256];

  g_snprintf(persist_name, sizeof(persist_name),
             "afsql_dd_sequence_number(%s,%s,%s,%s,%s)",
             self->type,self->host, self->port, self->database, self->table->template);

  return persist_name;
}

static gboolean
afsql_dd_init(LogPipe *s)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);
  gint len_cols, len_values;

  if (!log_dest_driver_init_method(s))
    return FALSE;

  if (!self->columns || !self->values)
    {
      msg_error("Default columns and values must be specified for database destinations",
                evt_tag_str("type", self->type));
      return FALSE;
    }

  if (self->ignore_tns_config && strcmp(self->type, s_oracle) != 0)
    {
      msg_warning("WARNING: Option ignore_tns_config was skipped because database type is not Oracle",
                  evt_tag_str("type", self->type));
    }

  stats_lock();
  {
    StatsClusterKey sc_key;
    stats_cluster_logpipe_key_set(&sc_key, SCS_SQL | SCS_DESTINATION, self->super.super.id,
                                  afsql_dd_format_stats_instance(self) );
    stats_register_counter(0, &sc_key, SC_TYPE_QUEUED, &self->queued_messages);
    stats_register_counter(0, &sc_key, SC_TYPE_DROPPED, &self->dropped_messages);
    stats_register_counter(STATS_LEVEL1, &sc_key, SC_TYPE_MEMORY_USAGE, &self->memory_usage);
  }
  stats_unlock();

  self->seq_num = GPOINTER_TO_INT(cfg_persist_config_fetch(cfg, afsql_dd_format_persist_sequence_number(self)));
  if (!self->seq_num)
    init_sequence_number(&self->seq_num);

  self->queue = log_dest_driver_acquire_queue(&self->super,
                                              afsql_dd_format_persist_name((const LogPipe *)self));
  if (self->queue == NULL)
    {
      return FALSE;
    }
  else
    {
      if (self->flags & AFSQL_DDF_EXPLICIT_COMMITS)
        log_queue_set_use_backlog(self->queue, TRUE);
    }
  log_queue_set_counters(self->queue, self->queued_messages, self->dropped_messages, self->memory_usage);
  log_queue_init_counters(self->queue);
  if (!self->fields)
    {
      GList *col, *value;
      gint i;

      len_cols = g_list_length(self->columns);
      len_values = g_list_length(self->values);
      if (len_cols != len_values)
        {
          msg_error("The number of columns and values do not match",
                    evt_tag_int("len_columns", len_cols),
                    evt_tag_int("len_values", len_values));
          goto error;
        }
      self->fields_len = len_cols;
      self->fields = g_new0(AFSqlField, len_cols);

      for (i = 0, col = self->columns, value = self->values; col && value; i++, col = col->next, value = value->next)
        {
          gchar *space;

          space = strchr(col->data, ' ');
          if (space)
            {
              self->fields[i].name = g_strndup(col->data, space - (gchar *) col->data);
              while (*space == ' ')
                space++;
              if (*space != '\0')
                self->fields[i].type = g_strdup(space);
              else
                self->fields[i].type = g_strdup("text");
            }
          else
            {
              self->fields[i].name = g_strdup(col->data);
              self->fields[i].type = g_strdup("text");
            }
          if (!_is_sql_identifier_sanitized(self->fields[i].name))
            {
              msg_error("Column name is not a proper SQL name",
                        evt_tag_str("column", self->fields[i].name));
              return FALSE;
            }

          if (GPOINTER_TO_UINT(value->data) > 4096)
            {
              self->fields[i].value = log_template_new(cfg, NULL);
              log_template_compile(self->fields[i].value, (gchar *) value->data, NULL);
            }
          else
            {
              switch (GPOINTER_TO_UINT(value->data))
                {
                case AFSQL_COLUMN_DEFAULT:
                  self->fields[i].flags |= AFSQL_FF_DEFAULT;
                  break;
                default:
                  g_assert_not_reached();
                  break;
                }
            }
        }
    }

  self->time_reopen = cfg->time_reopen;

  log_template_options_init(&self->template_options, cfg);

  if (self->flush_lines == -1)
    self->flush_lines = cfg->flush_lines;
  if (self->flush_timeout == -1)
    self->flush_timeout = cfg->flush_timeout;

  if ((self->flags & AFSQL_DDF_EXPLICIT_COMMITS) && (self->flush_lines > 0 || self->flush_timeout > 0))
    self->flush_lines_queued = 0;

  if (!dbi_initialized)
    {
      errno = 0;
      gint rc = dbi_initialize_r(NULL, &dbi_instance);

      if (rc < 0)
        {
          /* NOTE: errno might be unreliable, but that's all we have */
          msg_error("Unable to initialize database access (DBI)",
                    evt_tag_int("rc", rc),
                    evt_tag_error("error"));
          goto error;
        }
      else if (rc == 0)
        {
          msg_error("The database access library (DBI) reports no usable SQL drivers, perhaps DBI drivers are not installed properly");
          goto error;
        }
      else
        {
          dbi_initialized = TRUE;
        }
    }

  afsql_dd_start_thread(self);
  return TRUE;

error:

  stats_lock();
  {
    StatsClusterKey sc_key;
    stats_cluster_logpipe_key_set(&sc_key, SCS_SQL | SCS_DESTINATION, self->super.super.id,
                                  afsql_dd_format_stats_instance(self) );
    stats_unregister_counter(&sc_key, SC_TYPE_QUEUED, &self->queued_messages);
    stats_unregister_counter(&sc_key, SC_TYPE_DROPPED, &self->dropped_messages);
    stats_unregister_counter(&sc_key, SC_TYPE_MEMORY_USAGE, &self->memory_usage);
  }
  stats_unlock();

  return FALSE;
}

static gboolean
afsql_dd_deinit(LogPipe *s)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;

  log_queue_reset_parallel_push(self->queue);

  log_queue_set_counters(self->queue, NULL, NULL, NULL);
  cfg_persist_config_add(log_pipe_get_config(s), afsql_dd_format_persist_sequence_number(self),
                         GINT_TO_POINTER(self->seq_num), NULL, FALSE);

  stats_lock();
  StatsClusterKey sc_key;
  stats_cluster_logpipe_key_set(&sc_key, SCS_SQL | SCS_DESTINATION, self->super.super.id,
                                afsql_dd_format_stats_instance(self) );
  stats_unregister_counter(&sc_key, SC_TYPE_QUEUED, &self->queued_messages);
  stats_unregister_counter(&sc_key, SC_TYPE_DROPPED, &self->dropped_messages);
  stats_unlock();

  if (!log_dest_driver_deinit_method(s))
    return FALSE;

  return TRUE;
}

static void
afsql_dd_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;
  LogPathOptions local_options;

  if (!path_options->flow_control_requested)
    path_options = log_msg_break_ack(msg, path_options, &local_options);

  log_msg_add_ack(msg, path_options);
  log_queue_push_tail(self->queue, log_msg_ref(msg), path_options);
  log_dest_driver_queue_method(s, msg, path_options);
}

static void
afsql_dd_free(LogPipe *s)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;
  gint i;

  log_template_options_destroy(&self->template_options);
  for (i = 0; i < self->fields_len; i++)
    {
      g_free(self->fields[i].name);
      g_free(self->fields[i].type);
      log_template_unref(self->fields[i].value);
    }

  g_free(self->fields);
  g_free(self->type);
  g_free(self->host);
  g_free(self->port);
  g_free(self->user);
  g_free(self->password);
  g_free(self->database);
  g_free(self->encoding);
  g_free(self->create_statement_append);
  if (self->null_value)
    g_free(self->null_value);
  string_list_free(self->columns);
  string_list_free(self->indexes);
  string_list_free(self->values);
  log_template_unref(self->table);
  g_hash_table_destroy(self->syslogng_conform_tables);
  g_hash_table_destroy(self->dbd_options);
  g_hash_table_destroy(self->dbd_options_numeric);
  if (self->session_statements)
    string_list_free(self->session_statements);
  g_mutex_free(self->db_thread_mutex);
  g_cond_free(self->db_thread_wakeup_cond);
  log_dest_driver_free(s);
}

LogDriver *
afsql_dd_new(GlobalConfig *cfg)
{
  AFSqlDestDriver *self = g_new0(AFSqlDestDriver, 1);

  log_dest_driver_init_instance(&self->super, cfg);

  self->super.super.super.init = afsql_dd_init;
  self->super.super.super.deinit = afsql_dd_deinit;
  self->super.super.super.queue = afsql_dd_queue;
  self->super.super.super.free_fn = afsql_dd_free;
  self->super.super.super.generate_persist_name = afsql_dd_format_persist_name;

  self->type = g_strdup("mysql");
  self->host = g_strdup("");
  self->port = g_strdup("");
  self->user = g_strdup("syslog-ng");
  self->password = g_strdup("");
  self->database = g_strdup("logs");
  self->encoding = g_strdup("UTF-8");
  self->transaction_active = FALSE;
  self->ignore_tns_config = FALSE;

  self->table = log_template_new(configuration, NULL);
  log_template_compile(self->table, "messages", NULL);
  self->failed_message_counter = 0;

  self->flush_lines = -1;
  self->flush_timeout = -1;
  self->flush_lines_queued = -1;
  self->session_statements = NULL;
  self->num_retries = MAX_FAILED_ATTEMPTS;

  self->syslogng_conform_tables = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  self->dbd_options = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  self->dbd_options_numeric = g_hash_table_new_full(g_str_hash, g_int_equal, g_free, NULL);

  log_template_options_defaults(&self->template_options);

  self->db_thread_wakeup_cond = g_cond_new();
  self->db_thread_mutex = g_mutex_new();

  self->worker_options.is_output_thread = TRUE;
  return &self->super.super;
}

gint
afsql_dd_lookup_flag(const gchar *flag)
{
  if (strcmp(flag, "explicit-commits") == 0)
    return AFSQL_DDF_EXPLICIT_COMMITS;
  else if (strcmp(flag, "dont-create-tables") == 0)
    return AFSQL_DDF_DONT_CREATE_TABLES;
  else
    msg_warning("Unknown SQL flag",
                evt_tag_str("flag", flag));
  return 0;
}
