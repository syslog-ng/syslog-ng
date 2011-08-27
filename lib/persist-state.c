/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
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

#include "persist-state.h"
#include "serialize.h"
#include "messages.h"

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>

typedef struct _PersistFileHeader
{
  union
  {
    struct
    {
      /* should contain SLP4, everything is Big-Endian */

      /* 64 bytes for file header */
      gchar magic[4];
      /* should be zero, any non-zero value is not supported and causes the state to be dropped */
      guint32 flags;
      /* number of name-value keys in the file */
      guint32 key_count;
      /* space reserved for additional information in the header */
      gchar __reserved1[52];
      /* initial key store where the first couple of NV keys are stored, sized to align the header to 4k boundary */
      gchar initial_key_store[4032];
    };
    gchar __padding[4096];
  };
} PersistFileHeader;

#define PERSIST_FILE_INITIAL_SIZE 16384
#define PERSIST_STATE_KEY_BLOCK_SIZE 4096

/*
 * The syslog-ng persistent state is a set of name-value pairs,
 * updated atomically during syslog-ng runtime. When syslog-ng
 * initializes its configuration it allocates chunks of memory to
 * represent its own state which gets mmapped from a file.
 *
 * Whenever syslog-ng writes to the state memory, it gets atomically
 * written to the persistent file.
 *
 * Allocated blocks have a name in order to make it possible to fetch
 * the same state even between syslog-ng restarts.
 *
 * Thus PersistState has two memory areas to represent the information
 *   - the key store containing names and the associated offset
 *     information basically in a sequential file
 *   - the state area containing the values
 *
 * When a new NV pair is registered, syslog-ng allocates the requested
 * area in the state, and writes a new record to the key store
 * file containing the name of the state entry and its length
 * information. If the same name is reused, it is appended to the
 * key store, never updated in-line.
 *
 * If syslog-ng crashes then both the state and the keystore should be
 * left where it was. The information processed the following way:
 *   - the key store file is read into memory and a new key store file is
 *     produced (to ensure that unused state entries are dropped)
 *   - a new state file is produced, again unused data is dropped
 *
 * Old persist files can be converted the same way.
 *
 * We're using a trick to represent both areas in the same file: some
 * space is allocated initially for the key store and once that fills
 * up, syslog-ng allocates another key store area and chains these
 * areas up using a next pointer.
 *
 * Value blocks are prefixed with an 8 byte header, containing the
 * following information:
 *   - block size (4 bytes)
 *   - format version (1 byte)
 *   - whether the block is in use (1 byte)
 *
 * Cleaning up:
 * ------------
 *
 * It can be seen that no explicit deallocation is performed on the
 * persistent file, in effect it could grow indefinitely. There's a
 * simple cleanup procedure though:
 *
 *  - on every startup, the persist file is rewritten, entries with an
 *    in_use bit set are copied to the new one, with the in_use bit cleared
 *  - whenever syslog-ng looks up (e.g. uses) an entry, its in_use bit is set again
 *
 * This way unused entries in the persist file are reaped when
 * syslog-ng restarts.
 *
 * Trusts:
 * -------
 *
 * We don't trust the on-disk file when following a reference
 * (e.g. offset, or object size) however we do trust the internal hash
 * table built after validating the disk contents. This means that if
 * you look up a key in the hashtable you can use the returned offset
 * blindly without checking. However when reading the same value from
 * the file you need to check it whether it is inside the mapped file.
 *
 */
struct _PersistState
{
  gint version;
  gchar *commited_filename;
  gchar *temp_filename;
  gint fd;
  gint mapped_counter;
  guint32 current_size;
  guint32 current_ofs;
  gpointer current_map;
  PersistFileHeader *header;

  /* keys being used */
  GHashTable *keys;
  PersistEntryHandle current_key_block;
  gint current_key_ofs;
  gint current_key_size;
};

typedef struct _PersistEntry
{
  PersistEntryHandle ofs;
} PersistEntry;

/* everything is big-endian */
typedef struct _PersistValueHeader
{
  guint32 size;
  guint8 in_use;
  guint8 version;
  guint16 __padding;
} PersistValueHeader;

/* lowest layer, "store" functions manage the file on disk */

static gboolean
persist_state_grow_store(PersistState *self, guint32 new_size)
{
  int pgsize = getpagesize();

  g_assert(g_atomic_int_get(&self->mapped_counter) == 0);

  if ((new_size & (pgsize-1)) != 0)
    {
      new_size = ((new_size / pgsize) + 1) * pgsize;
    }

  if (new_size > self->current_size)
    {
      gchar zero = 0;

      if (lseek(self->fd, new_size - 1, SEEK_SET) < 0)
        return FALSE;
      if (write(self->fd, &zero, 1) != 1)
        return FALSE;
      if (self->current_map)
        munmap(self->current_map, self->current_size);
      self->current_size = new_size;
      self->current_map = mmap(NULL, self->current_size, PROT_READ | PROT_WRITE, MAP_SHARED, self->fd, 0);
      if (self->current_map == MAP_FAILED)
        {
          self->current_map = NULL;
          return FALSE;
        }
      self->header = (PersistFileHeader *) self->current_map;
      memcpy(&self->header->magic, "SLP4", 4);
    }
  return TRUE;
}

static gboolean
persist_state_create_store(PersistState *self)
{
  self->fd = open(self->temp_filename, O_RDWR | O_CREAT | O_TRUNC, 0600);
  if (self->fd < 0)
    {
      msg_error("Error creating persistent state file",
                evt_tag_str("filename", self->temp_filename),
                evt_tag_errno("error", errno),
                NULL);
      return FALSE;
    }
  self->current_key_block = offsetof(PersistFileHeader, initial_key_store);
  self->current_key_ofs = 0;
  self->current_key_size = sizeof((((PersistFileHeader *) NULL))->initial_key_store);
  return persist_state_grow_store(self, PERSIST_FILE_INITIAL_SIZE);
}

static gboolean
persist_state_commit_store(PersistState *self)
{
  /* NOTE: we don't need to remap the file in case it is renamed */
  return rename(self->temp_filename, self->commited_filename) >= 0;
}

/* "value" layer that handles memory block allocation in the file, without working with keys */

static PersistEntryHandle
persist_state_alloc_value(PersistState *self, guint32 orig_size, gboolean in_use, guint8 version)
{
  PersistEntryHandle result;
  PersistValueHeader *header;
  guint32 size = orig_size;

  /* round up size to 8 bytes boundary */
  if ((size & 0x7))
    size = ((size >> 3) + 1) << 3;

  if (self->current_ofs + size + sizeof(PersistValueHeader) > self->current_size)
    {
      if (!persist_state_grow_store(self, self->current_size + sizeof(PersistValueHeader) + size))
        return 0;
    }

  result = self->current_ofs + sizeof(PersistValueHeader);

  /* fill value header */
  header = (PersistValueHeader *) persist_state_map_entry(self, self->current_ofs);
  header->size = GUINT32_TO_BE(orig_size);
  header->in_use = in_use;
  header->version = version;
  persist_state_unmap_entry(self, self->current_ofs);

  self->current_ofs += size + sizeof(PersistValueHeader);
  return result;
}

static void
persist_state_free_value(PersistState *self, PersistEntryHandle handle)
{
  if (handle)
    {
      PersistValueHeader *header;

      if (handle < self->current_size)
        {
          msg_error("Invalid persistent handle passed to persist_state_free_value",
                    evt_tag_printf("handle", "%08x", handle),
                    NULL);
          return;
        }

      header = (PersistValueHeader *) persist_state_map_entry(self, handle - sizeof(PersistValueHeader));
      if (GUINT32_FROM_BE(header->size) + handle > self->current_size)
        {
          msg_error("Corrupted entry header found in persist_state_free_value, size too large",
                    evt_tag_printf("handle", "%08x", handle),
                    NULL);
          return;
        }
      header->in_use = 0;
      persist_state_unmap_entry(self, handle);
    }
}

/* key management */

gboolean
persist_state_lookup_key(PersistState *self, const gchar *key, PersistEntryHandle *handle)
{
  PersistEntry *entry;

  entry = g_hash_table_lookup(self->keys, key);
  if (entry)
    {
      *handle = entry->ofs;
      return TRUE;
    }
  return FALSE;
}

gboolean
persist_state_add_key(PersistState *self, const gchar *key, PersistEntryHandle handle)
{
  PersistEntry *entry;
  gpointer key_area;
  gboolean new_block_created = FALSE;
  SerializeArchive *sa;

  g_assert(key[0] != 0);

  entry = g_new(PersistEntry, 1);
  entry->ofs = handle;
  g_hash_table_insert(self->keys, g_strdup(key), entry);

  /* we try to insert the key into the current block first, then if it
     doesn't fit, we create a new block */

  while (1)
    {
      /* the size of the key block chain part, 4 byte for the empty string length, guint32 for the link to the next block */
      guint32 chain_size = sizeof(guint32) + sizeof(guint32);
      gboolean success;

      key_area = persist_state_map_entry(self, self->current_key_block);

      /* we reserve space for the next area pointer */
      sa = serialize_buffer_archive_new(key_area + self->current_key_ofs, self->current_key_size - self->current_key_ofs - chain_size);
      sa->silent = TRUE;
      success =
        serialize_write_cstring(sa, key, -1) &&
        serialize_write_uint32(sa, handle);

      if (!success)
        {
          serialize_archive_free(sa);
          if (!new_block_created)
            {
              PersistEntryHandle new_block;

              /* we unmap the key_area as otherwise we can't grow because of the pending maps */
              persist_state_unmap_entry(self, self->current_key_block);

              /* ah, we couldn't fit into the current block, create a new one and link it off the old one */
              new_block = persist_state_alloc_value(self, PERSIST_STATE_KEY_BLOCK_SIZE, TRUE, 0);
              if (!new_block)
                {
                  msg_error("Unable to allocate space in the persistent file for key store",
                            NULL);
                  return FALSE;
                }

              key_area = persist_state_map_entry(self, self->current_key_block);
              sa = serialize_buffer_archive_new(key_area + self->current_key_ofs, self->current_key_size - self->current_key_ofs);
              if (!serialize_write_cstring(sa, "", 0) ||
                  !serialize_write_uint32(sa, new_block))
                {
                  /* hmmm. now this is bad, we couldn't write the tail of the
                     block even though we always reserved space for it, this
                     is a programming error somewhere in this function. */
                  g_assert_not_reached();
                }
              serialize_archive_free(sa);
              persist_state_unmap_entry(self, self->current_key_block);
              self->current_key_block = new_block;
              self->current_key_size = PERSIST_STATE_KEY_BLOCK_SIZE;
              self->current_key_ofs = 0;
              new_block_created = TRUE;
            }
          else
            {
              /* if this happens, that means that the current key
               * entry won't fit even into a freshly initialized key
               * block, this means that the key is too large. */
              msg_error("Persistent key too large, it cannot be larger than somewhat less than 4k",
                        evt_tag_str("key", key),
                        NULL);
              persist_state_unmap_entry(self, self->current_key_block);
              return FALSE;
            }
        }
      else
        {
          self->header->key_count = GUINT32_TO_BE(GUINT32_FROM_BE(self->header->key_count) + 1);
          self->current_key_ofs += serialize_buffer_archive_get_pos(sa);
          serialize_archive_free(sa);
          persist_state_unmap_entry(self, self->current_key_block);
          return TRUE;
        }
    }
  g_assert_not_reached();
}

/* process an on-disk persist file into the current one */

/* function to load v2 and v3 format persistent files */
gboolean
persist_state_load_v23(PersistState *self, gint version, SerializeArchive *sa)
{
  gchar *key, *value;

  while (serialize_read_cstring(sa, &key, NULL))
    {
      gsize len;
      guint32 str_len;
      if (key[0] && serialize_read_cstring(sa, &value, &len))
        {
          gpointer new_block;
          PersistEntryHandle new_handle;

          /*  add length of the string */
          new_handle = persist_state_alloc_value(self, len + sizeof(str_len), FALSE, version);
          new_block = persist_state_map_entry(self, new_handle);

          /* NOTE: we add an extra length field to the old value, as our
           * persist_state_lookup_string() needs that.
           * persist_state_lookup_string is used to fetch disk queue file
           * names.  It could have been solved somewhat better, but now it
           * doesn't justify a persist-state file format change.
           */
          str_len = GUINT32_TO_BE(len);
          memcpy(new_block, &str_len, sizeof(str_len));
          memcpy(new_block + sizeof(str_len), value, len);
          persist_state_unmap_entry(self, new_handle);
          /* add key to the current file */
          persist_state_add_key(self, key, new_handle);
          g_free(value);
          g_free(key);
        }
      else
        {
          g_free(key);
          break;
        }
    }
  return TRUE;
}

gboolean
persist_state_load_v4(PersistState *self)
{
  gint fd;
  gint64 file_size;
  gpointer map;
  gpointer key_block;
  guint32 key_size;
  PersistFileHeader *header;
  gint key_count, i;

  fd = open(self->commited_filename, O_RDONLY);
  if (fd < 0)
    {
      /* no previous data found */
      return TRUE;
    }

  file_size = lseek(fd, 0, SEEK_END);
  if (file_size > ((1LL << 31) - 1))
    {
      msg_error("Persistent file too large",
                evt_tag_str("filename", self->commited_filename),
                evt_tag_printf("size", "%" G_GINT64_FORMAT, file_size),
                NULL);
      return FALSE;
    }
  map = mmap(NULL, file_size, PROT_READ, MAP_SHARED, fd, 0);
  close(fd);
  if (map == MAP_FAILED)
    {
      msg_error("Error mapping persistent file into memory",
                evt_tag_str("filename", self->commited_filename),
                evt_tag_errno("error", errno),
                NULL);
      return FALSE;
    }
  header = (PersistFileHeader *) map;

  key_block = ((gchar *) map) + offsetof(PersistFileHeader, initial_key_store);
  key_size = sizeof((((PersistFileHeader *) NULL))->initial_key_store);

  key_count = GUINT32_FROM_BE(header->key_count);
  i = 0;
  while (i < key_count)
    {
      gchar *name;
      guint32 entry_ofs, chain_ofs;
      SerializeArchive *sa;

      sa = serialize_buffer_archive_new(key_block, key_size);
      while (i < key_count)
        {
          if (!serialize_read_cstring(sa, &name, NULL))
            {
              serialize_archive_free(sa);
              msg_error("Persistent file format error, unable to fetch key name",
                        NULL);
              goto free_and_exit;
            }
          if (name[0])
            {
              if (serialize_read_uint32(sa, &entry_ofs))
                {
                  PersistValueHeader *header;
                  i++;

                  if (entry_ofs < sizeof(PersistFileHeader) || entry_ofs > file_size)
                    {
                      serialize_archive_free(sa);
                      g_free(name);
                      msg_error("Persistent file format error, entry offset is out of bounds",
                                NULL);
                      goto free_and_exit;
                    }

                  header = (PersistValueHeader *) ((gchar *) map + entry_ofs - sizeof(PersistValueHeader));
                  if (header->in_use)
                    {
                      gpointer new_block;
                      PersistEntryHandle new_handle;

                      new_handle = persist_state_alloc_value(self, GUINT32_FROM_BE(header->size), FALSE, header->version);
                      new_block = persist_state_map_entry(self, new_handle);
                      memcpy(new_block, header + 1, GUINT32_FROM_BE(header->size));
                      persist_state_unmap_entry(self, new_handle);
                      /* add key to the current file */
                      persist_state_add_key(self, name, new_handle);
                    }
                  g_free(name);
                }
              else
                {
                  /* bad format */
                  serialize_archive_free(sa);
                  g_free(name);
                  msg_error("Persistent file format error, unable to fetch key name",
                            NULL);
                  goto free_and_exit;
                }
            }
          else
            {
              g_free(name);
              if (serialize_read_uint32(sa, &chain_ofs))
                {
                  /* end of block, chain to the next one */
                  if (chain_ofs == 0 || chain_ofs > file_size)
                    {
                      msg_error("Persistent file format error, key block chain offset is too large or zero",
                                evt_tag_printf("key_block", "%08lx", (gulong) ((gchar *) key_block - (gchar *) map)),
                                evt_tag_printf("key_size", "%d", key_size),
                                evt_tag_int("ofs", chain_ofs),
                                NULL);
                      serialize_archive_free(sa);
                      goto free_and_exit;
                    }
                  key_block = ((gchar *) map) + chain_ofs;
                  key_size = GUINT32_FROM_BE(*(guint32 *) (((gchar *) key_block) - sizeof(PersistValueHeader)));
                  if (chain_ofs + key_size > file_size)
                    {
                      msg_error("Persistent file format error, key block size is too large",
                                evt_tag_int("key_size", key_size),
                                NULL);
                      serialize_archive_free(sa);
                      goto free_and_exit;
                    }
                  break;
                }
              else
                {
                  serialize_archive_free(sa);
                  msg_error("Persistent file format error, unable to fetch chained key block offset",
                            NULL);
                  goto free_and_exit;
                }
            }
        }
    }
 free_and_exit:
  munmap(map, file_size);
  return TRUE;
}

gboolean
persist_state_load(PersistState *self)
{
  FILE *persist_file;
  gboolean success = FALSE;

  persist_file = fopen(self->commited_filename, "r");
  if (persist_file)
    {
      SerializeArchive *sa;
      gchar magic[4];
      gint version;

      sa = serialize_file_archive_new(persist_file);
      serialize_read_blob(sa, magic, 4);
      if (memcmp(magic, "SLP", 3) != 0)
        {
          msg_error("Persistent configuration file is in invalid format, ignoring", NULL);
          success = TRUE;
          goto close_and_exit;
        }
      version = magic[3] - '0';
      if (version >= 2 && version <= 3)
        {
          success = persist_state_load_v23(self, version, sa);
        }
      else if (version == 4)
        {
          success = persist_state_load_v4(self);
        }
      else
        {
          msg_error("Persistent configuration file has an unsupported major version, ignoring",
                    evt_tag_int("version", version),
                    NULL);
          success = TRUE;
        }
    close_and_exit:
      fclose(persist_file);
      serialize_archive_free(sa);
    }
  else
    {
      success = TRUE;
    }
  return success;
}


/**
 * All persistent data accesses must be guarded by a _map and _unmap
 * call in order to get a dereferencable pointer. It is not safe to
 * save this pointer for longer terms as the underlying mapping may
 * change when the file grows.
 **/
gpointer
persist_state_map_entry(PersistState *self, PersistEntryHandle handle)
{
  /* we count the number of mapped entries in order to know if we're
   * safe to remap the file region */

  g_atomic_int_add(&self->mapped_counter, 1);
  return (gpointer) (((gchar *) self->current_map) + (guint32) handle);
}

void
persist_state_unmap_entry(PersistState *self, PersistEntryHandle handle)
{
  g_atomic_int_add(&self->mapped_counter, -1);
}

PersistEntryHandle
persist_state_alloc_entry(PersistState *self, const gchar *persist_name, gsize alloc_size)
{
  PersistEntryHandle handle;

  if (persist_state_lookup_key(self, persist_name, &handle))
    {
      PersistValueHeader *header;

      /* if an entry with the same name is allocated, make sure the
       * old one gets ripped out in the next rebuild of the persistent
       * state */

      header = (PersistValueHeader *) persist_state_map_entry(self, handle - sizeof(PersistValueHeader));
      header->in_use = FALSE;
      persist_state_unmap_entry(self, handle);
    }

  handle = persist_state_alloc_value(self, alloc_size, TRUE, self->version);
  if (!handle)
    return 0;

  if (!persist_state_add_key(self, persist_name, handle))
    {
      persist_state_free_value(self, handle);
      return 0;
    }

  return handle;
}

PersistEntryHandle
persist_state_lookup_entry(PersistState *self, const gchar *key, gsize *size, guint8 *version)
{
  PersistEntryHandle handle;
  PersistValueHeader *header;

  if (!persist_state_lookup_key(self, key, &handle))
    return 0;
  if (handle > self->current_size)
    {
      msg_error("Corrupted handle in persist_state_lookup_entry, handle value too large",
                evt_tag_printf("handle", "%08x", handle),
                NULL);
      return 0;
    }
  header = (PersistValueHeader *) persist_state_map_entry(self, handle - sizeof(PersistValueHeader));
  if (GUINT32_FROM_BE(header->size) + handle > self->current_size)
    {
      msg_error("Corrupted entry header found in persist_state_lookup_entry, size too large",
                evt_tag_printf("handle", "%08x", handle),
                evt_tag_int("size", GUINT32_FROM_BE(header->size)),
                evt_tag_int("file_size", self->current_size),
                NULL);
      return 0;
    }
  header->in_use = TRUE;
  *size = GUINT32_FROM_BE(header->size);
  *version = header->version;
  persist_state_unmap_entry(self, handle);

  return handle;
}

/* easier to use string based interface */
gchar *
persist_state_lookup_string(PersistState *self, const gchar *key, gsize *length, guint8 *version)
{
  PersistEntryHandle handle;
  gpointer block;
  SerializeArchive *sa;
  gchar *result;
  gsize result_len, size;
  guint8 result_version;
  gboolean success;

  if (!(handle = persist_state_lookup_entry(self, key, &size, &result_version)))
    return NULL;
  block = persist_state_map_entry(self, handle);
  sa = serialize_buffer_archive_new(block, size);
  success = serialize_read_cstring(sa, &result, &result_len);
  serialize_archive_free(sa);
  persist_state_unmap_entry(self, handle);
  if (!success)
    return NULL;
  if (length)
    *length = result_len;
  if (version)
    *version = result_version;
  return result;
}

void
persist_state_alloc_string(PersistState *self, const gchar *persist_name, const gchar *value, gssize len)
{
  PersistEntryHandle handle;
  SerializeArchive *sa;
  GString *buf;
  gboolean success;
  gpointer block;

  if (len < 0)
    len = strlen(value);

  buf = g_string_sized_new(len + 5);
  sa = serialize_string_archive_new(buf);

  success = serialize_write_cstring(sa, value, len);
  g_assert(success == TRUE);

  serialize_archive_free(sa);

  handle = persist_state_alloc_entry(self, persist_name, buf->len);
  block = persist_state_map_entry(self, handle);
  memcpy(block, buf->str, buf->len);
  persist_state_unmap_entry(self, handle);
  g_string_free(buf, TRUE);
}

gboolean
persist_state_start(PersistState *self)
{
  if (!persist_state_create_store(self))
    return FALSE;
  if (!persist_state_load(self))
    return FALSE;
  return TRUE;
}

/*
 * This function commits the current store as the "persistent" file by
 * renaming the temp file we created to build the loaded
 * information. Once this function returns, then the current
 * persistent file will be visible to the next relaunch of syslog-ng,
 * even if we crashed.
 */
gboolean
persist_state_commit(PersistState *self)
{
  if (!persist_state_commit_store(self))
    return FALSE;
  return TRUE;
}

/*
 * This routine should revert to the persist_state_new() state,
 * e.g. just like the PersistState object wasn't started yet.
 */
void
persist_state_cancel(PersistState *self)
{
  gchar *commited_filename, *temp_filename;

  close(self->fd);
  munmap(self->current_map, self->current_size);
  unlink(self->temp_filename);
  g_hash_table_destroy(self->keys);
  commited_filename = self->commited_filename;
  temp_filename = self->temp_filename;
  memset(self, 0, sizeof(*self));
  self->commited_filename = commited_filename;
  self->temp_filename = temp_filename;
  self->fd = -1;
  self->keys = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  self->current_ofs = sizeof(PersistFileHeader);
  self->version = 4;
}

PersistState *
persist_state_new(const gchar *filename)
{
  PersistState *self = g_new0(PersistState, 1);

  self->commited_filename = g_strdup(filename);
  self->temp_filename = g_strdup_printf("%s-", self->commited_filename);
  self->keys = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  self->current_ofs = sizeof(PersistFileHeader);
  self->version = 4;
  return self;
}

void
persist_state_free(PersistState *self)
{
  g_assert(g_atomic_int_get(&self->mapped_counter) == 0);
  g_free(self->temp_filename);
  g_free(self->commited_filename);
  g_hash_table_destroy(self->keys);
  g_free(self);
}
