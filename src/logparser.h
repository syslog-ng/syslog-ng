#ifndef LOGPARSER_H_INCLUDED
#define LOGPARSER_H_INCLUDED

#include "logmsg.h"
#include "messages.h"
#include "logprocess.h"
#include "templates.h"

typedef struct _LogParser LogParser;
typedef struct _LogColumnParser LogColumnParser;
typedef struct _LogDBParser LogDBParser;

struct _LogParser
{
  struct _LogTemplate *template;
  void (*process)(LogParser *s, LogMessage *msg, const gchar *input);
  void (*free_fn)(LogParser *s);
};

void log_parser_set_template(LogParser *self, LogTemplate *template);
void log_parser_process(LogParser *self, LogMessage *msg);

static inline void
log_parser_free(LogParser *self)
{
  self->free_fn(self);
  log_template_unref(self->template);
  g_free(self);
}

struct _LogColumnParser
{
  LogParser super;
  GList *columns;
};


void log_column_parser_set_columns(LogColumnParser *s, GList *fields);

#define LOG_CSV_PARSER_ESCAPE_NONE        0x0001
#define LOG_CSV_PARSER_ESCAPE_BACKSLASH   0x0002
#define LOG_CSV_PARSER_ESCAPE_DOUBLE_CHAR 0x0004
#define LOG_CSV_PARSER_STRIP_WHITESPACE   0x0008


void log_csv_parser_set_flags(LogColumnParser *s, guint32 flags);
void log_csv_parser_set_delimiters(LogColumnParser *s, const gchar *delimiters);
void log_csv_parser_set_quotes(LogColumnParser *s, const gchar *quotes);
void log_csv_parser_set_quote_pairs(LogColumnParser *s, const gchar *quote_pairs);
void log_csv_parser_set_null_value(LogColumnParser *s, const gchar *null_value);
LogColumnParser *log_csv_parser_new(void);
gint log_csv_parser_lookup_flag(const gchar *flag);

void log_db_parser_set_db_file(LogDBParser *self, const gchar *db_file);
LogParser *log_db_parser_new(void);

LogProcessRule *log_parser_rule_new(const gchar *name, LogParser *parser);


#endif

