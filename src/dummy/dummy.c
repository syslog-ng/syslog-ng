
#include "dummy.h"
#include "dummy-parser.h"
#include "plugin.h"
#include "messages.h"

static gboolean
dummy_dd_init(LogPipe *s)
{
  return TRUE;
}

static gboolean
dummy_dd_deinit(LogPipe *s)
{
  return TRUE;
}

static void
dummy_dd_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  DummyDestDriver *self = (DummyDestDriver *) s;
  msg_notice("Dummy plugin received a message",
             evt_tag_str("msg", log_msg_get_value(msg, LM_V_MESSAGE, NULL)),
             evt_tag_int("opt", self->opt),
             NULL);

  log_msg_ack(msg, path_options);
  log_msg_unref(msg);
}

DummyDestDriver *
dummy_dd_new(void)
{
  DummyDestDriver *self = g_new0(DummyDestDriver, 1);

  log_drv_init_instance(&self->super);
  self->super.super.init = dummy_dd_init;
  self->super.super.deinit = dummy_dd_deinit;
  self->super.super.queue = dummy_dd_queue;

  return self;
}

extern CfgParser dummy_dd_parser;

static Plugin dummy_plugin =
{
  .type = LL_CONTEXT_DESTINATION,
  .name = "dummy",
  .parser = &dummy_parser,
};

gboolean
syslogng_module_init(void)
{
  plugin_register(&dummy_plugin, 1);
  return TRUE;
}
