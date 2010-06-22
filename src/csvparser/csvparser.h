#ifndef CSVPARSER_H_INCLUDED
#define CSVPARSER_H_INCLUDED

#include "logparser.h"

#define LOG_CSV_PARSER_ESCAPE_NONE        0x0001
#define LOG_CSV_PARSER_ESCAPE_BACKSLASH   0x0002
#define LOG_CSV_PARSER_ESCAPE_DOUBLE_CHAR 0x0004
#define LOG_CSV_PARSER_STRIP_WHITESPACE   0x0008
#define LOG_CSV_PARSER_GREEDY             0x0010
#define LOG_CSV_PARSER_DROP_INVALID       0x0020


void log_csv_parser_set_flags(LogColumnParser *s, guint32 flags);
void log_csv_parser_set_delimiters(LogColumnParser *s, const gchar *delimiters);
void log_csv_parser_set_quotes(LogColumnParser *s, const gchar *quotes);
void log_csv_parser_set_quote_pairs(LogColumnParser *s, const gchar *quote_pairs);
void log_csv_parser_set_null_value(LogColumnParser *s, const gchar *null_value);
LogColumnParser *log_csv_parser_new(void);
gint log_csv_parser_lookup_flag(const gchar *flag);

#endif
