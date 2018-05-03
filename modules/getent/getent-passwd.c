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

static formatter_map_t passwd_field_map[] =
{
  { "name", _getent_format_string, offsetof(struct passwd, pw_name) },
  { "uid", _getent_format_uid_gid, offsetof(struct passwd, pw_uid) },
  { "gid", _getent_format_uid_gid, offsetof(struct passwd, pw_gid) },
  { "gecos", _getent_format_string, offsetof(struct passwd, pw_gecos) },
  { "dir", _getent_format_string, offsetof(struct passwd, pw_dir) },
  { "shell", _getent_format_string, offsetof(struct passwd, pw_shell) },
  { NULL, NULL, 0 }
};

static gboolean
tf_getent_passwd(gchar *key, gchar *member_name, GString *result)
{
  struct passwd pwd;
  struct passwd *res;
  char *buf;
  long bufsize;
  int s;
  glong d;
  gboolean is_num, r;

  bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
  if (bufsize == -1)
    bufsize = 16384;

  buf = g_malloc(bufsize);

  if ((is_num = parse_number(key, &d)) == TRUE)
    s = getpwuid_r((uid_t)d, &pwd, buf, bufsize, &res);
  else
    s = getpwnam_r(key, &pwd, buf, bufsize, &res);

  if (res == NULL && s != 0)
    {
      msg_error("$(getent passwd) failed",
                evt_tag_str("key", key),
                evt_tag_error("errno"),
                NULL);
      g_free(buf);
      return FALSE;
    }

  if (member_name == NULL)
    {
      if (is_num)
        member_name = "name";
      else
        member_name = "uid";
    }

  if (res == NULL)
    {
      g_free(buf);
      return FALSE;
    }

  s = _find_formatter(passwd_field_map, member_name);

  if (s == -1)
    {
      msg_error("$(getent passwd): unknown member",
                evt_tag_str("key", key),
                evt_tag_str("member", member_name),
                NULL);
      g_free(buf);
      return FALSE;
    }

  r = passwd_field_map[s].format(member_name,
                                 ((uint8_t *)res) + passwd_field_map[s].offset,
                                 result);
  g_free(buf);
  return r;
}
