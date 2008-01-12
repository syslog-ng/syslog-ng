/*
 * COPYRIGHTHERE
 */
  
#ifndef AFSQL_H_INCLUDED
#define AFSQL_H_INCLUDED

#include "driver.h"

#if ENABLE_SQL /* BEGIN MARK: sql */

void afsql_dd_set_type(LogDriver *s, const gchar *type);
void afsql_dd_set_host(LogDriver *s, const gchar *host);
void afsql_dd_set_port(LogDriver *s, const gchar *host);
void afsql_dd_set_user(LogDriver *s, const gchar *user);
void afsql_dd_set_password(LogDriver *s, const gchar *password);
void afsql_dd_set_database(LogDriver *s, const gchar *database);
void afsql_dd_set_table(LogDriver *s, const gchar *table);
void afsql_dd_set_columns(LogDriver *s, GList *columns);
void afsql_dd_set_values(LogDriver *s, GList *values);
void afsql_dd_set_indexes(LogDriver *s, GList *indexes);
void afsql_dd_set_mem_fifo_size(LogDriver *s, gint mem_fifo_size);
void afsql_dd_set_disk_fifo_size(LogDriver *s, gint64 disk_fifo_size);

LogDriver *afsql_dd_new();

#else /* ELSE MARK */

#define afsql_dd_set_type(s, t)
#define afsql_dd_set_host(s, h)
#define afsql_dd_set_port(s, p)
#define afsql_dd_set_user(s, u)
#define afsql_dd_set_password(s, p)
#define afsql_dd_set_database(s, d)
#define afsql_dd_set_table(s, t)
#define afsql_dd_set_columns(s, c)
#define afsql_dd_set_values(s, v)
#define afsql_dd_set_mem_fifo_size(s, f)
#define afsql_dd_set_disk_fifo_size(s, f)

#define afsql_dd_new() 0

#endif /* END MARK */

#endif

