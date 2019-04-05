/*
 * Copyright (c) 2002-2016 Balabit
 * Copyright (c) 1998-2016 Balázs Scheidler
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

#include "pdb-file.h"
#include "pdb-error.h"
#include "reloc.h"
#include "pathutils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>

gint
pdb_file_detect_version(const gchar *pdbfile, GError **error)
{
  FILE *pdb;
  gchar line[1024];
  gint result = 0;

  pdb = fopen(pdbfile, "r");
  if (!pdb)
    {
      g_set_error(error, PDB_ERROR, PDB_ERROR_FAILED, "Error opening file %s (%s)", pdbfile, g_strerror(errno));
      return 0;
    }

  while (fgets(line, sizeof(line), pdb))
    {
      gchar *patterndb_tag;

      patterndb_tag = strstr(line, "<patterndb");
      if (patterndb_tag)
        {
          gchar *version, *start_quote, *end_quote;

          /* ok, we do have the patterndb tag, look for the version attribute */
          version = strstr(patterndb_tag, "version=");

          if (!version)
            goto exit;
          start_quote = version + 8;
          end_quote = strchr(start_quote + 1, *start_quote);
          if (!end_quote)
            {
              goto exit;
            }
          *end_quote = 0;
          result = strtoll(start_quote + 1, NULL, 0);
          break;
        }
    }
exit:
  fclose(pdb);
  if (!result)
    {
      g_set_error(error, PDB_ERROR, PDB_ERROR_FAILED,
                  "Error detecting pdbfile version, <patterndb> version attribute not found or <patterndb> is not on its own line");
    }
  return result;
}

static const gchar *
_get_xsddir_in_build(void)
{
  static gchar path[256];

  g_snprintf(path, sizeof(path), "%s/doc/xsd", SYSLOG_NG_PATH_TOPSRC_DIR);
  return path;
}

static const gchar *
_get_xsddir_in_production(void)
{
  return get_installation_path_for(SYSLOG_NG_PATH_XSDDIR);
}

typedef const gchar *(*PdbGetXsdDirFunc) (void);

static gchar *
_get_xsd_file(gint version, PdbGetXsdDirFunc get_xsd_dir)
{
  return g_strdup_printf("%s/patterndb-%d.xsd", get_xsd_dir(), version);
}

static gboolean
_pdb_file_validate(const gchar *filename, GError **error, PdbGetXsdDirFunc get_xsd_dir)
{
  gchar *xmllint_cmdline;
  gint version;
  gint exit_status;
  gchar *stderr_content = NULL;
  gchar *xsd_file;

  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  version = pdb_file_detect_version(filename, error);
  if (!version)
    return FALSE;

  xsd_file = _get_xsd_file(version, get_xsd_dir);
  if (!is_file_regular(xsd_file))
    {
      g_set_error(error, PDB_ERROR, PDB_ERROR_FAILED, "XSD file is not available at %s", xsd_file);
      g_free(xsd_file);
      return FALSE;
    }

  xmllint_cmdline = g_strdup_printf("xmllint --noout --nonet --schema '%s' '%s'", xsd_file, filename);
  g_free(xsd_file);

  if (!g_spawn_command_line_sync(xmllint_cmdline, NULL, &stderr_content, &exit_status, error))
    {
      g_free(xmllint_cmdline);
      g_free(stderr_content);
      return FALSE;
    }

  if (exit_status != 0)
    {
      g_set_error(error, PDB_ERROR, PDB_ERROR_FAILED,
                  "Non-zero exit code from xmllint while validating PDB file, "
                  "schema version %d, rc=%d, error: %s, command line %s",
                  version,
                  WEXITSTATUS(exit_status), stderr_content,
                  xmllint_cmdline);
      g_free(stderr_content);
      g_free(xmllint_cmdline);
      return FALSE;
    }
  g_free(xmllint_cmdline);
  g_free(stderr_content);
  return TRUE;
}

gboolean
pdb_file_validate(const gchar *filename, GError **error)
{
  return _pdb_file_validate(filename, error, _get_xsddir_in_production);
}

gboolean
pdb_file_validate_in_tests(const gchar *filename, GError **error)
{
  return _pdb_file_validate(filename, error, _get_xsddir_in_build);
}

GPtrArray *
pdb_get_filenames(const gchar *dir_path, gboolean recursive, gchar *pattern, GError **error)
{
  GDir *dir;

  if ((dir = g_dir_open(dir_path, 0, error)) == NULL)
    return NULL;

  GPtrArray *filenames = g_ptr_array_new_with_free_func(g_free);
  const gchar *path;

  while ((path = g_dir_read_name(dir)) != NULL)
    {
      gchar *full_path = g_build_filename(dir_path, path, NULL);
      if (recursive && is_file_directory(full_path))
        {
          GPtrArray *recursive_filenames = pdb_get_filenames(full_path, recursive, pattern, error);
          if (recursive_filenames == NULL)
            {
              g_ptr_array_free(recursive_filenames, TRUE);
              g_ptr_array_free(filenames, TRUE);
              g_free(full_path);
              g_dir_close(dir);
              return NULL;
            }
          for (guint i = 0; i < recursive_filenames->len; ++i)
            g_ptr_array_add(filenames, g_ptr_array_index(recursive_filenames, i));
          g_ptr_array_free(recursive_filenames, FALSE);
          g_free(full_path);
        }
      else if (is_file_regular(full_path) && (!pattern || g_pattern_match_simple(pattern, full_path)))
        {
          g_ptr_array_add(filenames, full_path);
        }
      else
        {
          g_free(full_path);
        }
    }
  g_dir_close(dir);

  return filenames;
}

static guint
pdbtool_get_path_depth(const gchar *path)
{
  gint depth = 0;

  while (*path)
    {
      if (*path++ == '/')
        ++depth;
    }

  return depth;
}

static int
pdbtool_path_compare(gconstpointer a, gconstpointer b)
{
  const gchar *path_a = *(const gchar **)a;
  const gchar *path_b = *(const gchar **)b;

  guint a_depth = pdbtool_get_path_depth(path_a);
  guint b_depth = pdbtool_get_path_depth(path_b);

  if (a_depth > b_depth)
    return 1;
  else if (a_depth < b_depth)
    return -1;
  else
    return strcmp(path_a, path_b);
}

void
pdb_sort_filenames(GPtrArray *filenames)
{
  g_ptr_array_sort(filenames, pdbtool_path_compare);
}
