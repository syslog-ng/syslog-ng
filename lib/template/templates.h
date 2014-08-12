/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2013 Balázs Scheidler
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

#ifndef TEMPLATES_H_INCLUDED
#define TEMPLATES_H_INCLUDED

#include "syslog-ng.h"
#include "timeutils.h"
#include "type-hinting.h"
#include "plugin.h"

#define LTZ_LOCAL 0
#define LTZ_SEND  1
#define LTZ_MAX   2

#define LOG_TEMPLATE_ERROR log_template_error_quark()

GQuark log_template_error_quark(void);

enum LogTemplateError
{
  LOG_TEMPLATE_ERROR_FAILED,
  LOG_TEMPLATE_ERROR_COMPILE,
};

typedef enum
{
  ON_ERROR_DROP_MESSAGE        = 0x01,
  ON_ERROR_DROP_PROPERTY       = 0x02,
  ON_ERROR_FALLBACK_TO_STRING  = 0x04, /* Valid for type hinting
                                          only! */
  ON_ERROR_SILENT              = 0x08
} LogTemplateOnError;

/* structure that represents an expandable syslog-ng template */
typedef struct _LogTemplate
{
  gint ref_cnt;
  gchar *name;
  gchar *template;
  GList *compiled_template;
  gboolean escape;
  gboolean def_inline;
  GlobalConfig *cfg;
  GStaticMutex arg_lock;
  GPtrArray *arg_bufs;
  TypeHint type_hint;
} LogTemplate;

/* template expansion options that can be influenced by the user and
 * is static throughout the runtime for a given configuration. There
 * are call-site specific options too, those are specified as
 * arguments to log_template_format() */
typedef struct _LogTemplateOptions
{
  gboolean initialized;
  /* timestamp format as specified by ts_format() */
  gint ts_format;
  /* number of digits in the fraction of a second part, specified using frac_digits() */
  gint frac_digits;

  /* timezone for LTZ_LOCAL/LTZ_SEND settings */
  gchar *time_zone[LTZ_MAX];
  TimeZoneInfo *time_zone_info[LTZ_MAX];

  /* Template error handling settings */
  gint on_error;
} LogTemplateOptions;

/* macros (not NV pairs!) that syslog-ng knows about. This was the
 * earliest mechanism for inserting message-specific information into
 * texts. It is now superseeded by name-value pairs where the value is
 * text, but remains to be used for time and other metadata.
 */
typedef struct _LogMacroDef
{
  char *name;
  int id;
} LogMacroDef;

extern LogMacroDef macros[];

/* This structure contains the arguments for template-function
 * expansion. It is defined in a struct because otherwise a large
 * number of function arguments, that are passed around, possibly
 * several times. */
typedef struct _LogTemplateInvokeArgs
{
  /* scratch buffers, stores GString *, elements are managed by the
   * function, storage/free is performed by the core. Can be used to
   * avoid allocating GString buffers in the fast-path. */

  GPtrArray *bufs;

  /* context in case of correllation */
  LogMessage **messages;
  gint num_messages;

  /* options for recursive template evaluation, inherited from the parent */
  const LogTemplateOptions *opts;
  gint tz;
  gint seq_num;
  const gchar *context_id;
} LogTemplateInvokeArgs;

/* function pointers for template functions */
typedef struct _LogTemplateFunction LogTemplateFunction;
struct _LogTemplateFunction
{
  /* size of the state that carries information from parse-time to
   * runtime. Can be used to store the results of expensive
   * operations that don't need to be performed for all invocations */
  gint size_of_state;

  /* called when parsing the arguments to be compiled into an internal
   * representation if necessary.  Returns the compiled state in state */
  gboolean (*prepare)(LogTemplateFunction *self, gpointer state, LogTemplate *parent, gint argc, gchar *argv[], GError **error);

  /* evaluate arguments, storing argument buffers in arg_bufs in case it
   * makes sense to reuse those buffers */
  void (*eval)(LogTemplateFunction *self, gpointer state, const LogTemplateInvokeArgs *args);

  /* call the function */
  void (*call)(LogTemplateFunction *self, gpointer state, const LogTemplateInvokeArgs *args, GString *result);

  /* free data in state */
  void (*free_state)(gpointer s);

  /* free LogTemplateFunction instance (if not static) */
  void (*free_fn)(LogTemplateFunction *self);

  /* generic argument that can be used to pass information from registration time */
  gpointer arg;
};

typedef struct _TFSimpleFuncState
{
  gint argc;
  LogTemplate **argv;
} TFSimpleFuncState;

typedef void (*TFSimpleFunc)(LogMessage *msg, gint argc, GString *argv[], GString *result);

gboolean tf_simple_func_prepare(LogTemplateFunction *self, gpointer state, LogTemplate *parent, gint argc, gchar *argv[], GError **error);
void tf_simple_func_eval(LogTemplateFunction *self, gpointer state, const LogTemplateInvokeArgs *args);
void tf_simple_func_call(LogTemplateFunction *self, gpointer state, const LogTemplateInvokeArgs *args, GString *result);
void tf_simple_func_free_state(gpointer state);


#define TEMPLATE_FUNCTION_PROTOTYPE(prefix) \
  gpointer                                                              \
  prefix ## _construct(Plugin *self,                                    \
                       GlobalConfig *cfg,                               \
                       gint plugin_type, const gchar *plugin_name)

#define TEMPLATE_FUNCTION_DECLARE(prefix)	\
  TEMPLATE_FUNCTION_PROTOTYPE(prefix);

/* helper macros for template function plugins */
#define TEMPLATE_FUNCTION(state_struct, prefix, prepare, eval, call, free_state, arg) \
  TEMPLATE_FUNCTION_PROTOTYPE(prefix) 					\
  {                                                                     \
    static LogTemplateFunction func = {                                 \
      sizeof(state_struct),                                             \
      prepare,                                                          \
      eval,                                                             \
      call,                                                             \
      free_state,                                                       \
      NULL,								\
      arg                                                               \
    };                                                                  \
    return &func;                                                       \
  }

#define TEMPLATE_FUNCTION_SIMPLE(x) TEMPLATE_FUNCTION(TFSimpleFuncState, x, tf_simple_func_prepare, tf_simple_func_eval, tf_simple_func_call, tf_simple_func_free_state, x)

#define TEMPLATE_FUNCTION_PLUGIN(x, tf_name) \
  {                                     \
    .type = LL_CONTEXT_TEMPLATE_FUNC,   \
    .name = tf_name,                    \
    .construct = x ## _construct,       \
  }


/* appends the formatted output into result */

void log_template_set_escape(LogTemplate *self, gboolean enable);
gboolean log_template_set_type_hint(LogTemplate *self, const gchar *hint, GError **error);
gboolean log_template_compile(LogTemplate *self, const gchar *template, GError **error);
void log_template_format(LogTemplate *self, LogMessage *lm, const LogTemplateOptions *opts, gint tz, gint32 seq_num, const gchar *context_id, GString *result);
void log_template_append_format(LogTemplate *self, LogMessage *lm, const LogTemplateOptions *opts, gint tz, gint32 seq_num, const gchar *context_id, GString *result);
void log_template_append_format_with_context(LogTemplate *self, LogMessage **messages, gint num_messages, const LogTemplateOptions *opts, gint tz, gint32 seq_num, const gchar *context_id, GString *result);
void log_template_format_with_context(LogTemplate *self, LogMessage **messages, gint num_messages, const LogTemplateOptions *opts, gint tz, gint32 seq_num, const gchar *context_id, GString *result);
void log_template_append_format_recursive(LogTemplate *self, const LogTemplateInvokeArgs *args, GString *result);


/* low level macro functions */
guint log_macro_lookup(gchar *macro, gint len);
gboolean log_macro_expand(GString *result, gint id, gboolean escape, const LogTemplateOptions *opts, gint tz, gint32 seq_num, const gchar *context_id, LogMessage *msg);
gboolean log_macro_expand_simple(GString *result, gint id, LogMessage *msg);

LogTemplate *log_template_new(GlobalConfig *cfg, gchar *name);
LogTemplate *log_template_ref(LogTemplate *s);
void log_template_unref(LogTemplate *s);


void log_template_options_init(LogTemplateOptions *options, GlobalConfig *cfg);
void log_template_options_destroy(LogTemplateOptions *options);
void log_template_options_defaults(LogTemplateOptions *options);

void log_template_global_init(void);
void log_template_global_deinit(void);

gboolean log_template_on_error_parse(const gchar *on_error, gint *out);
void log_template_options_set_on_error(LogTemplateOptions *options, gint on_error);

#endif
