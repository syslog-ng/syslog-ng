#include "riak.h"
#include "riak-parser.h"
#include "plugin.h"
#include "messages.h"
#include "misc.h"
#include "stats/stats-registry.h"
#include "logqueue.h"
#include "driver.h"
#include "plugin-types.h"
#include "logthrdestdrv.h"
#include <stdlib.h>
#include <riack/riack.h>

typedef enum
{
  RIAK_BUCKET_MODE_STORE,
  RIAK_BUCKET_MODE_SET
} RiakBucketMode;

typedef struct
{
  LogThrDestDriver super;
  LogTemplateOptions template_options;
  char *host;
  int port;
  LogTemplate *bucket;
  char *bucket_type;
  LogTemplate *key;
  LogTemplate *value;
  char *content_type;
  char *charset;
  int flush_lines;
  riack_setop_t *setop;
  int flush_index;
  RiakBucketMode mode;
  riack_client_t *client;
} RiakDestDriver;

/*
 * Configuration
 */

void
riak_dd_set_host(LogDriver *d, const char *host)
{
  RiakDestDriver *self = (RiakDestDriver *)d;
  free(self->host);
  self->host = strdup(host);
}

void
riak_dd_set_port(LogDriver *d, int port)
{
  RiakDestDriver *self = (RiakDestDriver *)d;
  self->port           = port;
}

void
riak_dd_set_bucket(LogDriver *d, LogTemplate *bucket)
{
  RiakDestDriver *self = (RiakDestDriver *)d;
  log_template_unref(self->bucket);
  self->bucket = log_template_ref(bucket);
}

void
riak_dd_set_mode(LogDriver *d, char *mode)
{
  RiakDestDriver *self = (RiakDestDriver *)d;
  if (strcmp(mode, "store") == 0)
    self->mode = RIAK_BUCKET_MODE_STORE;
  else if (strcmp(mode, "set") == 0)
    self->mode = RIAK_BUCKET_MODE_SET;
}

void
riak_dd_set_type(LogDriver *d, char *type)
{
  RiakDestDriver *self = (RiakDestDriver *)d;
  free(self->bucket_type);
  self->bucket_type = strdup(type);
}

void
riak_dd_set_key(LogDriver *d, LogTemplate *key)
{
  RiakDestDriver *self = (RiakDestDriver *)d;
  log_template_unref(self->key);
  self->key = log_template_ref(key);
}

void
riak_dd_set_value(LogDriver *d, LogTemplate *value)
{
  RiakDestDriver *self = (RiakDestDriver *)d;
  log_template_unref(self->value);
  self->value = log_template_ref(value);
}

void
riak_dd_set_charset(LogDriver *d, char *charset)
{
  RiakDestDriver *self = (RiakDestDriver *)d;
  free(self->charset);
  self->charset = strdup(charset);
}

void
riak_dd_set_content_type(LogDriver *d, char *content_type)
{
  RiakDestDriver *self = (RiakDestDriver *)d;
  free(self->content_type);
  self->content_type = strdup(content_type);
}

void
riak_dd_set_flush_lines(LogDriver *d, int flush_lines)
{
  RiakDestDriver *self = (RiakDestDriver *)d;
  self->flush_lines    = flush_lines;
}

LogTemplateOptions *
riak_dd_get_template_options(LogDriver *d)
{
  RiakDestDriver *self = (RiakDestDriver *)d;
  return &self->template_options;
}

/*
 * Utilities
 */
static char *
riak_dd_format_stats_instance(LogThrDestDriver *d)
{
  RiakDestDriver *self = (RiakDestDriver *)d;
  static char persist_name[1024];
  sprintf(persist_name, "riak,%s,%u,%u,%s,%s,%s", self->host, self->port, self->mode,
          self->bucket_type, self->charset, self->content_type);
  return persist_name;
}

static char *
riak_dd_format_persist_name(LogThrDestDriver *d)
{
  RiakDestDriver *self = (RiakDestDriver *)d;
  static char persist_name[1024];
  sprintf(persist_name, "riak(%s,%u,%u,%s,%s,%s)", self->host, self->port, self->mode,
          self->bucket_type, self->charset, self->content_type);
  return persist_name;
}

static gboolean
riak_dd_connect(RiakDestDriver *self, gboolean reconnect)
{
  int check;
  if (!self->client)
    self->client = riack_client_new( );
  if (reconnect && (self->client != NULL))
    {
      check =
          riack_client_connect(self->client, RIACK_CONNECT_OPTION_HOST, self->host,
                               RIACK_CONNECT_OPTION_PORT, self->port, RIACK_CONNECT_OPTION_NONE);
    }

  if (check == 0)
    return TRUE;
  else
    return FALSE;
}

static void
riak_dd_disconnect(LogThrDestDriver *s)
{
  RiakDestDriver *self = (RiakDestDriver *)s;
  if (self->client)
    {
      riack_client_disconnect(self->client);
      riack_client_free(self->client);
    }
  self->client = NULL;
}

/*
 * Worker thread
 */

static worker_insert_result_t
riak_worker_insert(LogThrDestDriver *s, LogMessage *msg)
{
  RiakDestDriver *self = (RiakDestDriver *)s;
  GString *key_res     = g_string_sized_new(1024);
  GString *bucket_res  = g_string_sized_new(1024);
  GString *value_res   = g_string_sized_new(1024);
  riack_put_req_t *putreq;
  riack_content_t *content;
  riack_message_t *message;
  riack_dt_update_req_t *dtupdatereq;
  riack_dt_op_t *dtop;
  riack_setop_t *setop;
  int scheck;

  if (!riak_dd_connect(self, TRUE))
    return WORKER_INSERT_RESULT_NOT_CONNECTED;

  if (self->client->conn == 0)
    return WORKER_INSERT_RESULT_ERROR;

  log_template_format(self->key, msg, &self->template_options, LTZ_SEND, self->super.seq_num, NULL,
                      key_res);

  log_template_format(self->value, msg, &self->template_options, LTZ_SEND, self->super.seq_num,
                      NULL, value_res);

  log_template_format(self->bucket, msg, &self->template_options, LTZ_SEND, self->super.seq_num,
                      NULL, bucket_res);

  if (self->mode == RIAK_BUCKET_MODE_SET)
    {
      if (self->flush_lines)
        {

          if (self->flush_index == 0)
            self->setop = riack_setop_new( );

          riack_setop_set(self->setop, RIACK_SETOP_FIELD_BULK_ADD, self->flush_index, value_res,
                          RIACK_SETOP_FIELD_NONE);
          self->flush_index += 1;

          if (self->flush_lines == self->flush_index)
            {
              dtupdatereq       = riack_req_dt_update_new( );
              dtop              = riack_dt_op_new( );
              self->flush_index = 0;
              riack_dt_op_set(dtop, RIACK_DT_OP_FIELD_SETOP, self->setop, RIACK_DT_OP_FIELD_NONE);

              riack_req_dt_update_set(
                  dtupdatereq, RIACK_REQ_DT_UPDATE_FIELD_BUCKET, bucket_res,
                  RIACK_REQ_DT_UPDATE_FIELD_BUCKET_TYPE, "sets", RIACK_REQ_DT_UPDATE_FIELD_KEY,
                  key_res, RIACK_REQ_DT_UPDATE_FIELD_DT_OP, dtop, RIACK_REQ_DT_UPDATE_FIELD_NONE);

              message = riack_dtupdatereq_serialize(dtupdatereq);

              if ((scheck = riack_client_send(self->client, message)) == 0)
                {
                  if ((scheck = riack_client_recv(self->client)) == 0)
                    {
                      msg_debug("RIAK bucket sent in set mode",
                                evt_tag_str("driver", self->super.super.super.id),
                                evt_tag_str("bucket", bucket_res->str),
                                evt_tag_str("bucket_type", self->bucket_type),
                                evt_tag_str("key", key_res->str),
                                evt_tag_str("value", value_res->str), NULL);

                      printf("Above data sent to riak in set mode successfully\n");
                      g_string_free(value_res, TRUE);
                      g_string_free(bucket_res, TRUE);
                      g_string_free(key_res, TRUE);
                      riack_req_dt_update_free(dtupdatereq);
                      return WORKER_INSERT_RESULT_SUCCESS;
                    }
                  else
                    printf("Error in receiving response from riak\n");
                }
              else
                printf("Error in sending message to riak\n");

              g_string_free(value_res, TRUE);
              g_string_free(bucket_res, TRUE);
              g_string_free(key_res, TRUE);
              riack_req_dt_update_free(dtupdatereq);
              return WORKER_INSERT_RESULT_ERROR;
            }
        }
      else if (!self->flush_lines)
        {
          dtupdatereq = riack_req_dt_update_new( );
          dtop        = riack_dt_op_new( );
          setop       = riack_setop_new( );

          riack_setop_set(setop, RIACK_SETOP_FIELD_ADD, value_res, RIACK_SETOP_FIELD_NONE);

          riack_dt_op_set(dtop, RIACK_DT_OP_FIELD_SETOP, setop, RIACK_DT_OP_FIELD_NONE);

          riack_req_dt_update_set(
              dtupdatereq, RIACK_REQ_DT_UPDATE_FIELD_BUCKET, bucket_res,
              RIACK_REQ_DT_UPDATE_FIELD_BUCKET_TYPE, "sets", RIACK_REQ_DT_UPDATE_FIELD_KEY, key_res,
              RIACK_REQ_DT_UPDATE_FIELD_DT_OP, dtop, RIACK_REQ_DT_UPDATE_FIELD_NONE);

          message = riack_dtupdatereq_serialize(dtupdatereq);

          if ((scheck = riack_client_send(self->client, message)) == 0)
            {
              if ((scheck = riack_client_recv(self->client)) == 0)
                {
                  msg_debug("RIAK bucket sent in set mode",
                            evt_tag_str("driver", self->super.super.super.id),
                            evt_tag_str("bucket", bucket_res->str),
                            evt_tag_str("bucket_type", self->bucket_type),
                            evt_tag_str("key", key_res->str), evt_tag_str("value", value_res->str),
                            NULL);

                  printf("Above data sent to riak in set mode successfully\n");
                  g_string_free(value_res, TRUE);
                  g_string_free(bucket_res, TRUE);
                  g_string_free(key_res, TRUE);
                  riack_req_dt_update_free(dtupdatereq);
                  return WORKER_INSERT_RESULT_SUCCESS;
                }
              else
                printf("Error in receiving response from riak\n");
            }
          else
            printf("Error in sending message to riak\n");

          g_string_free(value_res, TRUE);
          g_string_free(bucket_res, TRUE);
          g_string_free(key_res, TRUE);
          riack_req_dt_update_free(dtupdatereq);
          return WORKER_INSERT_RESULT_ERROR;
        }
    }
  else if (self->mode == RIAK_BUCKET_MODE_STORE)
    {
      content = riack_content_new( );
      riack_content_set(content, RIACK_CONTENT_FIELD_VALUE, value_res->str, -1,
                        RIACK_CONTENT_FIELD_CONTENT_TYPE, self->content_type, -1,
                        RIACK_CONTENT_FIELD_CONTENT_ENCODING, "none", -1,
                        RIACK_CONTENT_FIELD_CHARSET, self->charset, -1, RIACK_CONTENT_FIELD_NONE);
      putreq = riack_req_put_new( );
      riack_req_put_set(putreq, RIACK_REQ_PUT_FIELD_BUCKET, bucket_res->str,
                        RIACK_REQ_PUT_FIELD_BUCKET_TYPE, self->bucket_type, RIACK_REQ_PUT_FIELD_KEY,
                        key_res->str, RIACK_REQ_PUT_FIELD_CONTENT, content,
                        RIACK_REQ_PUT_FIELD_NONE);
      message = riack_putreq_serialize(putreq);

      if ((scheck = riack_client_send(self->client, message)) == 0)
        {
          if ((scheck = riack_client_recv(self->client)) == 0)
            {
              msg_debug("RIAK bucket sent in store mode",
                        evt_tag_str("driver", self->super.super.super.id),
                        evt_tag_str("bucket", bucket_res->str),
                        evt_tag_str("bucket_type", self->bucket_type),
                        evt_tag_str("key", key_res->str), evt_tag_str("value", value_res->str),
                        evt_tag_str("content_type", self->content_type),
                        evt_tag_str("charset", self->charset), NULL);
              printf("Above data sent to riak in store mode successfully\n");
              g_string_free(value_res, TRUE);
              g_string_free(bucket_res, TRUE);
              g_string_free(key_res, TRUE);
              riack_req_put_free(putreq);
              return WORKER_INSERT_RESULT_SUCCESS;
            }
          else
            printf("Error in receiving response from riak\n");
        }
      else
        printf("Error in sending message to riak\n");

      g_string_free(value_res, TRUE);
      g_string_free(bucket_res, TRUE);
      g_string_free(key_res, TRUE);
      riack_req_put_free(putreq);
      return WORKER_INSERT_RESULT_ERROR;
    }
  return WORKER_INSERT_RESULT_SUCCESS;
}

static void
riak_worker_thread_init(LogThrDestDriver *d)
{
  msg_error("Riak worker init error, LogTemplate values NULL, suspending",
            evt_tag_str("driver", self->super.super.super.id),
            // evt_tag_str("error", self->client->errstr),
            evt_tag_int("time_reopen", self->super.time_reopen), NULL);
}
if (self->flush_lines && self->mode == RIAK_BUCKET_MODE_STORE)
  {
    printf("riak cannot use flush lines in bucket store mode, change to set mode");
  }
//   riak_dd_connect(self, FALSE);
}

msg_debug("Worker thread started", evt_tag_str("driver", self->super.super.super.id), NULL);

if (self->bucket == NULL || self->key == NULL || self->value == NULL)
  {
    msg_error("Riak worker init error, LogTemplate values NULL, suspending",
              evt_tag_str("driver", self->super.super.super.id),
              evt_tag_int("time_reopen", self->super.time_reopen), NULL);
  }

if (self->flush_lines && self->mode == RIAK_BUCKET_MODE_STORE)
  printf("riak cannot use flush lines in bucket store mode, change to set mode");
}

static void
riak_worker_thread_deinit(LogThrDestDriver *d)
{
  RiakDestDriver *self = (RiakDestDriver *)d;

  log_template_unref(self->bucket);
  log_template_unref(self->key);
  log_template_unref(self->value);
  free(self->host);
  free(self->bucket_type);
  free(self->charset);
  free(self->content_type);
}

/*
 * Main thread
 */

static gboolean riak_dd_init(LogPipe *s) // Initializes the destination driver
{
  RiakDestDriver *self = (RiakDestDriver *)s;
  GlobalConfig *cfg    = log_pipe_get_config(s);

  if (!log_dest_driver_init_method(s))
    return FALSE;

  log_template_options_init(&self->template_options, cfg);

  msg_verbose("Initializing Riak destination", evt_tag_str("driver", self->super.super.super.id),
              evt_tag_str("host", self->host), evt_tag_int("port", self->port), NULL);

  return log_threaded_dest_driver_start(s);
}

static void riak_dd_free(LogPipe *d) // frees up the structure allocated during config parsing
{
  RiakDestDriver *self = (RiakDestDriver *)d;

  log_template_options_destroy(&self->template_options);

  free(self->host);
  free(self->bucket_type);
  free(self->charset);
  free(self->content_type);
  log_template_unref(self->key);
  log_template_unref(self->value);
  log_template_unref(self->bucket);

  log_threaded_dest_driver_free(d);
}

/*
 * Plugin glue.
 */

LogDriver *
riak_dd_new(GlobalConfig *cfg)
{
  RiakDestDriver *self = (RiakDestDriver *)calloc(1, sizeof(RiakDestDriver));

  log_threaded_dest_driver_init_instance(&self->super, cfg);
  self->super.super.super.super.init    = riak_dd_init;
  self->super.super.super.super.free_fn = riak_dd_free;

  self->super.worker.thread_init   = riak_worker_thread_init;
  self->super.worker.thread_deinit = riak_worker_thread_deinit;
  self->super.worker.disconnect    = riak_dd_disconnect;
  self->super.worker.insert        = riak_worker_insert;

  self->super.format.stats_instance = riak_dd_format_stats_instance;
  self->super.format.persist_name   = riak_dd_format_persist_name;
  // self->super.stats_source = SCS_RIAK;

  riak_dd_set_host((LogDriver *)self, "127.0.0.1");
  riak_dd_set_port((LogDriver *)self, 8087);

  log_template_options_defaults(&self->template_options);

  return (LogDriver *)self;
}

extern CfgParser riak_dd_parser;

static Plugin riak_plugin = {
    .type = LL_CONTEXT_DESTINATION, .name = "riak", .parser = &riak_parser,
};

gboolean
riak_module_init(GlobalConfig *cfg, CfgArgs *args)
{
  plugin_register(cfg, &riak_plugin, 1);

  return TRUE;
}

const ModuleInfo module_info = {
    .canonical_name          = "riak",
    .version                 = SYSLOG_NG_VERSION,
    .description             = "The riak module provides Riak destination support for syslog-ng.",
    .core_revision           = SYSLOG_NG_SOURCE_REVISION,
    .plugins                 = &riak_plugin,
    .plugins_len             = 1,
};
