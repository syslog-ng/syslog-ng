/*
 * Copyright (c) 2018 Viktor Tusa <tusavik@gmail.com>
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
#include "python-stats.h"
#include "stats/stats-registry.h"
#include "stats/stats-cluster-single.h"
#include <Python.h>

typedef struct _PyStats
{
  PyObject_HEAD
  StatsCounterItem *counter;
  const char *stats_name;
} PyStats;

static PyTypeObject py_stats_type;

PyObject *
py_stats_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  const gchar *name;
  PyStats *self = (PyStats *) type->tp_alloc(type, 0);

  if (self != NULL)
    {
      if (!PyArg_ParseTuple(args, "s", &name))
        {
          return NULL;
        }
      self->stats_name = g_strdup(name);

      stats_lock();

      StatsClusterKey sc_key;
      stats_cluster_single_key_set(&sc_key, SCS_PYTHON, self->stats_name, NULL);
      stats_register_counter(0, &sc_key, SC_TYPE_SINGLE_VALUE, &(self->counter));

      stats_unlock();
    }
  return (PyObject *) self;
};

static void
py_stats_free(PyStats *self)
{
  stats_lock();
  StatsClusterKey sc_key;
  stats_cluster_single_key_set(&sc_key, SCS_PYTHON, self->stats_name, NULL);
  stats_unregister_counter(&sc_key, SC_TYPE_SINGLE_VALUE, &(self->counter));
  stats_unlock();

  g_free( (void *) self->stats_name);

  PyObject_Del(self);
}

static PyObject *
py_stats_increase(PyStats *self, PyObject *unused)
{
  stats_counter_inc(self->counter);
  Py_RETURN_NONE;
}

static PyObject *
py_stats_decrease(PyStats *self, PyObject *unused)
{
  stats_counter_dec(self->counter);
  Py_RETURN_NONE;
}

static PyObject *
py_stats_add(PyStats *self, PyObject *args)
{
  gssize amount;
  if (!PyArg_ParseTuple(args,"n", &amount))
    {
      return NULL;
    }
  stats_counter_add(self->counter, amount);
  Py_RETURN_NONE;
}

static PyObject *
py_stats_sub(PyStats *self, PyObject *args)
{
  gssize amount;
  if (!PyArg_ParseTuple(args,"n", &amount))
    {
      return NULL;
    }

  stats_counter_sub(self->counter, amount);
  Py_RETURN_NONE;
}

static PyMethodDef py_stats_methods[] =
{
  {
    "increase", (PyCFunction)py_stats_increase,
    METH_NOARGS, "Increase counter"
  },
  {
    "decrease", (PyCFunction)py_stats_decrease,
    METH_NOARGS, "Decrease counter"
  },
  {
    "add", (PyCFunction)py_stats_add,
    METH_VARARGS, "Increase counter with amount"
  },
  {
    "sub", (PyCFunction)py_stats_sub,
    METH_VARARGS, "Decrease counter with amount"
  },
  {NULL}
};

static PyTypeObject py_stats_type =
{
  PyVarObject_HEAD_INIT(&PyType_Type, 0)
  .tp_name = "StatsCounter",
  .tp_basicsize = sizeof(PyStats),
  .tp_dealloc = (destructor) py_stats_free,
  .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
  .tp_doc = "StatsCounter representing a counter in syslog-ng statistics",
  .tp_new = py_stats_new,
  .tp_methods = py_stats_methods,
  .tp_str = NULL,
  .tp_as_mapping = NULL,
  0,
};

static PyMethodDef stats_module_methods[] =
{
  {NULL}
};

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef moduledef =
{
  PyModuleDef_HEAD_INIT,
  "sngstats",     /* m_name */
  "Module for encapsulating StatsCounter type",  /* m_doc */
  -1,                  /* m_size */
  stats_module_methods,    /* m_methods */
  NULL,                /* m_reload */
  NULL,                /* m_traverse */
  NULL,                /* m_clear */
  NULL,                /* m_free */
};
#endif

/* avoid the usage of -fno-strict-aliasing compiler option, because direct casting would break strict aliasing rule */
union caster_py_stats_type
{
  PyObject *object;
  PyTypeObject *type;
};

static PyObject *
py_create_stats_module(void)
{
  PyObject *m;
  union caster_py_stats_type caster = { .type = &py_stats_type };
  if (PyType_Ready(caster.type) < 0)
    return NULL;

#if PY_MAJOR_VERSION >= 3
  m = PyModule_Create(&moduledef);
#else
  m = Py_InitModule3("sngstats", stats_module_methods,
                     "Module for encapsulating StatsCounter type");
#endif

  Py_INCREF(caster.object);
  PyModule_AddObject(m, "StatsCounter", caster.object);
  return m;
}

#if PY_MAJOR_VERSION < 3
PyMODINIT_FUNC PyInit_sngstats(void)
{
  py_create_stats_module();
}
#else
PyMODINIT_FUNC PyInit_sngstats(void)
{
  return py_create_stats_module();
}
#endif

void
python_stats_append_inittab(void)
{
  PyImport_AppendInittab("sngstats", &PyInit_sngstats);
}

