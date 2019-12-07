#ifndef HTTP_TEST_SLOTS_H_INCLUDED
#define HTTP_TEST_SLOTS_H_INCLUDED

#include "driver.h"

typedef struct _HttpTestSlotsPlugin HttpTestSlotsPlugin;

HttpTestSlotsPlugin *http_test_slots_plugin_new(void);

void http_test_slots_plugin_set_header(HttpTestSlotsPlugin *self, const gchar *header);

#endif
