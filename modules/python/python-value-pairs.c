#include "python-value-pairs.h"

/** Value pairs **/

static gboolean
python_worker_vp_add_one(const gchar *name,
                       TypeHint type, const gchar *value,
                       gpointer user_data)
{
  const LogTemplateOptions *template_options = (const LogTemplateOptions *)((gpointer *)user_data)[0];
  PyObject *dict = (PyObject *)((gpointer *)user_data)[1];
  gboolean need_drop = FALSE;
  gboolean fallback = template_options->on_error & ON_ERROR_FALLBACK_TO_STRING;

  switch (type)
    {
    case TYPE_HINT_INT32:
    case TYPE_HINT_INT64:
      {
        gint64 i;

        if (type_cast_to_int64(value, &i, NULL))
          PyDict_SetItemString(dict, name, PyLong_FromLong(i));
        else
          {
            need_drop = type_cast_drop_helper(template_options->on_error,
                                              value, "int");

            if (fallback)
              PyDict_SetItemString(dict, name, PyUnicode_FromString(value));
          }
        break;
      }
    case TYPE_HINT_STRING:
      PyDict_SetItemString(dict, name, PyUnicode_FromString(value));
      break;
    default:
      need_drop = type_cast_drop_helper(template_options->on_error,
                                        value, "<unknown>");
      break;
    }
  return need_drop;
}

/** Main code **/

gboolean
py_value_pairs_apply(ValuePairs *vp, const LogTemplateOptions *template_options, guint32 seq_num, LogMessage *msg, PyObject **dict)
{
  gpointer args[2];
  gboolean vp_ok;

  *dict = PyDict_New();

  args[0] = (gpointer) template_options;
  args[1] = *dict;

  vp_ok = value_pairs_foreach(vp, python_worker_vp_add_one,
                              msg, seq_num, LTZ_LOCAL, template_options,
                              args);
  if (!vp_ok)
    {
      Py_DECREF(*dict);
      *dict = NULL;
    }
  return vp_ok;
}
