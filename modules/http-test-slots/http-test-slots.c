#include "http-test-slots.h"
#include "http/http-signals.h"

#define HTTP_TEST_SLOTS_PLUGIN "http-test-slots"

struct _HttpTestSlotsPlugin
{
  LogDriverPlugin super;
  gchar *header;
};

void
http_test_slots_plugin_set_header(HttpTestSlotsPlugin *self, const gchar *header)
{
  g_free(self->header);
  self->header = g_strdup(header);
}

static void
_slot_append_test_headers(HttpTestSlotsPlugin *self, HttpHeaderRequestSignalData *data)
{
  list_append(data->request_headers, self->header);
}

static gboolean
_attach(LogDriverPlugin *s, LogDriver *driver)
{
  SignalSlotConnector *ssc = driver->super.signal_slot_connector;

  msg_debug("HttpTestSlotsPlugin::attach()",
            evt_tag_printf("SignalSlotConnector", "%p", ssc));

  /* DISCONNECT: the whole SignalSlotConnector is destroyed when the owner LogDriver is freed up, so DISCONNECT is not required */
  CONNECT(ssc, signal_http_header_request, _slot_append_test_headers, s);

  return TRUE;
}

static void
_free(LogDriverPlugin *s)
{
  msg_debug("HttpTestSlotsPlugin::free");
  HttpTestSlotsPlugin *self = (HttpTestSlotsPlugin *)s;
  g_free(self->header);
  log_driver_plugin_free_method(s);
}

HttpTestSlotsPlugin *
http_test_slots_plugin_new(void)
{
  HttpTestSlotsPlugin *self = g_new0(HttpTestSlotsPlugin, 1);

  log_driver_plugin_init_instance(&self->super, HTTP_TEST_SLOTS_PLUGIN);

  self->super.attach = _attach;
  self->super.free_fn = _free;

  return self;
}
