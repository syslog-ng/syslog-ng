#ifndef STR_FORMAT_H_INCLUDED
#define STR_FORMAT_H_INCLUDED

#include "syslog-ng.h"

gint
format_uint32_padded(GString *result, gint field_len, gchar pad_char, gint base, guint32 value);

gint
format_uint64_padded(GString *result, gint field_len, gchar pad_char, gint base, guint64 value);

gboolean
scan_iso_timestamp(const gchar **buf, gint *left, struct tm *tm);
gboolean
scan_pix_timestamp(const gchar **buf, gint *left, struct tm *tm);
gboolean
scan_linksys_timestamp(const gchar **buf, gint *left, struct tm *tm);
gboolean
scan_bsd_timestamp(const gchar **buf, gint *left, struct tm *tm);

#endif
