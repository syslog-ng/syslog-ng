#include "stateful-parser.h"
#include <string.h>


void
stateful_parser_set_inject_mode(StatefulParser *self, LogDBParserInjectMode inject_mode)
{
  self->inject_mode = inject_mode;
}

void
stateful_parser_emit_synthetic(StatefulParser *self, LogMessage *msg)
{
  if (self->inject_mode == LDBP_IM_PASSTHROUGH)
    {
      LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

      path_options.ack_needed = FALSE;
      log_pipe_forward_msg(&self->super.super, log_msg_ref(msg), &path_options);
    }
  else
    {
      msg_post_message(log_msg_ref(msg));
    }
}

void
stateful_parser_init_instance(StatefulParser *self, GlobalConfig *cfg)
{
  log_parser_init_instance(&self->super, cfg);
  self->inject_mode = LDBP_IM_PASSTHROUGH;
}

void
stateful_parser_free_method(LogPipe *s)
{
  log_parser_free_method(s);
}

int
stateful_parser_lookup_inject_mode(const gchar *inject_mode)
{
  if (strcasecmp(inject_mode, "internal") == 0)
    return LDBP_IM_INTERNAL;
  else if (strcasecmp(inject_mode, "pass-through") == 0 || strcasecmp(inject_mode, "pass_through") == 0)
    return LDBP_IM_PASSTHROUGH;
  return -1;
}
