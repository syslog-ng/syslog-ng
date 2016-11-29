#include "stopwatch.h"
#include <stdio.h>
#include <sys/time.h>

struct timeval start_time_val;

void
start_stopwatch(void)
{
  gettimeofday(&start_time_val, NULL);
}

void
stop_stopwatch_and_display_result(gint iterations, gchar *message_template, ...)
{
  va_list args;
  guint64 diff;
  struct timeval end_time_val;
  gettimeofday(&end_time_val, NULL);

  va_start(args, message_template);
  vprintf(message_template, args);
  va_end(args);

  diff = (end_time_val.tv_sec - start_time_val.tv_sec) * 1000000 + end_time_val.tv_usec - start_time_val.tv_usec;
  printf("; %.2f iterations/sec", iterations * 1e6 / diff);
  printf(", runtime=%lu.%06lus\n", diff / 1000000, diff % 1000000);
}
