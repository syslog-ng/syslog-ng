/*
 * Copyright (c) 2021 Balazs Scheidler <bazsi77@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "rewrite-set-matches.h"
#include "scanner/list-scanner/list-scanner.h"
#include "scratch-buffers.h"

/* LogRewriteSetMatches
 *
 * This class implements the "set" expression in a rewrite rule.
 */
typedef struct _LogRewriteSetMatches LogRewriteSetMatches;

struct _LogRewriteSetMatches
{
  LogRewrite super;
  LogTemplate *value_template;
  LogTemplateOptions template_options;
};

LogTemplateOptions *
log_rewrite_set_matches_get_template_options(LogRewrite *s)
{
  LogRewriteSetMatches *self = (LogRewriteSetMatches *) s;
  return &self->template_options;
}

static void
log_rewrite_set_matches_process(LogRewrite *s, LogMessage **pmsg, const LogPathOptions *path_options)
{
  LogRewriteSetMatches *self = (LogRewriteSetMatches *) s;
  GString *result;
  LogMessageValueType type;

  result = scratch_buffers_alloc();

  LogTemplateEvalOptions options = {&self->template_options, LTZ_SEND, 0, NULL, LM_VT_STRING};
  log_template_format_value_and_type(self->value_template, *pmsg, &options, result, &type);

  log_msg_make_writable(pmsg, path_options);

  ListScanner scanner;

  list_scanner_init(&scanner);
  list_scanner_input_string(&scanner, result->str, result->len);
  log_msg_clear_matches(*pmsg);
  for (gint i = 1; list_scanner_scan_next(&scanner) && i < LOGMSG_MAX_MATCHES; i++)
    {
      log_msg_set_match(*pmsg, i,
                        list_scanner_get_current_value(&scanner), list_scanner_get_current_value_len(&scanner));
    }
  list_scanner_deinit(&scanner);
}

static LogPipe *
log_rewrite_set_matches_clone(LogPipe *s)
{
  LogRewriteSetMatches *self = (LogRewriteSetMatches *) s;
  LogRewriteSetMatches *cloned;

  cloned = (LogRewriteSetMatches *) log_rewrite_set_matches_new(self->value_template, s->cfg);
  log_rewrite_clone_method(&cloned->super, &self->super);

  return &cloned->super.super;
}

static void
log_rewrite_set_matches_free(LogPipe *s)
{
  LogRewriteSetMatches *self = (LogRewriteSetMatches *) s;

  log_template_options_destroy(&self->template_options);
  log_template_unref(self->value_template);
  log_rewrite_free_method(s);
}

gboolean
log_rewrite_set_matches_init_method(LogPipe *s)
{
  LogRewriteSetMatches *self = (LogRewriteSetMatches *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);
  if (log_rewrite_init_method(s))
    {
      log_template_options_init(&self->template_options, cfg);
      return TRUE;
    }
  else
    return FALSE;
}

LogRewrite *
log_rewrite_set_matches_new(LogTemplate *new_value, GlobalConfig *cfg)
{
  LogRewriteSetMatches *self = g_new0(LogRewriteSetMatches, 1);

  log_rewrite_init_instance(&self->super, cfg);
  self->super.super.free_fn = log_rewrite_set_matches_free;
  self->super.super.clone = log_rewrite_set_matches_clone;
  self->super.super.init = log_rewrite_set_matches_init_method;
  self->super.process = log_rewrite_set_matches_process;
  self->value_template = log_template_ref(new_value);
  log_template_options_defaults(&self->template_options);

  return &self->super;
}
