#include "alarms.h"
#include "messages.h"

#include <unistd.h>
#include <signal.h>
#include <string.h>

static gboolean sig_alarm_received = FALSE;
static gboolean alarm_pending = FALSE;

static void
sig_alarm_handler(int signo)
{
  sig_alarm_received = TRUE;
}

void
alarm_set(int timeout)
{
  if (G_UNLIKELY(alarm_pending))
    {
      msg_error("Internal error, alarm_set() called while an alarm is still active", NULL);
      return;
    }
  alarm(timeout);
  alarm_pending = TRUE;
}

void
alarm_cancel()
{
  alarm(0);
  sig_alarm_received = FALSE;
  alarm_pending = FALSE;
}

gboolean
alarm_has_fired()
{
  gboolean res = sig_alarm_received;
  return res;
}

void
alarm_init()
{
  struct sigaction sa;
  
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = sig_alarm_handler;
  sigaction(SIGALRM, &sa, NULL);
}
