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

/* display CONTEXT lines before and after the offending line */
#define CONTEXT 5

static void
_format_source_prefix(gchar *line_prefix, gsize line_prefix_len, gint lineno, gboolean error_location)
{
  g_snprintf(line_prefix, line_prefix_len, "%d", lineno);
  if (error_location)
    {
      for (gint i = strlen(line_prefix); i < 6; i++)
        g_strlcat(line_prefix, "-", line_prefix_len);

      g_strlcat(line_prefix, ">", line_prefix_len);
    }
}

static void
_print_underline(const gchar *line, gint whitespace_before, gint number_of_carets)
{
  for (gint i = 0; line[i] && i < whitespace_before; i++)
    {
      fprintf(stderr, "%c", line[i] == '\t' ? '\t' : ' ');
    }

  /* NOTE: sometimes the yylloc has zero characters, print a caret even in
   * this case, that's why i == 0 is there */

  for (gint i = 0; i == 0 || i < number_of_carets; i++)
    fprintf(stderr, "^");
  fprintf(stderr, "\n");
}

static void
_print_underlined_source_block(const CFG_LTYPE *yylloc, gchar **lines, gint error_index)
{
  gint line_ndx;
  gchar line_prefix[12];
  gint error_length = yylloc->last_line - yylloc->first_line + 1;

  for (line_ndx = 0; lines[line_ndx]; line_ndx++)
    {
      gint lineno = yylloc->first_line + line_ndx - error_index;
      const gchar *line = lines[line_ndx];
      gint line_len = strlen(line);
      gboolean line_ends_with_newline = line_len > 0 && line[line_len - 1] == '\n';

      _format_source_prefix(line_prefix, sizeof(line_prefix), lineno,
                            line_ndx >= error_index && line_ndx < error_index + error_length);

      fprintf(stderr, "%-8s%s%s", line_prefix, line, line_ends_with_newline ? "" : "\n");

      if (line_ndx == error_index)
        {
          /* print the underline right below the source line we just printed */
          fprintf(stderr, "%-8s", line_prefix);

          gboolean multi_line = yylloc->first_line != yylloc->last_line;

          _print_underline(line, yylloc->first_column - 1,
                           multi_line ? strlen(&line[yylloc->first_column]) + 1
                           : yylloc->last_column - yylloc->first_column);
        }
      else if (line_ndx >= error_index + CONTEXT)
        break;
    }
}

static void
_report_file_location(const gchar *filename, const CFG_LTYPE *yylloc)
{
  FILE *f;
  gint lineno = 0;
  gsize buflen = 65520;
  gchar *buf = g_malloc(buflen);
  GPtrArray *context = g_ptr_array_new();
  gint error_index = 0;

  f = fopen(filename, "r");
  if (f)
    {
      while (fgets(buf, buflen, f))
        {
          lineno++;
          if (lineno > (gint) yylloc->first_line + CONTEXT)
            break;
          else if (lineno < (gint) yylloc->first_line - CONTEXT)
            continue;
          else if (lineno == yylloc->first_line)
            error_index = context->len;
          g_ptr_array_add(context, g_strdup(buf));
        }
      /* NOTE: do we have the appropriate number of lines? */
      if (lineno <= yylloc->first_line)
        goto exit;
      g_ptr_array_add(context, NULL);
      fclose(f);
    }
  if (context->len > 0)
    _print_underlined_source_block(yylloc, (gchar **) context->pdata, error_index);

exit:
  g_free(buf);
  g_ptr_array_foreach(context, (GFunc) g_free, NULL);
  g_ptr_array_free(context, TRUE);
}

static void
_report_buffer_location(const gchar *buffer_content, const CFG_LTYPE *yylloc)
{
  gchar **lines = g_strsplit(buffer_content, "\n", yylloc->first_line + CONTEXT + 1);
  gint num_lines = g_strv_length(lines);

  if (num_lines <= yylloc->first_line)
    goto exit;

  gint start = yylloc->first_line - 1 - CONTEXT;
  gint error_index = CONTEXT;
  if (start < 0)
    {
      error_index += start;
      start = 0;
    }
  _print_underlined_source_block(yylloc, &lines[start], error_index);

exit:
  g_strfreev(lines);
}

gboolean
cfg_source_print_source_context(CfgLexer *lexer, CfgIncludeLevel *level, const CFG_LTYPE *yylloc)
{
  if (level->include_type == CFGI_FILE)
    {
      _report_file_location(yylloc->name, yylloc);
    }
  else if (level->include_type == CFGI_BUFFER)
    {
      if (level->lloc_changed_by_at_line)
        _report_file_location(yylloc->name, yylloc);
      else
        _report_buffer_location(level->buffer.original_content, yylloc);
    }
  return TRUE;
}

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
