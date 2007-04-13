#ifndef TEMPLATES_H_INCLUDED
#define TEMPLATES_H_INCLUDED

#include "syslog-ng.h"
#include "logmsg.h"

#define LT_ESCAPE 0x0001
#define LT_TZ_SET 0x0002

typedef struct _LogTemplate
{
  gint ref_cnt;
  GString *name;
  GString *template;
  GList *compiled_template;
  guint flags;
  gboolean def_inline;
  glong zone_offset;
} LogTemplate;

void log_template_set_escape(LogTemplate *self, gboolean enable);
void log_template_format(LogTemplate *self, LogMessage *lm, guint macro_flags, GString *result);

LogTemplate *log_template_new(gchar *name, gchar *template);
LogTemplate *log_template_ref(LogTemplate *s);
void log_template_unref(LogTemplate *s);


#endif
