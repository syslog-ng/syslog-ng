/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
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

#ifndef ENABLE_THREADS
#error "SQL module requires threads to be enabled"
#endif

#include "logqueue.h"
#include "templates.h"
#include "messages.h"
#include "misc.h"
#include "stats.h"
#include "apphook.h"
#include "timeutils.h"

#include <dbi/dbi.h>
#include <string.h>

#define AFSQL_DDF_EXPLICIT_COMMITS 0x0001

typedef struct _AFSqlField
{
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
  LogDriver super;
  /* read by the db thread */
  gchar *type;
  gchar *host;
  gchar *port;
  gchar *user;
  gchar *password;
  gchar *database;
  gchar *encoding;
  gboolean uses_default_columns:1, uses_default_values:1, uses_default_indexes:1;
  GList *columns;
  GList *values;
  GList *indexes;
  LogTemplate *table;
  gint mem_fifo_size;
  gint64 disk_fifo_size;
  gboolean use_time_recvd;
  gint fields_len;
  AFSqlField *fields;
  gchar *null_value;
  gint time_reopen;
  gint flush_lines;
  gint flush_timeout;
  gint flush_lines_queued;
  gint flags;
  GList *session_statements;

  TimeZoneInfo *local_time_zone_info;
  gchar *local_time_zone;
  TimeZoneInfo *send_time_zone_info;
  gchar *send_time_zone;
  gshort frac_digits;

  guint32 *dropped_messages;
  guint32 *stored_messages;

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
  gboolean pending_msg_flow_control;
  dbi_conn dbi_ctx;
  GHashTable *validated_tables;
  guint32 failed_message_counter;
} AFSqlDestDriver;

static gboolean dbi_initialized = FALSE;
static dbi_inst dbi_instance;

#define MAX_FAILED_ATTEMPTS 3

void
afsql_dd_set_type(LogDriver *s, const gchar *type)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;

  g_free(self->type);
  if (strcmp(type, "mssql") == 0)
    type = "freetds";
  self->type = g_strdup(type);
}

void
afsql_dd_set_host(LogDriver *s, const gchar *host)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;

  g_free(self->host);
  self->host = g_strdup(host);
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

  log_template_unref(self->table);
  self->table = log_template_new(NULL, table);
}

void
afsql_dd_set_columns(LogDriver *s, GList *columns)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;

  string_list_free(self->columns);
  self->columns = columns;
  self->uses_default_columns = FALSE;
}

void
afsql_dd_set_indexes(LogDriver *s, GList *indexes)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;

  string_list_free(self->indexes);
  self->indexes = indexes;
  self->uses_default_indexes = FALSE;
}

void
afsql_dd_set_values(LogDriver *s, GList *values)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;

  string_list_free(self->values);
  self->values = values;
  self->uses_default_values = FALSE;
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
afsql_dd_set_mem_fifo_size(LogDriver *s, gint mem_fifo_size)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;

  self->mem_fifo_size = mem_fifo_size;
}

void
afsql_dd_set_disk_fifo_size(LogDriver *s, gint64 disk_fifo_size)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;

  self->disk_fifo_size = disk_fifo_size;
}

void
afsql_dd_set_frac_digits(LogDriver *s, gint frac_digits)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;

  self->frac_digits = frac_digits;
}

void
afsql_dd_set_send_time_zone(LogDriver *s, const gchar *send_time_zone)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;

  self->send_time_zone = g_strdup(send_time_zone);
}

void
afsql_dd_set_local_time_zone(LogDriver *s, const gchar *local_time_zone)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;

  self->local_time_zone = g_strdup(local_time_zone);
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

  if (strcmp(self->type, "oracle") == 0)
    g_string_printf(query_string, "CREATE INDEX %s_%s_idx ON %s ('%s')",
                    table, column, table, column);
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
static gboolean
afsql_dd_validate_table(AFSqlDestDriver *self, gchar *table)
{
  GString *query_string;
  dbi_result db_res;
  gboolean success = FALSE;
  gint i;

  afsql_dd_check_sql_identifier(table, TRUE);

  if (g_hash_table_lookup(self->validated_tables, table))
    return TRUE;

  query_string = g_string_sized_new(32);
  g_string_printf(query_string, "SELECT * FROM %s WHERE 0=1", table);
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
              g_string_printf(query_string, "ALTER TABLE %s ADD %s %s", table, self->fields[i].name, self->fields[i].type);
              if (!afsql_dd_run_query(self, query_string->str, FALSE, NULL))
                {
                  msg_error("Error adding missing column, giving up",
                            evt_tag_str("table", table),
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
                      afsql_dd_create_index(self, table, self->fields[i].name);
                    }
                }
            }
        }
      dbi_result_free(db_res);
    }
  else
    {
      /* table does not exist, create it */

      g_string_printf(query_string, "CREATE TABLE %s (", table);
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
              afsql_dd_create_index(self, table, (gchar *) l->data);
            }
        }
      else
        {
          msg_error("Error creating table, giving up",
                    evt_tag_str("table", table),
                    NULL);
        }
    }
  if (success)
    {
      /* we have successfully created/altered the destination table, record this information */
      g_hash_table_insert(self->validated_tables, g_strdup(table), GUINT_TO_POINTER(TRUE));
    }
  g_string_free(query_string, TRUE);
  return success;
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
  gboolean success;

  success = afsql_dd_run_query(self, "BEGIN", FALSE, NULL);
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
  g_time_val_add(&self->db_thread_suspend_target, self->time_reopen * 1000);
}

static void
afsql_dd_disconnect(AFSqlDestDriver *self)
{
  dbi_conn_close(self->dbi_ctx);
  self->dbi_ctx = NULL;
  g_hash_table_remove_all(self->validated_tables);
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

  if (!self->dbi_ctx)
    {
      self->dbi_ctx = dbi_conn_new_r(self->type, dbi_instance);
      if (self->dbi_ctx)
        {
          dbi_conn_set_option(self->dbi_ctx, "host", self->host);
          dbi_conn_set_option(self->dbi_ctx, "port", self->port);
          dbi_conn_set_option(self->dbi_ctx, "username", self->user);
          dbi_conn_set_option(self->dbi_ctx, "password", self->password);
          dbi_conn_set_option(self->dbi_ctx, "dbname", self->database);
          dbi_conn_set_option(self->dbi_ctx, "encoding", self->encoding);

          /* database specific hacks */
          dbi_conn_set_option(self->dbi_ctx, "sqlite_dbdir", "");
          dbi_conn_set_option(self->dbi_ctx, "sqlite3_dbdir", "");

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
        }
      else
        {
          msg_error("No such DBI driver",
                    evt_tag_str("type", self->type),
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
    }

  /* connection established, try to insert a message */

  if (self->pending_msg)
    {
      msg = self->pending_msg;
      path_options.flow_control = self->pending_msg_flow_control;
      self->pending_msg = NULL;
    }
  else
    {
      g_mutex_lock(self->db_thread_mutex);
      success = log_queue_pop_head(self->queue, &msg, &path_options, (self->flags & AFSQL_DDF_EXPLICIT_COMMITS));
      g_mutex_unlock(self->db_thread_mutex);
      if (!success)
        return TRUE;
    }

  msg_set_context(msg);

  table = g_string_sized_new(32);
  value = g_string_sized_new(256);
  query_string = g_string_sized_new(512);

  log_template_format(self->table, msg,
                      (self->use_time_recvd ? LT_STAMP_RECVD : 0),
                      TS_FMT_BSD,
                      self->local_time_zone_info,
                      0, 0, table);

  if (!afsql_dd_validate_table(self, table->str))
    {
      /* If validate table is FALSE then close the connection and wait time_reopen time (next call) */
      msg_error("Error checking table, disconnecting from database, trying again shortly",
                evt_tag_int("time_reopen", self->time_reopen),
                NULL);
      success = FALSE;
      goto error;
    }

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
      log_template_format(self->fields[i].value, msg,
                          (self->use_time_recvd ? LT_STAMP_RECVD : 0),
                          TS_FMT_BSD,
                          self->send_time_zone_info,
                          self->frac_digits,
                          self->seq_num, value);
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
 error:
  g_string_free(table, TRUE);
  g_string_free(value, TRUE);
  g_string_free(query_string, TRUE);

  msg_set_context(NULL);

  if (success)
    {
      /* we only ACK if each INSERT is a separate transaction */
      if ((self->flags & AFSQL_DDF_EXPLICIT_COMMITS) == 0)
        log_msg_ack(msg, &path_options);
      log_msg_unref(msg);
      step_sequence_number(&self->seq_num);
      self->failed_message_counter = 0;
    }
  else
    {
      if (self->failed_message_counter < MAX_FAILED_ATTEMPTS - 1)
        {
          self->pending_msg = msg;
          self->pending_msg_flow_control = path_options.flow_control;
          self->failed_message_counter++;
        }
      else
        {
          msg_error("Multiple failures while inserting this record into the database, message dropped",
                    evt_tag_int("attempts", MAX_FAILED_ATTEMPTS),
                    NULL);
          stats_counter_inc(self->dropped_messages);
          log_msg_drop(msg, &path_options);
          self->failed_message_counter = 0;
        }
    }
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
              evt_tag_str("driver", self->super.id),
              NULL);
  while (!self->db_thread_terminate)
    {
      g_mutex_lock(self->db_thread_mutex);
      if (self->db_thread_suspended)
        {
          /* we got suspended, probably because of a connection error,
           * during this time we only get wakeups if we need to be
           * terminated. */
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
              if (!g_cond_timed_wait(self->db_thread_wakeup_cond, self->db_thread_mutex, &flush_target))
                {
                  /* timeout elapsed */
                  if (!afsql_dd_commit_txn(self, FALSE))
                    {
                      afsql_dd_disconnect(self);
                      afsql_dd_suspend(self);
                      continue;
                    }
                }
            }
          else
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
              evt_tag_str("driver", self->super.id),
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
  self->db_thread_terminate = TRUE;
  g_cond_signal(self->db_thread_wakeup_cond);
  g_thread_join(self->db_thread);
  g_mutex_free(self->db_thread_mutex);
  g_cond_free(self->db_thread_wakeup_cond);
}

static gchar *
afsql_dd_format_stats_instance(AFSqlDestDriver *self)
{
  static gchar persist_name[64];

  g_snprintf(persist_name, sizeof(persist_name),
             "%s,%s,%s,%s",
             self->type, self->host, self->port, self->database);
  return persist_name;
}

static inline gchar *
afsql_dd_format_persist_name(AFSqlDestDriver *self)
{
  static gchar persist_name[256];

  g_snprintf(persist_name, sizeof(persist_name),
             "afsql_dd(%s,%s,%s,%s)",
             self->type, self->host, self->port, self->database);
  return persist_name;
}


static gboolean
afsql_dd_init(LogPipe *s)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);
  gint len_cols, len_values;

  /* the maximum time to sleep on a condvar in the db_thread is the smallest
   * time_reopen value divided by 2. This ensures that we are not stuck with
   * a complete queue and a failed DB connection. (in which case the
   * db_thread_wakeup_cond is not signaled as no new messages enter the
   * queue).
   */

  if (self->uses_default_columns || self->uses_default_indexes || self->uses_default_values)
    {
      msg_warning("WARNING: You are using the default values for columns(), indexes() or values(), "
                  "please specify these explicitly as the default will be dropped in the future", NULL);
    }

  stats_register_counter(0, SCS_SQL | SCS_DESTINATION, self->super.id, afsql_dd_format_stats_instance(self), SC_TYPE_STORED, &self->stored_messages);
  stats_register_counter(0, SCS_SQL | SCS_DESTINATION, self->super.id, afsql_dd_format_stats_instance(self), SC_TYPE_DROPPED, &self->dropped_messages);

  if (!self->queue)
    {
      self->queue = cfg_persist_config_fetch(cfg, afsql_dd_format_persist_name(self));
      if (!self->queue)
        self->queue = log_queue_new(self->mem_fifo_size);
    }

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
          self->fields[i].value = log_template_new(NULL, (gchar *) value->data);
        }
    }

  self->use_time_recvd = cfg->use_time_recvd;
  self->time_reopen = cfg->time_reopen;

  if (self->frac_digits == -1)
    self->frac_digits = cfg->frac_digits;
  if (self->send_time_zone == NULL)
    self->send_time_zone = g_strdup(cfg->send_time_zone);

  if (self->send_time_zone_info)
    time_zone_info_free(self->send_time_zone_info);
  self->send_time_zone_info = time_zone_info_new(self->send_time_zone);

  if (self->local_time_zone_info)
    time_zone_info_free(self->local_time_zone_info);
  self->local_time_zone_info = time_zone_info_new(self->local_time_zone);

  if (self->flush_lines == -1)
    self->flush_lines = cfg->flush_lines;
  if (self->flush_timeout == -1)
    self->flush_timeout = cfg->flush_timeout;

  if ((self->flags & AFSQL_DDF_EXPLICIT_COMMITS) && (self->flush_lines > 0 || self->flush_timeout > 0))
    self->flush_lines_queued = 0;

  if (!dbi_initialized)
    {
      dbi_initialize_r(NULL, &dbi_instance);
      dbi_initialized = TRUE;
    }

  afsql_dd_start_thread(self);
  return TRUE;

 error:

  stats_unregister_counter(SCS_SQL | SCS_DESTINATION, self->super.id, afsql_dd_format_stats_instance(self), SC_TYPE_STORED, &self->stored_messages);
  stats_unregister_counter(SCS_SQL | SCS_DESTINATION, self->super.id, afsql_dd_format_stats_instance(self), SC_TYPE_DROPPED, &self->dropped_messages);

  return FALSE;
}

static gboolean
afsql_dd_deinit(LogPipe *s)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  afsql_dd_stop_thread(self);
  cfg_persist_config_add(cfg, afsql_dd_format_persist_name(self), self->queue, (GDestroyNotify) log_queue_free, FALSE);
  self->queue = NULL;

  stats_unregister_counter(SCS_SQL | SCS_DESTINATION, self->super.id, afsql_dd_format_stats_instance(self), SC_TYPE_STORED, &self->stored_messages);
  stats_unregister_counter(SCS_SQL | SCS_DESTINATION, self->super.id, afsql_dd_format_stats_instance(self), SC_TYPE_DROPPED, &self->dropped_messages);

  return TRUE;
}

static void
afsql_dd_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;
  gboolean consumed, queue_was_empty;

  g_mutex_lock(self->db_thread_mutex);
  queue_was_empty = log_queue_get_length(self->queue) == 0;
  consumed = log_queue_push_tail(self->queue, msg, path_options);

  if (consumed && queue_was_empty && !self->db_thread_suspended)
    g_cond_signal(self->db_thread_wakeup_cond);
  g_mutex_unlock(self->db_thread_mutex);

  if (!consumed)
    {
      /* drop incoming message, we must ack here, otherwise the sender might
       * block forever, however this should not happen unless the sum of
       * window_sizes of sources feeding this writer exceeds log_fifo_size
       * or if flow control is not turned on.
       */

      /* we don't send a message here since the system is draining anyway */

      stats_counter_inc(self->dropped_messages);
      msg_debug("Destination queue full, dropping message",
                evt_tag_int("queue_len", log_queue_get_length(self->queue)),
                evt_tag_int("mem_fifo_size", self->mem_fifo_size),
                evt_tag_int("disk_fifo_size", self->disk_fifo_size),
                NULL);
      log_msg_drop(msg, path_options);
    }
}

static void
afsql_dd_free(LogPipe *s)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;
  gint i;

  if (self->pending_msg)
    log_msg_unref(self->pending_msg);
  if (self->queue)
    log_queue_free(self->queue);
  for (i = 0; i < self->fields_len; i++)
    {
      g_free(self->fields[i].name);
      g_free(self->fields[i].type);
      log_template_unref(self->fields[i].value);
    }

  if (self->send_time_zone_info)
    time_zone_info_free(self->send_time_zone_info);
  if (self->local_time_zone_info)
    time_zone_info_free(self->local_time_zone_info);
  g_free(self->send_time_zone);
  g_free(self->local_time_zone);
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
  string_list_free(self->values);
  log_template_unref(self->table);
  g_hash_table_destroy(self->validated_tables);
  if(self->session_statements)
    string_list_free(self->session_statements);
  log_drv_free(s);
}

const gchar *default_columns[] =
{
  "date datetime",
  "facility int",
  "level int",
  "host varchar(255)",
  "program varchar(64)",
  "pid int",
  "message text",
  NULL
};

const gchar *default_values[] =
{
  "${R_YEAR}-${R_MONTH}-${R_DAY} ${R_HOUR}:${R_MIN}:${R_SEC}",
  "$FACILITY_NUM",
  "$LEVEL_NUM",
  "$HOST",
  "$PROGRAM",
  "$PID",
  "$MSGONLY",
  NULL
};

const gchar *default_indexes[] =
{
  "date",
  "facility",
  "host",
  "program",
  NULL
};

LogDriver *
afsql_dd_new(void)
{
  AFSqlDestDriver *self = g_new0(AFSqlDestDriver, 1);

  log_drv_init_instance(&self->super);
  self->super.super.init = afsql_dd_init;
  self->super.super.deinit = afsql_dd_deinit;
  self->super.super.queue = afsql_dd_queue;
  self->super.super.free_fn = afsql_dd_free;

  self->type = g_strdup("mysql");
  self->host = g_strdup("");
  self->port = g_strdup("");
  self->user = g_strdup("syslog-ng");
  self->password = g_strdup("");
  self->database = g_strdup("logs");
  self->encoding = g_strdup("UTF-8");

  self->table = log_template_new(NULL, "messages");
  self->columns = string_array_to_list(default_columns);
  self->values = string_array_to_list(default_values);
  self->indexes = string_array_to_list(default_indexes);
  self->uses_default_columns = TRUE;
  self->uses_default_values = TRUE;
  self->uses_default_indexes = TRUE;
  self->mem_fifo_size = 1000;
  self->disk_fifo_size = 0;
  self->send_time_zone = NULL;
  self->send_time_zone_info = NULL;
  self->local_time_zone_info = NULL;
  self->frac_digits = -1;
  self->failed_message_counter = 0;

  self->flush_lines = -1;
  self->flush_timeout = -1;
  self->flush_lines_queued = -1;
  self->session_statements = NULL;

  self->validated_tables = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

  init_sequence_number(&self->seq_num);
  return &self->super;
}

gint
afsql_dd_lookup_flag(const gchar *flag)
{
  if (strcmp(flag, "explicit-commits") == 0 || strcmp(flag, "explicit_commits") == 0)
    return AFSQL_DDF_EXPLICIT_COMMITS;
  else
    msg_warning("Unknown SQL flag",
                evt_tag_str("flag", flag),
                NULL);
  return 0;
}

#endif
