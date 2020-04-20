/*
 * Copyright (c) 2002-2014 Balabit
 * Copyright (c) 1998-2014 Balázs Scheidler
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

#ifndef TEMPLATE_FUNCTION_H_INCLUDED
#define TEMPLATE_FUNCTION_H_INCLUDED

#include "syslog-ng.h"
#include "plugin-types.h"
#include "common-template-typedefs.h"

#define TEMPLATE_INVOKE_MAX_ARGS 64

/* This structure contains the arguments for template-function
 * expansion. It is defined in a struct because otherwise a large
 * number of function arguments, that are passed around, possibly
 * several times. */
typedef struct _LogTemplateInvokeArgs
{
  /* context in case of correllation */
  LogMessage **messages;
  gint num_messages;

  /* options for recursive template evaluation, inherited from the parent */
  const LogTemplateOptions *opts;
  gint tz;
  gint seq_num;
  const gchar *context_id;
  GString *argv[TEMPLATE_INVOKE_MAX_ARGS];
} LogTemplateInvokeArgs;

typedef struct _LogTemplateFunction LogTemplateFunction;
struct _LogTemplateFunction
{
  /* size of the state that carries information from parse-time to
   * runtime. Can be used to store the results of expensive
   * operations that don't need to be performed for all invocations */
  gint size_of_state;

  /* called when parsing the arguments to be compiled into an internal
   * representation if necessary.  Returns the compiled state in state */
  gboolean (*prepare)(LogTemplateFunction *self, gpointer state, LogTemplate *parent, gint argc, gchar *argv[],
                      GError **error);

  /* evaluate arguments, storing argument buffers in arg_bufs in case it
   * makes sense to reuse those buffers */
  void (*eval)(LogTemplateFunction *self, gpointer state, LogTemplateInvokeArgs *args);

  /* call the function */
  void (*call)(LogTemplateFunction *self, gpointer state, const LogTemplateInvokeArgs *args, GString *result);

  /* free data in state */
  void (*free_state)(gpointer s);

  /* free LogTemplateFunction instance (if not static) */
  void (*free_fn)(LogTemplateFunction *self);

  /* generic argument that can be used to pass information from registration time */
  gpointer arg;
};

#define TEMPLATE_FUNCTION_PROTOTYPE(prefix) \
  gpointer                                                              \
  prefix ## _construct(Plugin *self)

#define TEMPLATE_FUNCTION_DECLARE(prefix) \
  TEMPLATE_FUNCTION_PROTOTYPE(prefix);

/* helper macros for template function plugins */
#define TEMPLATE_FUNCTION(state_struct, prefix, prepare, eval, call, free_state, arg) \
  TEMPLATE_FUNCTION_PROTOTYPE(prefix)           \
  {                                                                     \
    static LogTemplateFunction func = {                                 \
      sizeof(state_struct),                                             \
      prepare,                                                          \
      eval,                                                             \
      call,                                                             \
      free_state,                                                       \
      NULL,               \
      arg                                                               \
    };                                                                  \
    return &func;                                                       \
  }

#define TEMPLATE_FUNCTION_PLUGIN(x, tf_name) \
  {                                     \
    .type = LL_CONTEXT_TEMPLATE_FUNC,   \
    .name = tf_name,                    \
    .construct = x ## _construct,       \
  }

#endif
