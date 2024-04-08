/*
 * Copyright (c) 2024 Balazs Scheidler <balazs.scheidler@axoflow.com>
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
#include "cfg-source.h"

static gboolean
_extract_source_from_file_location(GString *result, const gchar *filename, const CFG_LTYPE *yylloc)
{
  FILE *f;
  gint lineno = 0;
  gint buflen = 65520;
  gchar *line = g_malloc(buflen);

  if (yylloc->first_column < 1 || yylloc->last_column < 1 ||
      yylloc->first_column > buflen-1 || yylloc->last_column > buflen-1)
    return FALSE;

  f = fopen(filename, "r");
  if (!f)
    return FALSE;

  while (fgets(line, buflen, f))
    {
      lineno++;
      gint linelen = strlen(line);
      if (line[linelen-1] == '\n')
        {
          line[linelen-1] = 0;
          linelen--;
        }

      if (lineno > (gint) yylloc->last_line)
        break;
      else if (lineno < (gint) yylloc->first_line)
        continue;
      else if (lineno == yylloc->first_line)
        {
          if (yylloc->first_line == yylloc->last_line)
            g_string_append_len(result, &line[MIN(linelen, yylloc->first_column-1)], yylloc->last_column - yylloc->first_column);
          else
            g_string_append(result, &line[MIN(linelen, yylloc->first_column-1)]);
        }
      else if (lineno < yylloc->last_line)
        {
          g_string_append_c(result, ' ');
          g_string_append(result, line);
        }
      else if (lineno == yylloc->last_line)
        {
          g_string_append_c(result, ' ');
          g_string_append_len(result, line, yylloc->last_column);
        }
    }
  fclose(f);

  /* NOTE: do we have the appropriate number of lines? */
  if (lineno <= yylloc->first_line)
    return FALSE;

  g_free(line);
  return TRUE;
}

static gboolean
_extract_source_from_buffer_location(GString *result, const gchar *buffer_content, const CFG_LTYPE *yylloc)
{
  gchar **lines = g_strsplit(buffer_content, "\n", yylloc->last_line + 1);
  gint num_lines = g_strv_length(lines);

  if (num_lines <= yylloc->first_line)
    goto exit;

  for (gint lineno = yylloc->first_line; lineno <= yylloc->last_line; lineno++)
    {
      gchar *line = lines[lineno - 1];
      gint linelen = strlen(line);

      if (lineno == yylloc->first_line)
        {
          if (yylloc->first_line == yylloc->last_line)
            g_string_append_len(result, &line[MIN(linelen, yylloc->first_column)], yylloc->last_column - yylloc->first_column);
          else
            g_string_append(result, &line[MIN(linelen, yylloc->first_column)]);
        }
      else if (lineno < yylloc->last_line)
        {
          g_string_append_c(result, ' ');
          g_string_append(result, line);
        }
      else if (lineno == yylloc->last_line)
        {
          g_string_append_c(result, ' ');
          g_string_append_len(result, line, yylloc->last_column);
        }
    }

exit:
  g_strfreev(lines);
  return TRUE;
}

gboolean
cfg_source_extract_source_text(CfgLexer *lexer, const CFG_LTYPE *yylloc, GString *result)
{
  CfgIncludeLevel *level = &lexer->include_stack[lexer->include_depth];

  g_string_truncate(result, 0);
  if (level->include_type == CFGI_FILE)
    return _extract_source_from_file_location(result, yylloc->name, yylloc);
  else if (level->include_type == CFGI_BUFFER)
    {
      if (level->lloc_changed_by_at_line)
        return _extract_source_from_file_location(result, yylloc->name, yylloc);
      else
        return _extract_source_from_buffer_location(result, level->buffer.original_content, yylloc);
    }
  else
    g_assert_not_reached();
}
