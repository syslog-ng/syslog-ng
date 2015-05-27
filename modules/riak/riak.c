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

  RiakBucketMode mode;

  riak_client_t *client;
} RiakDestDriver;

/*
 * Configuration
 */

void
riak_dd_set_host(LogDriver *d, char *host)
{
  RiakDestDriver *self = (RiakDestDriver *)d;

  free(self->host);
  self->host = strdup(host);
}

void
riak_dd_set_port(LogDriver *d, int port)
{
  RiakDestDriver *self = (RiakDestDriver *)d;
  self->port = port;
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

  sprintf(persist_name, "riak,%s,%u,%s,%s,%s,%s,%s", self->host, self->port, self->mode,
            self->bucket_type, self->bucket, self->key );
  return persist_name;
}

static char *
riak_dd_format_persist_name(LogThrDestDriver *d)
{
  RiakDestDriver *self = (RiakDestDriver *)d;
  static char persist_name[1024];

  sprintf(persist_name, "riak(%s,%u,%s,%s,%s,%s,%s)", self->host, self->port, self->mode,
            self->bucket_type, self->bucket, self->key );
  return persist_name;
}

static boolean
riak_dd_connect(RiakDestDriver *self, boolean reconnect)
{
  if (reconnect && (self->client != NULL))
    {
      riakCommand(self->client, "ping");

      if (!self->client->err)
        return TRUE;
      else
        self->client = riakConnect(self->host, self->port);
    }
  else
    self->client = riakConnect(self->host, self->port);

  if (self->client->err)
    {
      msg_error("RIAK server error, suspending",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("error", self->client->errstr),
                evt_tag_int("time_reopen", self->super.time_reopen),
                NULL);
      return FALSE;
    }
  else
    msg_debug("Connecting to RIAK succeeded",
              evt_tag_str("driver", self->super.super.super.id), NULL);

  return TRUE;
}

static void
riak_dd_disconnect(LogThrDestDriver *s)
{
  RiakDriver *self = (RiakDriver *)s;

  if (self->client)
    riakFree(self->client);
  self->client = NULL;
}

/*
 * Worker thread
 */


 
/*
 * Main thread
 */

static boolean
riak_dd_init(LogPipe *s)                           //Initializes the destination driver
{
  RiakDestDriver *self = (RiakDestDriver *)s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (!log_dest_driver_init_method(s))
    return FALSE;

  log_template_options_init(&self->template_options, cfg);

  msg_verbose("Initializing Riak destination",
              evt_tag_str("driver", self->super.super.super.id),
              evt_tag_str("host", self->host),
              evt_tag_int("port", self->port),
              NULL);

  return log_threaded_dest_driver_start(s);
}

static void
riak_dd_free(LogPipe *d)                   //frees up the structure allocated during config parsing
{
  RiakDestDriver *self = (RiakDestDriver *)d;

  log_template_options_destroy(&self->template_options);

  free(self->host);
  free(self->type);
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
  RiakDestDriver *self = calloc(RiakDestDriver, 1);

  log_threaded_dest_driver_init_instance(&self->super, cfg);
  self->super.super.super.super.init = riak_dd_init;
  self->super.super.super.super.free_fn = riak_dd_free;

  self->super.worker.thread_init = riak_worker_thread_init;
  self->super.worker.thread_deinit = riak_worker_thread_deinit;
  self->super.worker.disconnect = riak_dd_disconnect;
  self->super.worker.insert = riak_worker_insert;

  self->super.format.stats_instance = riak_dd_format_stats_instance;
  self->super.format.persist_name = riak_dd_format_persist_name;
  self->super.stats_source = SCS_RIAK;

  riak_dd_set_host((LogDriver *)self, "127.0.0.1");
  riak_dd_set_port((LogDriver *)self, 8087);

  log_template_options_defaults(&self->template_options);

  return (LogDriver *)self;
}

extern CfgParser riak_dd_parser;

static Plugin riak_plugin =
{
  .type = LL_CONTEXT_DESTINATION,
  .name = "riak",
  .parser = &riak_parser,
};

boolean
riak_module_init(GlobalConfig *cfg, CfgArgs *args)
{
  plugin_register(cfg, &riak_plugin, 1);

  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "riak",
  .version = SYSLOG_NG_VERSION,
  .description = "The riak module provides Riak destination support for syslog-ng.",
  .core_revision = SYSLOG_NG_SOURCE_REVISION,
  .plugins = &riak_plugin,
  .plugins_len = 1,
};
  

