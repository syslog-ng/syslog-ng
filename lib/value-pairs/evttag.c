#include "value-pairs/evttag.h"

static gboolean
value_pairs_debug_append (const gchar *name, TypeHint type, const gchar *value, gpointer user_data)
{
  GString *text = (GString *) user_data;
  g_string_append_printf(text, "%s=%s ",name, value);
  return FALSE;
}

EVTTAG *
evt_tag_value_pairs(const char* key, ValuePairs *vp, LogMessage *msg, gint32 seq_num, gint time_zone_mode, LogTemplateOptions *template_options)
{
   GString *debug_text = g_string_new("");
   EVTTAG* result;

   value_pairs_foreach(vp, value_pairs_debug_append, msg, seq_num, time_zone_mode, template_options, debug_text);

   result = evt_tag_str(key, debug_text->str);

   g_string_free(debug_text, TRUE);
   return result;
}
