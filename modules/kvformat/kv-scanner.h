#ifndef KV_SCANNER_H_INCLUDED
#define KV_SCANNER_H_INCLUDED

#include "syslog-ng.h"

typedef struct _KVScannerState
{
  const gchar *input;
  gsize input_pos;
  gsize input_len;
  GString *key;
  GString *value;
  gchar quote_char;
  gint quote_state;
  gint next_quote_state;
} KVScannerState;

void kv_scanner_init(KVScannerState *self);
void kv_scanner_destroy(KVScannerState *self);
void kv_scanner_input(KVScannerState *self, const gchar *input);
gboolean kv_scanner_scan_next(KVScannerState *self);
const gchar *kv_scanner_get_current_key(KVScannerState *self);
const gchar *kv_scanner_get_current_value(KVScannerState *self);

#endif
