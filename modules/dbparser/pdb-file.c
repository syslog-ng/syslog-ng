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
#ifdef SYSLOG_NG_ENABLE_DEBUG
  const gchar *srcdir;
  static gchar path[256];

  srcdir = getenv("top_srcdir");
  if (srcdir)
    {
      g_snprintf(path, sizeof(path), "%s/doc/xsd", srcdir);
      return path;
    }
#endif
  return NULL;
}

static const gchar *
_get_xsddir_in_production(void)
{
  return get_installation_path_for(SYSLOG_NG_PATH_XSDDIR);
}

static const gchar *
_get_xsddir(void)
{
  return _get_xsddir_in_build() ? : _get_xsddir_in_production();
}

static gchar *
_get_xsd_file(gint version)
{
  return g_strdup_printf("%s/patterndb-%d.xsd", _get_xsddir(), version);
}

gboolean
pdb_file_validate(const gchar *filename, GError **error)
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

  xsd_file = _get_xsd_file(version);
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
