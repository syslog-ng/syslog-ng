/*
 * Copyright (c) 2023 Balazs Scheidler <balazs.scheidler@axoflow.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "python-binding.h"
#include "python-helpers.h"
#include "python-main.h"
#include "string-list.h"
#include "messages.h"

void
python_binding_set_loaders(PythonBinding *self, GList *loaders)
{
  string_list_free(self->loaders);
  self->loaders = loaders;
}

void
python_binding_set_class(PythonBinding *self, gchar *class)
{
  g_free(self->class);
  self->class = g_strdup(class);
}

/*
 * Should be called at log_pipe_init() time.  The Python lock is acquired as
 * a part of this function.
 */
gboolean
python_binding_init(PythonBinding *self, GlobalConfig *cfg, const gchar *desc)
{
  if (!self->class)
    {
      msg_error("Error initializing Python bindings: no class specified",
                evt_tag_str("config", desc));
      return FALSE;
    }

  PyGILState_STATE gstate;
  gboolean result = TRUE;

  gstate = PyGILState_Ensure();
  if (!_py_init_main_module_for_config(python_config_get(cfg)) ||
      !_py_perform_imports(self->loaders))
    result = FALSE;
  PyGILState_Release(gstate);

  return result;
}

void
python_binding_deinit(PythonBinding *self)
{
}

void
python_binding_clone(PythonBinding *self, PythonBinding *cloned)
{
  python_binding_set_class(cloned, self->class);
  python_binding_set_loaders(cloned, string_list_clone(self->loaders));

  python_options_free(cloned->options);
  cloned->options = python_options_clone(self->options);
}

void
python_binding_init_instance(PythonBinding *self)
{
  self->options = python_options_new();
}

void
python_binding_clear(PythonBinding *self)
{
  g_free(self->class);
  string_list_free(self->loaders);
  python_options_free(self->options);
}
