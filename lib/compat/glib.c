/*
 * Copyright (c) 2017 Balabit
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

#include "glib.h"

#ifndef SYSLOG_NG_HAVE_G_LIST_COPY_DEEP

/*
  Less efficient than the original implementation in glib 2.53.2 that
  I wanted to port back, because this version iterates through the
  list twice. Though the original version depends on the internal api
  of GList (for example in terms on memory allocation), so I felt
  safer to reduce the problem to use public glib api only: g_list_copy
  and iteration.
 */

GList *
g_list_copy_deep(GList     *list,
                 GCopyFunc  func,
                 gpointer   user_data)
{
  if (!list)
    return NULL;

  GList *new_list = g_list_copy(list);
  if (func)
    {
      GList *iter = new_list;
      while (iter != NULL)
        {
          iter->data = func(iter->data, user_data);
          iter = g_list_next(iter);
        }
    }

  return new_list;
}

#endif

#ifndef SYSLOG_NG_HAVE_G_QUEUE_FREE_FULL

void
g_queue_free_full(GQueue *queue, GDestroyNotify free_func)
{
  g_queue_foreach(queue, (GFunc) free_func, NULL);
  g_queue_free(queue);
}

#endif

#ifndef SYSLOG_NG_HAVE_G_LIST_FREE_FULL

void
g_list_free_full (GList *list, GDestroyNotify free_func)
{
  g_list_foreach (list, (GFunc) free_func, NULL);
  g_list_free (list);
}

#endif


#ifndef SYSLOG_NG_HAVE_G_PTR_ARRAY_FIND_WITH_EQUAL_FUNC
gboolean
g_ptr_array_find_with_equal_func (GPtrArray     *haystack,
                                  gconstpointer  needle,
                                  GEqualFunc     equal_func,
                                  guint         *index_)
{
  guint i;

  g_return_val_if_fail (haystack != NULL, FALSE);

  if (equal_func == NULL)
    equal_func = g_direct_equal;

  for (i = 0; i < haystack->len; i++)
    {
      if (equal_func (g_ptr_array_index (haystack, i), needle))
        {
          if (index_ != NULL)
            *index_ = i;
          return TRUE;
        }
    }

  return FALSE;
}
#endif

#ifndef SYSLOG_NG_HAVE_G_CANONICALIZE_FILENAME
#include <string.h>
/**
 * g_canonicalize_filename:
 * @filename: (type filename): the name of the file
 * @relative_to: (type filename) (nullable): the relative directory, or %NULL
 * to use the current working directory
 *
 * Gets the canonical file name from @filename. All triple slashes are turned into
 * single slashes, and all `..` and `.`s resolved against @relative_to.
 *
 * Symlinks are not followed, and the returned path is guaranteed to be absolute.
 *
 * If @filename is an absolute path, @relative_to is ignored. Otherwise,
 * @relative_to will be prepended to @filename to make it absolute. @relative_to
 * must be an absolute path, or %NULL. If @relative_to is %NULL, it'll fallback
 * to g_get_current_dir().
 *
 * This function never fails, and will canonicalize file paths even if they don't
 * exist.
 *
 * No file system I/O is done.
 *
 * Returns: (type filename) (transfer full): a newly allocated string with the
 * canonical file path
 * Since: 2.58
 */
gchar *
g_canonicalize_filename (const gchar *filename,
                         const gchar *relative_to)
{
  gchar *canon, *start, *p, *q;
  guint i;

  g_return_val_if_fail (relative_to == NULL || g_path_is_absolute (relative_to), NULL);

  if (!g_path_is_absolute (filename))
    {
      gchar *cwd_allocated = NULL;
      const gchar  *cwd;

      if (relative_to != NULL)
        cwd = relative_to;
      else
        cwd = cwd_allocated = g_get_current_dir ();

      canon = g_build_filename (cwd, filename, NULL);
      g_free (cwd_allocated);
    }
  else
    {
      canon = g_strdup (filename);
    }

  start = (char *)g_path_skip_root (canon);

  if (start == NULL)
    {
      /* This shouldn't really happen, as g_get_current_dir() should
         return an absolute pathname, but bug 573843 shows this is
         not always happening */
      g_free (canon);
      return g_build_filename (G_DIR_SEPARATOR_S, filename, NULL);
    }

  /* POSIX allows double slashes at the start to
   * mean something special (as does windows too).
   * So, "//" != "/", but more than two slashes
   * is treated as "/".
   */
  i = 0;
  for (p = start - 1;
       (p >= canon) &&
       G_IS_DIR_SEPARATOR (*p);
       p--)
    i++;
  if (i > 2)
    {
      i -= 1;
      start -= i;
      memmove (start, start+i, strlen (start+i) + 1);
    }

  /* Make sure we're using the canonical dir separator */
  p++;
  while (p < start && G_IS_DIR_SEPARATOR (*p))
    *p++ = G_DIR_SEPARATOR;

  p = start;
  while (*p != 0)
    {
      if (p[0] == '.' && (p[1] == 0 || G_IS_DIR_SEPARATOR (p[1])))
        {
          memmove (p, p+1, strlen (p+1)+1);
        }
      else if (p[0] == '.' && p[1] == '.' && (p[2] == 0 || G_IS_DIR_SEPARATOR (p[2])))
        {
          q = p + 2;
          /* Skip previous separator */
          p = p - 2;
          if (p < start)
            p = start;
          while (p > start && !G_IS_DIR_SEPARATOR (*p))
            p--;
          if (G_IS_DIR_SEPARATOR (*p))
            *p++ = G_DIR_SEPARATOR;
          memmove (p, q, strlen (q)+1);
        }
      else
        {
          /* Skip until next separator */
          while (*p != 0 && !G_IS_DIR_SEPARATOR (*p))
            p++;

          if (*p != 0)
            {
              /* Canonicalize one separator */
              *p++ = G_DIR_SEPARATOR;
            }
        }

      /* Remove additional separators */
      q = p;
      while (*q && G_IS_DIR_SEPARATOR (*q))
        q++;

      if (p != q)
        memmove (p, q, strlen (q) + 1);
    }

  /* Remove trailing slashes */
  if (p > start && G_IS_DIR_SEPARATOR (*(p-1)))
    *(p-1) = 0;

  return canon;
}

#endif

#ifndef SYSLOG_NG_HAVE_G_HASH_TABLE_CONTAINS
gboolean
g_hash_table_contains (GHashTable    *hash_table,
                       gconstpointer  key)
{
  gpointer orig_key;
  gpointer value;
  return g_hash_table_lookup_extended(hash_table, key, &orig_key, &value);
}
#endif

#if !GLIB_CHECK_VERSION(2, 54, 0)
/**
 * g_base64_encode:
 * @data: (array length=len) (element-type guint8): the binary data to encode
 * @len: the length of @data
 *
 * Encode a sequence of binary data into its Base-64 stringified
 * representation.
 *
 * Returns: (transfer full): a newly allocated, zero-terminated Base-64
 *               encoded string representing @data. The returned string must
 *               be freed with g_free().
 *
 * Since: 2.12
 */
gchar *
g_base64_encode_fixed(const guchar *data, gsize len)
{
  gchar *out;
  gint state = 0, outlen;
  gint save = 0;

  g_return_val_if_fail (data != NULL || len == 0, NULL);

  /* We can use a smaller limit here, since we know the saved state is 0,
     +1 is needed for trailing \0, also check for unlikely integer overflow */
  if (len >= ((G_MAXSIZE - 1) / 4 - 1) * 3)
    g_error("%s: input too large for Base64 encoding (%"G_GSIZE_FORMAT" chars)",
            G_STRLOC, len);

  out = g_malloc ((len / 3 + 1) * 4 + 1);

  outlen = g_base64_encode_step (data, len, FALSE, out, &state, &save);

  /*FIX BEGINS; copied from tf_base64encode*/
  if (((unsigned char *) &save)[0] == 1)
    ((unsigned char *) &save)[2] = 0;
  /*FIX ENDS*/

  outlen += g_base64_encode_close (FALSE, out + outlen, &state, &save);
  out[outlen] = '\0';

  return (gchar *) out;
}
#endif
