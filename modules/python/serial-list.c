/*
 * Copyright (c) 2019 Balabit
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

#include "serial-list.h"

#include <stdio.h>
#include <string.h>

typedef enum
{
  HEAD,
  FREE_SPACE,
  DATA
} NodeType;

typedef struct
{
  Offset prev;
  Offset next;
  Offset offset;
  NodeType type;
  gsize data_len;
  guchar data[];
} Node;

static Node *
get_node_at_offset(SerialList *self, Offset offset)
{
  g_assert((offset & 3) == 0);

  return (Node *)&self->base[offset];
}

static Node *
get_head(SerialList *self)
{
  return get_node_at_offset(self, 0);
}

static gsize
align(gsize size)
{
  if (size % sizeof(guint64) == 0)
    return size;

  return size + (sizeof(guint64) - size % sizeof(guint64));
}

static void
initialize_head(SerialList *self)
{
  Node *head = get_node_at_offset(self, 0);
  head->prev = 0;
  head->next = 0;
  head->offset = 0;
  head->type = HEAD;
  head->data_len = 0;
}

static void
initialize_free_space(SerialList *self)
{
  Offset free_space_offset = align(sizeof(Node));
  Node *free_space = get_node_at_offset(self, free_space_offset);
  Node *head = get_head(self);

  free_space->prev = 0;
  free_space->next = 0;
  free_space->offset = free_space_offset;
  free_space->type = FREE_SPACE;
  free_space->data_len = self->max_size - 2 * sizeof(Node);

  head->prev = free_space_offset;
  head->next = free_space_offset;
}

SerialList *
serial_list_new(guchar *base, gsize size)
{
  g_assert(size > 2 * sizeof(Node));

  SerialList *self = g_new0(SerialList, 1);
  self->base = base;
  self->max_size = size;

  initialize_head(self);
  initialize_free_space(self);

  return self;
}

void
serial_list_free(SerialList *self)
{
  g_free(self);
}

/* The data length might be smaller than the available space.
   This function calculates the available space. This can be useful
   for example when we want to update a data node with larger data.
   The longer data might fit into the same space. */
static gsize
get_available_space(SerialList *self, Node *node)
{
  if (node->next)
    return node->next - offsetof(Node, data) - node->offset;
  else
    return self->max_size - offsetof(Node, data) - node->offset;
}

static Node *
find_free_space(SerialList *self, gsize data_size)
{
  Node *node = get_node_at_offset(self, get_head(self)->next);

  while (TRUE)
    {
      if (node->type == HEAD)
        return NULL;

      if (node->type == FREE_SPACE && get_available_space(self, node) >= align(data_size))
        return node;

      node = get_node_at_offset(self, node->next);
    }
}

static SerialListHandle
split_free_space_and_insert(SerialList *self, Node *free_space, guchar *data, gsize data_len)
{
  gsize node_alloc_size = sizeof(Node) + align(data_len);
  Node free_space_copy = *free_space;
  Node *prev_node = get_node_at_offset(self, free_space_copy.prev);
  Node *next_node = get_node_at_offset(self, free_space_copy.next);

  Node *new_node = get_node_at_offset(self, free_space_copy.offset);
  new_node->offset = free_space_copy.offset;
  Node *new_free_space = get_node_at_offset(self, free_space_copy.offset + node_alloc_size);
  new_free_space->offset = free_space_copy.offset + node_alloc_size;

  new_node->prev = prev_node->offset;
  new_node->next = new_free_space->offset;
  new_node->type = DATA;
  new_node->data_len = data_len;
  memcpy(new_node->data, data, data_len);

  new_free_space->prev = new_node->offset;
  new_free_space->next = next_node->offset;
  new_free_space->type = FREE_SPACE;
  new_free_space->data_len = free_space_copy.data_len - (sizeof(Node) + node_alloc_size);

  prev_node->next = new_node->offset;
  next_node->prev = new_free_space->offset;

  return new_node->offset;
}

static SerialListHandle
convert_free_space(SerialList *self, Node *node, guchar *data, gsize data_len)
{
  node->type = DATA;
  memcpy(node->data, data, data_len);
  node->data_len = data_len;
  return node->offset;
}

SerialListHandle
serial_list_insert(SerialList *self, guchar *data, gsize data_len)
{
  Node *free_space = find_free_space(self, data_len);
  if (!free_space)
    return 0;

  if (get_available_space(self, free_space) >= sizeof(Node) + align(data_len) + sizeof(Node))
    return split_free_space_and_insert(self, free_space, data, data_len);
  else
    {
      return convert_free_space(self, free_space, data, data_len);
    }
}

SerialListHandle
serial_list_find(SerialList *self, SerialListFindFunc func, gpointer user_data)
{
  Node *node = get_node_at_offset(self, get_head(self)->next);

  while (TRUE)
    {
      if (node->type == HEAD)
        return 0;

      if (node->type == DATA && func(node->data, node->data_len, user_data))
        return node->offset;

      node = get_node_at_offset(self, node->next);
    }
}

const guchar *
serial_list_get_data(SerialList *self, SerialListHandle handle, const guchar **data, gsize *data_len)
{
  Node *node = get_node_at_offset(self, handle);
  if (data)
    *data = node->data;
  if (data_len)
    *data_len = node->data_len;

  return node->data;
}

static void
join_free_spaces(SerialList *self, Node *first, Node *second)
{
  first->next = second->next;
  first->data_len = get_available_space(self, first);
}

void
serial_list_remove(SerialList *self, SerialListHandle handle)
{
  Node *node = get_node_at_offset(self, handle);
  gsize new_free_space = get_available_space(self, node);

  node->type = FREE_SPACE;
  node->data_len = new_free_space;

  Node *next = get_node_at_offset(self, node->next);
  if (next->type == FREE_SPACE)
    join_free_spaces(self, node, next);

  Node *prev = get_node_at_offset(self, node->prev);

  if (prev->type == FREE_SPACE)
    join_free_spaces(self, prev, node);
}

SerialListHandle
serial_list_update(SerialList *self, SerialListHandle handle, guchar *data, gsize data_len)
{
  Node *node = get_node_at_offset(self, handle);
  gsize free_space = get_available_space(self, node);
  if (free_space >= data_len)
    {
      memcpy(node->data, data, data_len);
      node->data_len = data_len;
      return node->offset;
    }
  else
    {
      SerialListHandle new_handle = serial_list_insert(self, data, data_len);
      if (!new_handle)
        return 0;

      serial_list_remove(self, handle);
      return new_handle;
    }
}

void
serial_list_foreach(SerialList *self, SerialListFunc func, gpointer user_data)
{
  Node *node = get_node_at_offset(self, get_head(self)->next);

  while (TRUE)
    {
      if (node->type == DATA)
        func(node->data, node->data_len, user_data);

      if (node->type == HEAD)
        return;

      node = get_node_at_offset(self, node->next);
    }
}

void
serial_list_print(SerialList *self)
{
  Node *node = get_node_at_offset(self, get_head(self)->next);

  while (TRUE)
    {
      fprintf(stderr, "Node: offset: %lu, prev: %lu, next: %lu, type: %d, data_len: %lu, data: %.*s\n",
              node->offset, node->prev, node->next, node->type, node->data_len,
              node->type != FREE_SPACE ? (int)node->data_len : 0, node->data);

      if (node->type == HEAD)
        return;

      node = get_node_at_offset(self, node->next);
    }

}

void
serial_list_rebase(SerialList *self, guchar *new_base, gsize orig_new_size)
{
  gsize new_size = orig_new_size & ~3;
  gsize orig_max_size = self->max_size;

  g_assert(self->max_size <= new_size);

  self->base = new_base;
  self->max_size = new_size;

  if (new_size - orig_max_size >= sizeof(Node))
    {
      Node *prev = get_node_at_offset(self, get_head(self)->prev);
      Node *head = get_head(self);

      Node *new_free_space = get_node_at_offset(self, orig_max_size);
      new_free_space->offset = orig_max_size;
      new_free_space->next = 0;
      new_free_space->prev = prev->offset;
      new_free_space->type = FREE_SPACE;

      prev->next = new_free_space->offset;
      head->prev = new_free_space->offset;

      new_free_space->data_len = get_available_space(self, new_free_space);

      if (prev->type == FREE_SPACE)
        join_free_spaces(self, prev, new_free_space);
    }
}

SerialList *
serial_list_load(guchar *base, gsize size)
{
  SerialList *self = g_new0(SerialList, 1);
  self->base = base;
  self->max_size = size;

  return self;
}

void
serial_list_handle_foreach(SerialList *self, SerialListHandleFunc func, gpointer user_data)
{
  Node *node = get_node_at_offset(self, get_head(self)->next);

  while (TRUE)
    {
      if (node->type == DATA)
        func(self, node->offset, user_data);

      if (node->type == HEAD)
        return;

      node = get_node_at_offset(self, node->next);
    }
}
