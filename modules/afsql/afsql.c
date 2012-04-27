/*
 * Copyright (c) 2002-2012 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2012 Balázs Scheidler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
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

#include "afsql.h"

#if ENABLE_SQL

#include "logqueue.h"
#include "templates.h"
#include "messages.h"
#include "misc.h"
#include "stats.h"
#include "apphook.h"
#include "timeutils.h"

#include <dbi/dbi.h>
#include <string.h>

#if ENABLE_SSL
#include <openssl/md5.h>
#endif

/* field flags */
enum
{
  AFSQL_FF_DEFAULT = 0x0001,
};

/* destination driver flags */
enum
{
  AFSQL_DDF_EXPLICIT_COMMITS = 0x0001,
  AFSQL_DDF_DONT_CREATE_TABLES = 0x0002,
};

typedef struct _AFSqlField
{
  guint32 flags;
  gchar *name;
  gchar *type;
  LogTemplate *value;
} AFSqlField;

/**
 * AFSqlDestDriver:
 *
 * This structure encapsulates an SQL destination driver. SQL insert
 * statements are generated from a separate thread because of the blocking
 * nature of the DBI API. It is ensured that while the thread is running,
 * the reference count to the driver structure is increased, thus the db
 * thread can read any of the fields in this structure. To do anything more
 * than simple reading out a value, some kind of locking mechanism shall be
 * used.
 **/
typedef struct _AFSqlDestDriver
{
  LogDestDriver super;
  /* read by the db thread */
  gchar *type;
  gchar *host;
  gchar *port;
  gchar *user;
  gchar *password;
  gchar *database;
  gchar *encoding;
  GList *columns;
  GList *values;
  GList *indexes;
  LogTemplate *table;
  gint fields_len;
  AFSqlField *fields;
  gchar *null_value;
  gint time_reopen;
  gint num_retries;
  gint flush_lines;
  gint flush_timeout;
  gint flush_lines_queued;
  gint flags;
  GList *session_statements;

  LogTemplateOptions template_options;

  StatsCounterItem *dropped_messages;
  StatsCounterItem *stored_messages;

  GHashTable *dbd_options;
  GHashTable *dbd_options_numeric;

  /* shared by the main/db thread */
  GThread *db_thread;
  GMutex *db_thread_mutex;
  GCond *db_thread_wakeup_cond;
  gboolean db_thread_terminate;
  gboolean db_thread_suspended;
  GTimeVal db_thread_suspend_target;
  LogQueue *queue;
  /* used exclusively by the db thread */
  gint32 seq_num;
  LogMessage *pending_msg;
  gboolean pending_msg_ack_needed;
  dbi_conn dbi_ctx;
  GHashTable *validated_tables;
  guint32 failed_message_counter;
} AFSqlDestDriver;

static gboolean dbi_initialized = FALSE;
static const char *s_oracle = "oracle";
static const char *s_freetds = "freetds";

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

gboolean afsql_dd_check_port(const gchar *port)
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

  if (num_retries < 1)
    {
      self->num_retries = 1;
    }
  else
    {
      self->num_retries = num_retries;
    }
}

void
afsql_dd_set_frac_digits(LogDriver *s, gint frac_digits)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;

  self->template_options.frac_digits = frac_digits;
}

void
afsql_dd_set_send_time_zone(LogDriver *s, const gchar *send_time_zone)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;

  self->template_options.time_zone[LTZ_SEND] = g_strdup(send_time_zone);
}

void
afsql_dd_set_local_time_zone(LogDriver *s, const gchar *local_time_zone)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;

  self->template_options.time_zone[LTZ_LOCAL] = g_strdup(local_time_zone);
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
            evt_tag_str("query", query),
            NULL);

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
                    evt_tag_str("query", query),
                    NULL);
        }
      return FALSE;
    }
  if (result)
    *result = db_res;
  else
    dbi_result_free(db_res);
  return TRUE;
}

static gboolean
afsql_dd_check_sql_identifier(gchar *token, gboolean sanitize)
{
  gint i;

  for (i = 0; token[i]; i++)
    {
      if (!((token[i] == '.') || (token[i] == '_') || (i && token[i] >= '0' && token[i] <= '9') || (g_ascii_tolower(token[i]) >= 'a' && g_ascii_tolower(token[i]) <= 'z')))
        {
          if (sanitize)
            token[i] = '_';
          else
            return FALSE;
        }
    }
  return TRUE;
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
afsql_dd_create_index(AFSqlDestDriver *self, gchar *table, gchar *column)
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

#if ENABLE_SSL
          guchar hash[MD5_DIGEST_LENGTH];
          gchar hash_str[31];
          gchar *cat = g_strjoin("_", table, column, NULL);

          MD5((guchar *)cat, strlen(cat), hash);
          g_free(cat);

          format_hex_string(hash, sizeof(hash), hash_str, sizeof(hash_str));
          hash_str[0] = 'i';
          g_string_printf(query_string, "CREATE INDEX %s ON %s ('%s')",
              hash_str, table, column);
#else
          msg_warning("The name of the index would be too long for Oracle to handle and OpenSSL was not detected which would be used to generate a shorter name. Please enable SSL support in order to use this combination.",
                      evt_tag_str("table", table),
                      evt_tag_str("column", column),
                      NULL);
#endif
        }
      else
        g_string_printf(query_string, "CREATE INDEX %s_%s_idx ON %s ('%s')",
            table, column, table, column);
    }
  else
    g_string_printf(query_string, "CREATE INDEX %s_%s_idx ON %s (%s)",
                    table, column, table, column);
  if (!afsql_dd_run_query(self, query_string->str, FALSE, NULL))
    {
      msg_error("Error adding missing index",
                evt_tag_str("table", table),
                evt_tag_str("column", column),
                NULL);
      success = FALSE;
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
static GString *
afsql_dd_validate_table(AFSqlDestDriver *self, LogMessage *msg)
{
  GString *query_string, *table;
  dbi_result db_res;
  gboolean success = FALSE;
  gint i;

  table = g_string_sized_new(32);
  log_template_format(self->table, msg, &self->template_options, LTZ_LOCAL, 0, NULL, table);

  if (self->flags & AFSQL_DDF_DONT_CREATE_TABLES)
    return table;

  afsql_dd_check_sql_identifier(table->str, TRUE);

  if (g_hash_table_lookup(self->validated_tables, table->str))
    return table;

  query_string = g_string_sized_new(32);
  g_string_printf(query_string, "SELECT * FROM %s WHERE 0=1", table->str);
  if (afsql_dd_run_query(self, query_string->str, TRUE, &db_res))
    {

      /* table exists, check structure */
      success = TRUE;
      for (i = 0; success && (i < self->fields_len); i++)
        {
          if (dbi_result_get_field_idx(db_res, self->fields[i].name) == 0)
            {
              GList *l;
              /* field does not exist, add this column */
              g_string_printf(query_string, "ALTER TABLE %s ADD %s %s", table->str, self->fields[i].name, self->fields[i].type);
              if (!afsql_dd_run_query(self, query_string->str, FALSE, NULL))
                {
                  msg_error("Error adding missing column, giving up",
                            evt_tag_str("table", table->str),
                            evt_tag_str("column", self->fields[i].name),
                            NULL);
                  success = FALSE;
                  break;
                }
              for (l = self->indexes; l; l = l->next)
                {
                  if (strcmp((gchar *) l->data, self->fields[i].name) == 0)
                    {
                      /* this is an indexed column, create index */
                      afsql_dd_create_index(self, table->str, self->fields[i].name);
                    }
                }
            }
        }
      dbi_result_free(db_res);
    }
  else
    {
      /* table does not exist, create it */

      g_string_printf(query_string, "CREATE TABLE %s (", table->str);
      for (i = 0; i < self->fields_len; i++)
        {
          g_string_append_printf(query_string, "%s %s", self->fields[i].name, self->fields[i].type);
          if (i != self->fields_len - 1)
            g_string_append(query_string, ", ");
        }
      g_string_append(query_string, ")");
      if (afsql_dd_run_query(self, query_string->str, FALSE, NULL))
        {
          GList *l;

          success = TRUE;
          for (l = self->indexes; l; l = l->next)
            {
              afsql_dd_create_index(self, table->str, (gchar *) l->data);
            }
        }
      else
        {
          msg_error("Error creating table, giving up",
                    evt_tag_str("table", table->str),
                    NULL);
        }
    }
  if (success)
    {
      /* we have successfully created/altered the destination table, record this information */
      g_hash_table_insert(self->validated_tables, g_strdup(table->str), GUINT_TO_POINTER(TRUE));
    }
  else
    {
      g_string_free(table, TRUE);
      table = NULL;
    }
  g_string_free(query_string, TRUE);

  return table;
}

/**
 * afsql_dd_begin_txn:
 *
 * Begin SQL transaction.
 *
 * NOTE: This function can only be called from the database thread.
 **/
static gboolean
afsql_dd_begin_txn(AFSqlDestDriver *self)
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
  return success;
}

/**
 * afsql_dd_begin_txn:
 *
 * Commit SQL transaction.
 *
 * NOTE: This function can only be called from the database thread.
 **/
static gboolean
afsql_dd_commit_txn(AFSqlDestDriver *self, gboolean lock)
{
  gboolean success;

  success = afsql_dd_run_query(self, "COMMIT", FALSE, NULL);
  if (lock)
    g_mutex_lock(self->db_thread_mutex);

  /* FIXME: this is a workaround because of the non-proper locking semantics
   * of the LogQueue.  It might happen that the _queue() method sees 0
   * elements in the queue, while the thread is still busy processing the
   * previous message.  In that case arming the parallel push callback is
   * not needed and will cause assertions to fail.  This is ugly and should
   * be fixed by properly defining the "blocking" semantics of the LogQueue
   * object w/o having to rely on user-code messing with parallel push
   * callbacks. */

  log_queue_reset_parallel_push(self->queue);
  if (success)
    {
      log_queue_ack_backlog(self->queue, self->flush_lines_queued);
    }
  else
    {
      msg_notice("SQL transaction commit failed, rewinding backlog and starting again",
                 NULL);
      log_queue_rewind_backlog(self->queue);
    }
  if (lock)
    g_mutex_unlock(self->db_thread_mutex);
  self->flush_lines_queued = 0;
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
  g_hash_table_remove_all(self->validated_tables);
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
afsql_dd_connect(AFSqlDestDriver *self)
{
  if (self->dbi_ctx)
    return TRUE;

  self->dbi_ctx = dbi_conn_new(self->type);
  if (!self->dbi_ctx)
    {
      msg_error("No such DBI driver",
                evt_tag_str("type", self->type),
                NULL);
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
                evt_tag_str("error", dbi_error),
                NULL);
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
                        evt_tag_str("statement", (gchar *) l->data),
                        NULL);
              return FALSE;
            }
        }
    }

  return TRUE;
}

static gboolean
afsql_dd_insert_fail_handler(AFSqlDestDriver *self, LogMessage *msg,
                             LogPathOptions *path_options)
{
  if (self->failed_message_counter < self->num_retries - 1)
    {
      self->pending_msg = msg;
      self->pending_msg_ack_needed = path_options->ack_needed;

      /* database connection status sanity check after failed query */
      if (dbi_conn_ping(self->dbi_ctx) != 1)
        {
          const gchar *dbi_error;

          dbi_conn_error(self->dbi_ctx, &dbi_error);
          msg_error("Error, no SQL connection after failed query attempt",
                    evt_tag_str("type", self->type),
                    evt_tag_str("host", self->host),
                    evt_tag_str("port", self->port),
                    evt_tag_str("username", self->user),
                    evt_tag_str("database", self->database),
                    evt_tag_str("error", dbi_error),
                    NULL);
          return FALSE;
        }

      self->failed_message_counter++;
      return FALSE;
    }

  msg_error("Multiple failures while inserting this record into the database, message dropped",
            evt_tag_int("attempts", self->num_retries),
            NULL);
  stats_counter_inc(self->dropped_messages);
  log_msg_drop(msg, path_options);
  self->failed_message_counter = 0;
  return TRUE;
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
  GString *table, *query_string, *value;
  LogMessage *msg;
  gboolean success;
  gint i;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

  afsql_dd_connect(self);

  if (self->pending_msg)
    {
      msg = self->pending_msg;
      path_options.ack_needed = self->pending_msg_ack_needed;
      self->pending_msg = NULL;
    }
  else
    {
      g_mutex_lock(self->db_thread_mutex);

      /* FIXME: this is a workaround because of the non-proper locking semantics
       * of the LogQueue.  It might happen that the _queue() method sees 0
       * elements in the queue, while the thread is still busy processing the
       * previous message.  In that case arming the parallel push callback is
       * not needed and will cause assertions to fail.  This is ugly and should
       * be fixed by properly defining the "blocking" semantics of the LogQueue
       * object w/o having to rely on user-code messing with parallel push
       * callbacks. */
      log_queue_reset_parallel_push(self->queue);
      success = log_queue_pop_head(self->queue, &msg, &path_options, (self->flags & AFSQL_DDF_EXPLICIT_COMMITS), FALSE);
      g_mutex_unlock(self->db_thread_mutex);
      if (!success)
        return TRUE;
    }

  msg_set_context(msg);

  table = afsql_dd_validate_table(self, msg);
  if (!table)
    {
      /* If validate table is FALSE then close the connection and wait time_reopen time (next call) */
      msg_error("Error checking table, disconnecting from database, trying again shortly",
                evt_tag_int("time_reopen", self->time_reopen),
                NULL);
      msg_set_context(NULL);
      g_string_free(table, TRUE);
      return afsql_dd_insert_fail_handler(self, msg, &path_options);
    }

  value = g_string_sized_new(256);
  query_string = g_string_sized_new(512);

  g_string_printf(query_string, "INSERT INTO %s (", table->str);
  for (i = 0; i < self->fields_len; i++)
    {
      g_string_append(query_string, self->fields[i].name);
      if (i != self->fields_len - 1)
        g_string_append(query_string, ", ");
    }
  g_string_append(query_string, ") VALUES (");

  for (i = 0; i < self->fields_len; i++)
    {
      gchar *quoted;

      if (self->fields[i].value == NULL)
        {
          /* the config used the 'default' value for this column -> the fields[i].value is NULL, use SQL default */
          g_string_append(query_string, "DEFAULT");
        }
      else
        {
          log_template_format(self->fields[i].value, msg, &self->template_options, LTZ_SEND, self->seq_num, NULL, value);

          if (self->null_value && strcmp(self->null_value, value->str) == 0)
            {
              g_string_append(query_string, "NULL");
            }
          else
            {
              dbi_conn_quote_string_copy(self->dbi_ctx, value->str, &quoted);
              if (quoted)
                {
                  g_string_append(query_string, quoted);
                  free(quoted);
                }
              else
                {
                  g_string_append(query_string, "''");
                }
            }
        }

      if (i != self->fields_len - 1)
        g_string_append(query_string, ", ");
    }
  g_string_append(query_string, ")");

  /* we have the INSERT statement ready in query_string */

  if (self->flush_lines_queued == 0 && !afsql_dd_begin_txn(self))
    return FALSE;

  success = TRUE;
  if (!afsql_dd_run_query(self, query_string->str, FALSE, NULL))
    {
      /* error running INSERT on an already validated table, too bad. Try to reconnect. Maybe that helps. */
      success = FALSE;
    }

  if (success && self->flush_lines_queued != -1)
    {
      self->flush_lines_queued++;

      if (self->flush_lines && self->flush_lines_queued == self->flush_lines && !afsql_dd_commit_txn(self, TRUE))
        return FALSE;
    }

  g_string_free(table, TRUE);
  g_string_free(value, TRUE);
  g_string_free(query_string, TRUE);

  msg_set_context(NULL);

  if (!success)
    return afsql_dd_insert_fail_handler(self, msg, &path_options);

  /* we only ACK if each INSERT is a separate transaction */
  if ((self->flags & AFSQL_DDF_EXPLICIT_COMMITS) == 0)
    log_msg_ack(msg, &path_options);
  log_msg_unref(msg);
  step_sequence_number(&self->seq_num);
  self->failed_message_counter = 0;

  return TRUE;
}

/**
 * afsql_dd_database_thread:
 *
 * This is the thread inserting records into the database.
 **/
static gpointer
afsql_dd_database_thread(gpointer arg)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) arg;

  msg_verbose("Database thread started",
              evt_tag_str("driver", self->super.super.id),
              NULL);
  while (!self->db_thread_terminate)
    {
      g_mutex_lock(self->db_thread_mutex);
      if (self->db_thread_suspended)
        {
          /* we got suspended, probably because of a connection error,
           * during this time we only get wakeups if we need to be
           * terminated. */
          if (!self->db_thread_terminate)
            g_cond_timed_wait(self->db_thread_wakeup_cond, self->db_thread_mutex, &self->db_thread_suspend_target);
          self->db_thread_suspended = FALSE;
          g_mutex_unlock(self->db_thread_mutex);

          /* we loop back to check if the thread was requested to terminate */
        }
      else if (!self->pending_msg && log_queue_get_length(self->queue) == 0)
        {
          /* we have nothing to INSERT into the database, let's wait we get some new stuff */

          if (self->flush_lines_queued > 0 && self->flush_timeout > 0)
            {
              GTimeVal flush_target;

              g_get_current_time(&flush_target);
              g_time_val_add(&flush_target, self->flush_timeout * 1000);
              if (!self->db_thread_terminate && !g_cond_timed_wait(self->db_thread_wakeup_cond, self->db_thread_mutex, &flush_target))
                {
                  /* timeout elapsed */
                  if (!afsql_dd_commit_txn(self, FALSE))
                    {
                      afsql_dd_disconnect(self);
                      afsql_dd_suspend(self);
                      g_mutex_unlock(self->db_thread_mutex);
                      continue;
                    }
                }
            }
          else if (!self->db_thread_terminate)
            {
              g_cond_wait(self->db_thread_wakeup_cond, self->db_thread_mutex);
            }
          g_mutex_unlock(self->db_thread_mutex);

          /* we loop back to check if the thread was requested to terminate */
        }
      else
        g_mutex_unlock(self->db_thread_mutex);

      if (self->db_thread_terminate)
        break;

      if (!afsql_dd_insert_db(self))
        {
          afsql_dd_disconnect(self);
          afsql_dd_suspend(self);
        }
    }
  if (self->flush_lines_queued > 0)
    {
      /* we can't do anything with the return value here. if commit isn't
       * successful, we get our backlog back, but we have no chance
       * submitting that back to the SQL engine.
       */

      afsql_dd_commit_txn(self, TRUE);
    }

  afsql_dd_disconnect(self);

  msg_verbose("Database thread finished",
              evt_tag_str("driver", self->super.super.id),
              NULL);
  return NULL;
}

static void
afsql_dd_start_thread(AFSqlDestDriver *self)
{
  self->db_thread_wakeup_cond = g_cond_new();
  self->db_thread_mutex = g_mutex_new();
  self->db_thread = create_worker_thread(afsql_dd_database_thread, self, TRUE, NULL);
}

static void
afsql_dd_stop_thread(AFSqlDestDriver *self)
{
  g_mutex_lock(self->db_thread_mutex);
  self->db_thread_terminate = TRUE;
  g_cond_signal(self->db_thread_wakeup_cond);
  g_mutex_unlock(self->db_thread_mutex);
  g_thread_join(self->db_thread);
  g_mutex_free(self->db_thread_mutex);
  g_cond_free(self->db_thread_wakeup_cond);
}

static gchar *
afsql_dd_format_stats_instance(AFSqlDestDriver *self)
{
  static gchar persist_name[64];

  g_snprintf(persist_name, sizeof(persist_name),
             "%s,%s,%s,%s,%s",
             self->type, self->host, self->port, self->database, self->table->template);
  return persist_name;
}

static inline gchar *
afsql_dd_format_persist_name(AFSqlDestDriver *self)
{
  static gchar persist_name[256];

  g_snprintf(persist_name, sizeof(persist_name),
             "afsql_dd(%s,%s,%s,%s,%s)",
             self->type, self->host, self->port, self->database, self->table->template);
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
                evt_tag_str("database type", self->type),
                NULL);
      return FALSE;
    }

  stats_lock();
  stats_register_counter(0, SCS_SQL | SCS_DESTINATION, self->super.super.id, afsql_dd_format_stats_instance(self), SC_TYPE_STORED, &self->stored_messages);
  stats_register_counter(0, SCS_SQL | SCS_DESTINATION, self->super.super.id, afsql_dd_format_stats_instance(self), SC_TYPE_DROPPED, &self->dropped_messages);
  stats_unlock();

  self->queue = log_dest_driver_acquire_queue(&self->super, afsql_dd_format_persist_name(self));
  log_queue_set_counters(self->queue, self->stored_messages, self->dropped_messages);
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
                    evt_tag_int("len_values", len_values),
                    NULL);
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
          if (!afsql_dd_check_sql_identifier(self->fields[i].name, FALSE))
            {
              msg_error("Column name is not a proper SQL name",
                        evt_tag_str("column", self->fields[i].name),
                        NULL);
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
      gint rc = dbi_initialize(NULL);

      if (rc < 0)
        {
          /* NOTE: errno might be unreliable, but that's all we have */
          msg_error("Unable to initialize database access (DBI)",
                    evt_tag_int("rc", rc),
                    evt_tag_errno("error", errno),
                    NULL);
          goto error;
        }
      else if (rc == 0)
        {
          msg_error("The database access library (DBI) reports no usable SQL drivers, perhaps DBI drivers are not installed properly",
                    NULL);
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
  stats_unregister_counter(SCS_SQL | SCS_DESTINATION, self->super.super.id, afsql_dd_format_stats_instance(self), SC_TYPE_STORED, &self->stored_messages);
  stats_unregister_counter(SCS_SQL | SCS_DESTINATION, self->super.super.id, afsql_dd_format_stats_instance(self), SC_TYPE_DROPPED, &self->dropped_messages);
  stats_unlock();

  return FALSE;
}

static gboolean
afsql_dd_deinit(LogPipe *s)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;

  afsql_dd_stop_thread(self);

  log_queue_set_counters(self->queue, NULL, NULL);

  stats_lock();
  stats_unregister_counter(SCS_SQL | SCS_DESTINATION, self->super.super.id, afsql_dd_format_stats_instance(self), SC_TYPE_STORED, &self->stored_messages);
  stats_unregister_counter(SCS_SQL | SCS_DESTINATION, self->super.super.id, afsql_dd_format_stats_instance(self), SC_TYPE_DROPPED, &self->dropped_messages);
  stats_unlock();

  if (!log_dest_driver_deinit_method(s))
    return FALSE;

  return TRUE;
}

static void
afsql_dd_queue_notify(gpointer user_data)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) user_data;
  g_mutex_lock(self->db_thread_mutex);
  g_cond_signal(self->db_thread_wakeup_cond);
  log_queue_reset_parallel_push(self->queue);
  g_mutex_unlock(self->db_thread_mutex);
}

static void
afsql_dd_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options, gpointer user_data)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;
  gboolean queue_was_empty;
  LogPathOptions local_options;

  if (!path_options->flow_control_requested)
    path_options = log_msg_break_ack(msg, path_options, &local_options);

  g_mutex_lock(self->db_thread_mutex);
  queue_was_empty = log_queue_get_length(self->queue) == 0;
  if (queue_was_empty && !self->db_thread_suspended)
    {
      log_queue_set_parallel_push(self->queue, 1, afsql_dd_queue_notify, self, NULL);
    }
  g_mutex_unlock(self->db_thread_mutex);
  log_msg_add_ack(msg, path_options);
  log_queue_push_tail(self->queue, log_msg_ref(msg), path_options);
  log_dest_driver_queue_method(s, msg, path_options, user_data);
}

static void
afsql_dd_free(LogPipe *s)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;
  gint i;

  log_template_options_destroy(&self->template_options);
  if (self->pending_msg)
    log_msg_unref(self->pending_msg);
  if (self->queue)
    log_queue_unref(self->queue);
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
  if (self->null_value)
    g_free(self->null_value);
  string_list_free(self->columns);
  string_list_free(self->indexes);
  string_list_free(self->values);
  log_template_unref(self->table);
  g_hash_table_destroy(self->validated_tables);
  g_hash_table_destroy(self->dbd_options);
  g_hash_table_destroy(self->dbd_options_numeric);
  if(self->session_statements)
    string_list_free(self->session_statements);
  log_dest_driver_free(s);
}

LogDriver *
afsql_dd_new(void)
{
  AFSqlDestDriver *self = g_new0(AFSqlDestDriver, 1);

  log_dest_driver_init_instance(&self->super);
  self->super.super.super.init = afsql_dd_init;
  self->super.super.super.deinit = afsql_dd_deinit;
  self->super.super.super.queue = afsql_dd_queue;
  self->super.super.super.free_fn = afsql_dd_free;

  self->type = g_strdup("mysql");
  self->host = g_strdup("");
  self->port = g_strdup("");
  self->user = g_strdup("syslog-ng");
  self->password = g_strdup("");
  self->database = g_strdup("logs");
  self->encoding = g_strdup("UTF-8");

  self->table = log_template_new(configuration, NULL);
  log_template_compile(self->table, "messages", NULL);
  self->failed_message_counter = 0;

  self->flush_lines = -1;
  self->flush_timeout = -1;
  self->flush_lines_queued = -1;
  self->session_statements = NULL;
  self->num_retries = MAX_FAILED_ATTEMPTS;

  self->validated_tables = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  self->dbd_options = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  self->dbd_options_numeric = g_hash_table_new_full(g_str_hash, g_int_equal, g_free, NULL);

  log_template_options_defaults(&self->template_options);
  init_sequence_number(&self->seq_num);
  return &self->super.super;
}

gint
afsql_dd_lookup_flag(const gchar *flag)
{
  if (strcmp(flag, "explicit-commits") == 0 || strcmp(flag, "explicit_commits") == 0)
    return AFSQL_DDF_EXPLICIT_COMMITS;
  else if (strcmp(flag, "dont-create-tables") == 0 || strcmp(flag, "dont_create_tables") == 0)
    return AFSQL_DDF_DONT_CREATE_TABLES;
  else
    msg_warning("Unknown SQL flag",
                evt_tag_str("flag", flag),
                NULL);
  return 0;
}

#endif
