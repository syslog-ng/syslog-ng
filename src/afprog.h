#ifndef AFPROG_H_INCLUDED
#define AFPROG_H_INCLUDED

#include "driver.h"
#include "logwriter.h"

typedef struct _AFProgramDestDriver
{
  LogDriver super;
  GString *cmdline;
  LogPipe *writer;
  pid_t pid;
  LogWriterOptions writer_options;
} AFProgramDestDriver;


LogDriver *afprogram_dd_new(gchar *cmdline);

#endif
