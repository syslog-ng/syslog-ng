#ifndef DBPARSER_H_INCLUDED
#define DBPARSER_H_INCLUDED

#include "logparser.h"
#include "logpatterns.h"

typedef struct _LogDBParser LogDBParser;

void log_db_parser_set_db_file(LogDBParser *self, const gchar *db_file);
LogParser *log_db_parser_new(void);
void log_db_parser_process_lookup(LogPatternDatabase *db, LogMessage *msg, GSList **dbg_list);

void log_db_parser_global_init(void);

#endif
