/***************************************************************************
 *
 * COPYRIGHTHERE
 *
 * $Id: memtrace.h,v 1.3 2002/11/15 08:10:36 bazsi Exp $
 *
 ***************************************************************************/

#ifndef ZORP_MEMTRACE_H_INCLUDED
#define ZORP_MEMTRACE_H_INCLUDED

void z_mem_trace_init(gchar *memtrace_file);
void z_mem_trace_stats(void);
void z_mem_trace_dump();

#if ENABLE_MEM_TRACE

#include <stdlib.h>

void *z_malloc(size_t size, gpointer backtrace[]);
void z_free(void *ptr, gpointer backtrace[]);
void *z_realloc(void *ptr, size_t size, gpointer backtrace[]);
void *z_calloc(size_t nmemb, size_t size, gpointer backtrace[]);

#endif

#endif
