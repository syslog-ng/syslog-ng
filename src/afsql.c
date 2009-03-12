/*
 * Copyright (c) 2002-2008 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
  GList *columns;
  GList *values;
  GList *indexes;
  LogTemplate *table;
  gint mem_fifo_size;
  gint64 disk_fifo_size;
  gboolean use_time_recvd;
  gint fields_len;
  AFSqlField *fields;
  gint time_reopen;
  
  TimeZoneInfo *time_zone_info;
  gchar *time_zone_string;
  gshort frac_digits; 

  guint32 *dropped_messages;
  guint32 *stored_messages;
  
  /* shared by the main/db thread */
  GStaticMutex queue_lock;
  LogQueue *queue;
  /* used exclusively by the db thread */
  gint32 seq_num;
  LogMessage *pending_msg;
  gboolean pending_msg_flow_control;
  dbi_conn dbi_ctx;
  GHashTable *validated_tables;
  time_t last_conn_attempt;
} AFSqlDestDriver;


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
afsql_dd_set_time_zone_string(LogDriver *s, const gchar *time_zone_string)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;
  
  self->time_zone_string = g_strdup(time_zone_string);
}


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
      if (!((token[i] == '_') || (i && token[i] >= '0' && token[i] <= '9') || (g_ascii_tolower(token[i]) >= 'a' && g_ascii_tolower(token[i]) <= 'z')))
        {
          if (sanitize)
            token[i] = '_';
          else
            return FALSE;
        }
    }
  return TRUE;
}

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

static GMutex *sql_drivers_lock;
static GList *sql_active_drivers;
static GList *sql_deactivate_drivers;
static gboolean db_thread_iter_finished;
static GCond *db_thread_iter_finished_cond, *db_thread_wakeup_cond;
static gboolean db_thread_terminate = FALSE;
static GThread *db_thread;
static gint db_thread_max_sleep_time = -1;

#define INSERTS_AT_A_TIME 30

/**
 * afsql_dd_insert_db:
 *
 * This function is running in the database thread
 *
 * Returns: TRUE to indicate that more data is available to be sent 
 **/
static gboolean
afsql_dd_insert_db(AFSqlDestDriver *self)
{  
  GString *table, *query_string, *value;
  LogMessage *msg;
  gboolean success;
  gint count = 0, i;
  time_t now;
  
  if (!self->dbi_ctx)
    {
      now = time(NULL);
      
      if (self->last_conn_attempt >= now - self->time_reopen)
        {
          return FALSE;
        }
      self->dbi_ctx = dbi_conn_new(self->type);
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
              dbi_conn_close(self->dbi_ctx);
              self->dbi_ctx = NULL;
              self->last_conn_attempt = time(NULL);
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
    }

  while (!db_thread_terminate && count < INSERTS_AT_A_TIME)
    {
      LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
      
      if (self->pending_msg)
        {
          msg = self->pending_msg;  
          path_options.flow_control = self->pending_msg_flow_control;
          self->pending_msg = NULL;
        }
      else
        {
          g_static_mutex_lock(&self->queue_lock);
          success = log_queue_pop_head(self->queue, &msg, &path_options, FALSE);
          g_static_mutex_unlock(&self->queue_lock);
          if (!success)
            return FALSE;
        }  
      
      msg_set_context(msg);
        
      table = g_string_sized_new(32);
      value = g_string_sized_new(256);
      query_string = g_string_sized_new(512);
      
      log_template_format(self->table, msg, 
                          (self->use_time_recvd ? LT_STAMP_RECVD : 0),
                          TS_FMT_BSD, 
                          NULL,
                          0, 0, table);

      if (!afsql_dd_validate_table(self, table->str))
        {
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
                              self->time_zone_info,
                              self->frac_digits,
                              self->seq_num, value);
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
          if (i != self->fields_len - 1)
            g_string_append(query_string, ", ");
        }
      g_string_append(query_string, ")");
      
      success = TRUE;
      if (!afsql_dd_run_query(self, query_string->str, FALSE, NULL))
        {
          /* error running INSERT on an already validated table, too bad. Try to reconnect. Maybe that helps. */
          success = FALSE;
          dbi_conn_close(self->dbi_ctx);
          self->dbi_ctx = NULL;
          g_hash_table_remove(self->validated_tables, table->str);
        }
      
    error:
      g_string_free(table, TRUE);
      g_string_free(value, TRUE);
      g_string_free(query_string, TRUE);

      msg_set_context(NULL);
      
      if (success)
        {
          log_msg_ack(msg, &path_options);
          log_msg_unref(msg);
          step_sequence_number(&self->seq_num);
        }
      else
        {
          self->pending_msg = msg;
          self->pending_msg_flow_control = path_options.flow_control;
          return FALSE;
        }
      count++;
    }
  return TRUE;
}



static gpointer
afsql_db_thread(gpointer arg)
{
  GList *drivers, *l;
  
  msg_verbose("Database thread started", NULL);
  while (!db_thread_terminate)
    {
      gboolean remaining_data;
      
      g_mutex_lock(sql_drivers_lock);
      drivers = g_list_copy(sql_active_drivers);
      g_mutex_unlock(sql_drivers_lock);
      
      remaining_data = FALSE;
      for (l = drivers; !db_thread_terminate && l; l = l->next)
        {
          AFSqlDestDriver *dd = (AFSqlDestDriver *) l->data;
          
          remaining_data |= afsql_dd_insert_db(dd);
        }
      g_list_free(drivers);
      
      g_mutex_lock(sql_drivers_lock);
      db_thread_iter_finished = TRUE;
      g_cond_signal(db_thread_iter_finished_cond);
      
      if (!remaining_data)
        {
          GTimeVal target;
          
          g_get_current_time(&target);
          g_time_val_add(&target, db_thread_max_sleep_time * 1000);
          g_cond_timed_wait(db_thread_wakeup_cond, sql_drivers_lock, &target);
        }
      for (l = sql_deactivate_drivers; l; l = l->next)
        {
          AFSqlDestDriver *dd = (AFSqlDestDriver *) l->data;

  	  sql_active_drivers = g_list_remove(sql_active_drivers, dd);
          dbi_conn_close(dd->dbi_ctx);
	  dd->dbi_ctx = NULL;
        }
      g_list_free(sql_deactivate_drivers);
      sql_deactivate_drivers = NULL;
      g_mutex_unlock(sql_drivers_lock);
    }
  msg_verbose("Database thread finished", NULL);
  return NULL;
}

static void
afsql_thread_sync()
{
  g_mutex_lock(sql_drivers_lock);
  db_thread_iter_finished = FALSE;
  g_cond_signal(db_thread_wakeup_cond);
  while (!db_thread_iter_finished)
    g_cond_wait(db_thread_iter_finished_cond, sql_drivers_lock);
  g_mutex_unlock(sql_drivers_lock);
}

static void
afsql_activate(AFSqlDestDriver *dd)
{
  g_mutex_lock(sql_drivers_lock);
  sql_active_drivers = g_list_prepend(sql_active_drivers, dd);
  g_mutex_unlock(sql_drivers_lock);
}

static void
afsql_deactivate(AFSqlDestDriver *dd)
{
  g_mutex_lock(sql_drivers_lock);
  sql_deactivate_drivers = g_list_prepend(sql_deactivate_drivers, dd);
  g_mutex_unlock(sql_drivers_lock);
  afsql_thread_sync();
}


static void
afsql_kill_db_thread(gint hook_type G_GNUC_UNUSED, gpointer user_data G_GNUC_UNUSED)
{
  db_thread_terminate = TRUE;
  g_cond_signal(db_thread_wakeup_cond);
  g_thread_join(db_thread);
}

/**
 *
 * This function is registered by SQL destinations to be called after
 * syslog-ng went into background. Since threads cannot be propagated
 * through forks, we postpone thread creation until post-daemonize time.
 **/
static void
afsql_init_db_thread(gint hook_type G_GNUC_UNUSED, gpointer user_data)
{
  AFSqlDestDriver *activate = (AFSqlDestDriver *) user_data;
  
  if (!db_thread)
    {
      dbi_initialize(NULL);
      sql_drivers_lock = g_mutex_new();
      db_thread_iter_finished_cond = g_cond_new();
      db_thread_wakeup_cond = g_cond_new();
      db_thread = create_worker_thread(afsql_db_thread, NULL, TRUE, NULL);
      register_application_hook(AH_SHUTDOWN, afsql_kill_db_thread, NULL);
    }
  
  /* activate the destination that had been created before the thread was created */
  afsql_activate(activate);
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
  
  if (db_thread_max_sleep_time == -1 || (cfg->time_reopen * 1000 / 2) < db_thread_max_sleep_time)
    db_thread_max_sleep_time = ((cfg->time_reopen * 1000) + 1) / 2;

  stats_register_counter(0, SCS_SQL | SCS_DESTINATION, self->super.id, afsql_dd_format_stats_instance(self), SC_TYPE_STORED, &self->stored_messages);
  stats_register_counter(0, SCS_SQL | SCS_DESTINATION, self->super.id, afsql_dd_format_stats_instance(self), SC_TYPE_DROPPED, &self->dropped_messages);

  if (!self->queue)
    {
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
  if (self->time_zone_string == NULL)
    self->time_zone_string = g_strdup(cfg->send_time_zone_string);
  if (self->time_zone_info == NULL)
    self->time_zone_info = time_zone_info_new(self->time_zone_string);

  if (!db_thread)
    {
      register_application_hook(AH_POST_DAEMONIZED, afsql_init_db_thread, self);
    }
  else
    {
      afsql_activate(self);
    }
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
  
  afsql_deactivate(self);
  /* self->dbi_ctx is freed in the DB thread, through the sql_dbi_context_free_queue queue */

  stats_unregister_counter(SCS_SQL | SCS_DESTINATION, self->super.id, afsql_dd_format_stats_instance(self), SC_TYPE_STORED, &self->stored_messages);
  stats_unregister_counter(SCS_SQL | SCS_DESTINATION, self->super.id, afsql_dd_format_stats_instance(self), SC_TYPE_DROPPED, &self->dropped_messages);
  
  return TRUE;
}

static void
afsql_dd_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  AFSqlDestDriver *self = (AFSqlDestDriver *) s;
  gboolean consumed;
  
  g_static_mutex_lock(&self->queue_lock);
  consumed = log_queue_push_tail(self->queue, msg, path_options);
  g_static_mutex_unlock(&self->queue_lock);
  g_cond_signal(db_thread_wakeup_cond);
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

  if (self->time_zone_info)
    {
      time_zone_info_free(self->time_zone_info);
    }
  g_free(self->time_zone_string);
  g_free(self->fields);
  g_free(self->type);
  g_free(self->host);
  g_free(self->port);
  g_free(self->user);
  g_free(self->password);
  g_free(self->database);
  g_free(self->encoding);
  string_list_free(self->columns);
  string_list_free(self->values);
  log_template_unref(self->table);
  g_hash_table_destroy(self->validated_tables);
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
afsql_dd_new()
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
  self->mem_fifo_size = 1000;
  self->disk_fifo_size = 0;
  self->time_zone_string = NULL;
  self->time_zone_info = NULL;
  self->frac_digits = -1;

  self->validated_tables = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  g_static_mutex_init(&self->queue_lock);
  
  init_sequence_number(&self->seq_num);
  return &self->super;  
}

#endif
