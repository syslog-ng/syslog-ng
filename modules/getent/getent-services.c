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

static gboolean
tf_getent_services(gchar *key, gchar *member_name, GString *result)
{
  struct servent serv, *res;
  gint64 d;
  gboolean is_num;
  char buf[4096];

  if ((is_num = parse_int64(key, &d)) == TRUE)
    getservbyport_r((int)ntohs(d), NULL, &serv, buf, sizeof(buf), &res);
  else
    getservbyname_r(key, NULL, &serv, buf, sizeof(buf), &res);

  if (res == NULL)
    return TRUE;

  if (is_num)
    g_string_append(result, res->s_name);
  else
    g_string_append_printf(result, "%i", htons(res->s_port));

  return TRUE;
}
