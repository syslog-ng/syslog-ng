#include "debugger/debugger.h"
#include "logpipe.h"

static Debugger *current_debugger;

static gboolean
_pipe_hook(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  return debugger_stop_at_breakpoint(current_debugger, s, msg);
}

void
debugger_start(GlobalConfig *cfg)
{
  /* we don't support threaded mode (yet), force it to non-threaded */
  cfg->threaded = FALSE;
  current_debugger = debugger_new(cfg);
  pipe_single_step_hook = _pipe_hook;
  debugger_start_console(current_debugger);
}
