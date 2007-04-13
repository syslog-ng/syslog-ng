#ifndef LOGREADER_H_INCLUDED
#define LOGREADER_H_INCLUDED

#include "logpipe.h"
#include "fdread.h"

/* flags */
#define LR_LOCAL     0x0001
#define LR_INTERNAL  0x0002
#define LR_PKTTERM   0x0004 /* end-of-packet terminates log message (UDP sources) */
#define LR_FOLLOW    0x0008
#define LR_STRICT    0x0010

#define LR_COMPLETE_LINE 0x0100

/* options */
#define LRO_NOPARSE   0x0001
#define LRO_CHECK_HOSTNAME 0x0002

typedef struct _LogReaderOptions
{
  guint32 options;
  gint padding;
  gint init_window_size;
  gint fetch_limit;
  gint msg_size;
  gint follow_freq;
  gint mark_freq;
  
  /* source time zone if one is not specified */
  gboolean zone_offset_set;
  glong zone_offset;
} LogReaderOptions;

typedef struct _LogReader
{
  LogPipe super;
  gchar *buffer;
  FDRead *fd;
  GSource *source;
  gint ofs;
  gint window_size;
  guint32 flags;
  gint mark_target;
  LogPipe *control;
  GSockAddr *prev_addr;
  LogReaderOptions *options;
} LogReader;

LogPipe *log_reader_new(FDRead *fd, guint32 flags, LogPipe *control, LogReaderOptions *options);
void log_reader_options_defaults(LogReaderOptions *options);
void log_reader_options_init(LogReaderOptions *options, GlobalConfig *cfg);


#endif
