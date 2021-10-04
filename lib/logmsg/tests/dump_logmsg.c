/*
 * Copyright (c) 2021 Balazs Scheidler <bazsi77@gmail.com>
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
#include "test_logmsg_serialize.c"


GString *
_translate_version(const gchar *version)
{
  GString *r = g_string_new(version);

  for (gsize i = 0; i < r->len; i++)
    {
      if (r->str[i] == '.')
        r->str[i] = '_';
      if (r->str[i] == '+')
        {
          g_string_truncate(r, i);
          break;
        }
    }
  return r;
}

static void
_dump_serialized_bytes(const gchar *translated_version)
{
  GString *data = g_string_new("");
  SerializeArchive *sa = _serialize_message_for_test(data, RAW_MSG);

  printf("unsigned char serialized_message_%s[] = {\n", translated_version);
  printf("  /* serialized payload %d bytes */\n", (gint) data->len);
  for (gsize line_start = 0; line_start < data->len; line_start += 16)
    {
      for (gint row = 0; row < 16 && line_start + row < data->len; row++)
        {
          gsize cell = line_start + row;

          printf("%s0x%02x%s",
                 row == 0 ? "  " : "",
                 (guint8) data->str[cell],
                 row == 15 ? ",\n" : ", ");
        }
    }

  serialize_archive_free(sa);
  printf("\n};\n");
}

int main(int argc, char *argv[])
{
  setup();

  if (argc < 2)
    {
      fprintf(stderr, "Expected <version> number argument.\n");
      teardown();
      return 1;
    }
  GString *translated_version = _translate_version(argv[1]);

  _dump_serialized_bytes(translated_version->str);

  g_string_free(translated_version, TRUE);
  teardown();
}
