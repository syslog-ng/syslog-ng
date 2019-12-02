/*
 * Copyright (c) 2002-2019 Balabit
 * Copyright (c) 2019 Laszlo Budai <laszlo.budai@balabit.com>
 *
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

#include "http-curl-header-list.h"

typedef struct _HttpCurlHeaderList HttpCurlHeaderList;

struct _HttpCurlHeaderList
{
  List super;
  struct curl_slist *list;
};

static void
_append(List *s, gconstpointer item)
{
  HttpCurlHeaderList *self = (HttpCurlHeaderList *)s;
  self->list = curl_slist_append(self->list, item);
}

static void
_foreach(List *s, list_foreach_fn foreach_fn, gpointer user_data)
{
  HttpCurlHeaderList *self = (HttpCurlHeaderList *)s;
  struct curl_slist *it = self->list;
  while (it != NULL)
    {
      struct curl_slist *next = it->next;
      foreach_fn(it->data, user_data);
      it = next;
    }
}

static gboolean
_is_empty(List *s)
{
  HttpCurlHeaderList *self = (HttpCurlHeaderList *)s;

  return self->list == NULL;
}

static void
_remove_all(List *s)
{
  HttpCurlHeaderList *self = (HttpCurlHeaderList *)s;
  curl_slist_free_all(self->list);
  self->list = NULL;
}

static void
_free_fn(List *s)
{
  HttpCurlHeaderList *self = (HttpCurlHeaderList *)s;
  _remove_all(s);
}

List *
http_curl_header_list_new(void)
{
  HttpCurlHeaderList *self = g_new0(HttpCurlHeaderList, 1);

  self->super.append = _append;
  self->super.foreach = _foreach;
  self->super.is_empty = _is_empty;
  self->super.remove_all = _remove_all;
  self->super.free_fn = _free_fn;

  return &self->super;
}

struct curl_slist *
http_curl_header_list_as_slist(List *s)
{
  HttpCurlHeaderList *self = (HttpCurlHeaderList *)s;

  return self->list;
}

