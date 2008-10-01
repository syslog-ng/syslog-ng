#ifndef LOGREWRITE_H_INCLUDED
#define LOGREWRITE_H_INCLUDED

#include "logmsg.h"
#include "messages.h"
#include "logprocess.h"
#include "templates.h"
#include "logmatcher.h"

typedef struct _LogRewrite LogRewrite;

struct _LogRewrite
{
  const gchar *value_name;
  void (*process)(LogRewrite *s, LogMessage *msg);
  void (*free_fn)(LogRewrite *s);
};


LogRewrite *log_rewrite_subst_new(const gchar *replacement);
LogRewrite *log_rewrite_set_new(const gchar *new_value);
void log_rewrite_free(LogRewrite *self);
gboolean log_rewrite_set_regexp(LogRewrite *s, const gchar *regexp);
void log_rewrite_set_matcher(LogRewrite *s, LogMatcher *matcher);
void log_rewrite_set_flags(LogRewrite *s, gint flags);

LogProcessRule *log_rewrite_rule_new(const gchar *name, GList *rewrite_list);


#endif

