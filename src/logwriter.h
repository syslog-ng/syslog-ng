#ifndef LOG_WRITER_H_INCLUDED
#define LOG_WRITER_H_INCLUDED

#include "logpipe.h"
#include "fdwrite.h"
#include "templates.h"

/* flags */
#define LW_DETECT_EOF    0x0001
#define LW_FORMAT_FILE   0x0002
#define LW_FORMAT_PROTO  0x0004

/* writer options */
#define LWO_TMPL_ESCAPE     0x0001
#define LWO_TMPL_TIME_RECVD 0x0002

typedef struct _LogWriterOptions
{
  guint32 options;
  gint fifo_size;
  LogTemplate *template;
  LogTemplate *file_template;
  LogTemplate *proto_template;
  
  gint keep_timestamp;
  gint use_time_recvd;
  gint tz_convert;
  gint ts_format;
} LogWriterOptions;

typedef struct _LogWriter
{
  LogPipe super;
  GSource *source;
  GQueue *queue;
  guint32 flags;
  GString *partial;
  gint partial_pos;
  LogPipe *control;
  LogWriterOptions *options;
} LogWriter;

LogPipe *log_writer_new(guint32 flags, LogPipe *control, LogWriterOptions *options);
gboolean log_writer_reopen(LogPipe *s, FDWrite *fd);
void log_writer_options_defaults(LogWriterOptions *options);
void log_writer_options_init(LogWriterOptions *options, GlobalConfig *cfg, gboolean fixed_stamps);

#endif
