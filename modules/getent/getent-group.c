/*
 * Copyright (c) 2014 Balabit
 * Copyright (c) 2014 Gergely Nagy <algernon@balabit.hu>
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
 */

static formatter_map_t group_field_map[] =
{
  { "name", _getent_format_string, offsetof(struct group, gr_name) },
  { "gid", _getent_format_uid_gid, offsetof(struct group, gr_gid) },
  { "members", _getent_format_array_at_location, offsetof(struct group, gr_mem) },
  { NULL, NULL, 0 }
};

static gboolean
tf_getent_group(gchar *key, gchar *member_name, GString *result)
{
  struct group grp;
  struct group *res;
  char *buf;
  long bufsize;
  int s;
  gint64 d;
  gboolean is_num, r;

  bufsize = 16384;

  buf = g_malloc(bufsize);

  if ((is_num = parse_dec_number(key, &d)) == TRUE)
    s = getgrgid_r((gid_t)d, &grp, buf, bufsize, &res);
  else
    s = getgrnam_r(key, &grp, buf, bufsize, &res);

  if (res == NULL && s != 0)
    {
      msg_error("$(getent group) failed",
                evt_tag_str("key", key),
                evt_tag_error("errno"));
      g_free(buf);
      return FALSE;
    }

  if (member_name == NULL)
    {
      if (is_num)
        member_name = "name";
      else
        member_name = "gid";
    }

  if (res == NULL)
    {
      g_free(buf);
      return FALSE;
    }

  s = _find_formatter(group_field_map, member_name);

  if (s == -1)
    {
      msg_error("$(getent group): unknown member",
                evt_tag_str("key", key),
                evt_tag_str("member", member_name));
      g_free(buf);
      return FALSE;
    }

  r = group_field_map[s].format(member_name,
                                ((uint8_t *)res) + group_field_map[s].offset,
                                result);
  g_free(buf);
  return r;
}
