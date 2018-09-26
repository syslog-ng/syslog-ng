/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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
#include "fdhelpers.h"

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

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
#define PERSIST_FILE_WATERMARK 4096

typedef struct
{
  void (*handler)(gpointer user_data);
  gpointer cookie;
} PersistStateErrorHandler;

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
  gchar *committed_filename;
  gchar *temp_filename;
  gint fd;
  gint mapped_counter;
  GMutex *mapped_lock;
  GCond *mapped_release_cond;
  guint32 current_size;
  guint32 current_ofs;
  gpointer current_map;
  PersistFileHeader *header;
  PersistStateErrorHandler error_handler;

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

static void
_wait_until_map_release(PersistState *self)
{
  g_mutex_lock(self->mapped_lock);
  while (self->mapped_counter)
    g_cond_wait(self->mapped_release_cond, self->mapped_lock);
}

static gboolean
_increase_file_size(PersistState *self, guint32 new_size)
{
  gboolean result = TRUE;
  ssize_t length = new_size - self->current_size;
  gchar *pad_buffer = g_new0(gchar, length);
  ssize_t rc = pwrite(self->fd, pad_buffer, length, self->current_size);
  if (rc != length)
    {
      msg_error("Can't grow the persist file",
                evt_tag_int("old_size", self->current_size),
                evt_tag_int("new_size", new_size),
                evt_tag_str("error", rc < 0 ? g_strerror(errno) : "short write"));
      result = FALSE;
    }
  g_free(pad_buffer);
  return result;
}

static gboolean
_grow_store(PersistState *self, guint32 new_size)
{
  int pgsize = getpagesize();
  gboolean result = FALSE;

  _wait_until_map_release(self);

  if ((new_size & (pgsize-1)) != 0)
    {
      new_size = ((new_size / pgsize) + 1) * pgsize;
    }

  if (new_size > self->current_size)
    {
      if (!_increase_file_size(self, new_size))
        goto exit;

      if (self->current_map)
        munmap(self->current_map, self->current_size);
      self->current_size = new_size;
      self->current_map = mmap(NULL, self->current_size, PROT_READ | PROT_WRITE, MAP_SHARED, self->fd, 0);
      if (self->current_map == MAP_FAILED)
        {
          self->current_map = NULL;
          goto exit;
        }
      self->header = (PersistFileHeader *) self->current_map;
      memcpy(&self->header->magic, "SLP4", 4);
    }
  result = TRUE;
exit:
  g_mutex_unlock(self->mapped_lock);
  return result;
}

static gboolean
_create_store(PersistState *self)
{
  self->fd = open(self->temp_filename, O_RDWR | O_CREAT | O_TRUNC, 0600);
  if (self->fd < 0)
    {
      msg_error("Error creating persistent state file",
                evt_tag_str("filename", self->temp_filename),
                evt_tag_error("error"));
      return FALSE;
    }
  g_fd_set_cloexec(self->fd, TRUE);
  self->current_key_block = offsetof(PersistFileHeader, initial_key_store);
  self->current_key_ofs = 0;
  self->current_key_size = sizeof((((PersistFileHeader *) NULL))->initial_key_store);
  return _grow_store(self, PERSIST_FILE_INITIAL_SIZE);
}

static gboolean
_commit_store(PersistState *self)
{
  /* NOTE: we don't need to remap the file in case it is renamed */
  return rename(self->temp_filename, self->committed_filename) >= 0;
}

static gboolean
_check_watermark(PersistState *self)
{
  return (self->current_ofs + PERSIST_FILE_WATERMARK < self->current_size);
}

static inline gboolean
_check_free_space(PersistState *self, guint32 size)
{
  return (size + sizeof(PersistValueHeader) +  self->current_ofs) <= self->current_size;
}

static void
persist_state_run_error_handler(PersistState *self)
{
  if (self->error_handler.handler)
    self->error_handler.handler(self->error_handler.cookie);
}

/* "value" layer that handles memory block allocation in the file, without working with keys */

static PersistEntryHandle
_alloc_value(PersistState *self, guint32 orig_size, gboolean in_use, guint8 version)
{
  PersistEntryHandle result;
  PersistValueHeader *header;
  guint32 size = orig_size;

  /* round up size to 8 bytes boundary */
  if ((size & 0x7))
    size = ((size >> 3) + 1) << 3;

  if (!_check_free_space(self, size))
    {
      msg_error("No more free space exhausted in persist file");
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

  if (!_check_watermark(self) && !_grow_store(self, self->current_size + PERSIST_FILE_INITIAL_SIZE))
    {
      msg_error("Can't preallocate space for persist file",
                evt_tag_int("current", self->current_size),
                evt_tag_int("new_size", self->current_size + PERSIST_FILE_INITIAL_SIZE));
      persist_state_run_error_handler(self);
    }

  return result;
}

static PersistValueHeader *
_map_header_of_entry_from_handle(PersistState *self, PersistEntryHandle handle)
{
  PersistValueHeader *header;

  if (handle > self->current_size)
    {
      msg_error("Corrupted handle in persist_state_lookup_entry, handle value too large",
                evt_tag_printf("handle", "%08x", handle));
      return NULL;
    }
  header = (PersistValueHeader *) persist_state_map_entry(self, handle - sizeof(PersistValueHeader));
  if (GUINT32_FROM_BE(header->size) + handle > self->current_size)
    {
      msg_error("Corrupted entry header found in persist_state_lookup_entry, size too large",
                evt_tag_printf("handle", "%08x", handle),
                evt_tag_int("size", GUINT32_FROM_BE(header->size)),
                evt_tag_int("file_size", self->current_size));
      return NULL;
    }
  return header;
}

static void
_free_value(PersistState *self, PersistEntryHandle handle)
{
  if (handle)
    {
      PersistValueHeader *header;

      header = _map_header_of_entry_from_handle(self, handle);
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
persist_state_rename_entry(PersistState *self, const gchar *old_key, const gchar *new_key)
{
  PersistEntry *entry;
  gpointer old_orig_key;

  if (g_hash_table_lookup_extended(self->keys, old_key, &old_orig_key, (gpointer *)&entry))
    {
      if (g_hash_table_steal(self->keys, old_key))
        {
          g_free(old_orig_key);
          g_hash_table_insert(self->keys, g_strdup(new_key), entry);
          return TRUE;
        }
    }
  return FALSE;
}

/*
 * NOTE: can only be called from the main thread (e.g. log_pipe_init/deinit).
 */
static gboolean
_add_key(PersistState *self, const gchar *key, PersistEntryHandle handle)
{
  PersistEntry *entry;
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

      gchar *key_area = persist_state_map_entry(self, self->current_key_block);

      /* we reserve space for the next area pointer */
      sa = serialize_buffer_archive_new(key_area + self->current_key_ofs,
                                        self->current_key_size - self->current_key_ofs - chain_size);
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
              new_block = _alloc_value(self, PERSIST_STATE_KEY_BLOCK_SIZE, TRUE, 0);
              if (!new_block)
                {
                  msg_error("Unable to allocate space in the persistent file for key store");
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
                        evt_tag_str("key", key));
              persist_state_unmap_entry(self, self->current_key_block);
              return FALSE;
            }
        }
      else
        {
          const guint32 key_count = GUINT32_FROM_BE(self->header->key_count);
          self->header->key_count = GUINT32_TO_BE(key_count + 1);
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
static gboolean
_load_v23(PersistState *self, gint version, SerializeArchive *sa)
{
  gchar *key, *value;

  while (serialize_read_cstring(sa, &key, NULL))
    {
      gsize len;
      guint32 str_len;
      if (key[0] && serialize_read_cstring(sa, &value, &len))
        {
          PersistEntryHandle new_handle;

          /*  add length of the string */
          new_handle = _alloc_value(self, len + sizeof(str_len), FALSE, version);
          gchar *new_block = persist_state_map_entry(self, new_handle);

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
          _add_key(self, key, new_handle);
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

static gboolean
_load_v4(PersistState *self, gboolean load_all_entries)
{
  gint fd;
  gint64 file_size;
  gpointer map;
  gpointer key_block;
  guint32 key_size;
  PersistFileHeader *header;
  gint key_count, i;

  fd = open(self->committed_filename, O_RDONLY);
  if (fd < 0)
    {
      /* no previous data found */
      return TRUE;
    }

  file_size = lseek(fd, 0, SEEK_END);
  if (file_size > ((1LL << 31) - 1))
    {
      msg_error("Persistent file too large",
                evt_tag_str("filename", self->committed_filename),
                evt_tag_printf("size", "%" G_GINT64_FORMAT, file_size));
      close(fd);
      return FALSE;
    }
  map = mmap(NULL, file_size, PROT_READ, MAP_SHARED, fd, 0);
  close(fd);
  if (map == MAP_FAILED)
    {
      msg_error("Error mapping persistent file into memory",
                evt_tag_str("filename", self->committed_filename),
                evt_tag_error("error"));
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
              msg_error("Persistent file format error, unable to fetch key name");
              goto free_and_exit;
            }
          if (name[0])
            {
              if (serialize_read_uint32(sa, &entry_ofs))
                {
                  PersistValueHeader *value_header;
                  i++;

                  if (entry_ofs < sizeof(PersistFileHeader) || entry_ofs > file_size)
                    {
                      serialize_archive_free(sa);
                      g_free(name);
                      msg_error("Persistent file format error, entry offset is out of bounds");
                      goto free_and_exit;
                    }

                  value_header = (PersistValueHeader *) ((gchar *) map + entry_ofs - sizeof(PersistValueHeader));
                  if ((value_header->in_use) || load_all_entries)
                    {
                      gpointer new_block;
                      PersistEntryHandle new_handle;

                      new_handle = _alloc_value(self, GUINT32_FROM_BE(value_header->size), FALSE, value_header->version);
                      new_block = persist_state_map_entry(self, new_handle);
                      memcpy(new_block, value_header + 1, GUINT32_FROM_BE(value_header->size));
                      persist_state_unmap_entry(self, new_handle);
                      /* add key to the current file */
                      _add_key(self, name, new_handle);
                    }
                  g_free(name);
                }
              else
                {
                  /* bad format */
                  serialize_archive_free(sa);
                  g_free(name);
                  msg_error("Persistent file format error, unable to fetch key name");
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
                                evt_tag_int("ofs", chain_ofs));
                      serialize_archive_free(sa);
                      goto free_and_exit;
                    }
                  key_block = ((gchar *) map) + chain_ofs;
                  key_size = GUINT32_FROM_BE(*(guint32 *) (((gchar *) key_block) - sizeof(PersistValueHeader)));
                  if (chain_ofs + key_size > file_size)
                    {
                      msg_error("Persistent file format error, key block size is too large",
                                evt_tag_int("key_size", key_size));
                      serialize_archive_free(sa);
                      goto free_and_exit;
                    }
                  break;
                }
              else
                {
                  serialize_archive_free(sa);
                  msg_error("Persistent file format error, unable to fetch chained key block offset");
                  goto free_and_exit;
                }
            }
        }
      serialize_archive_free(sa);
    }
free_and_exit:
  munmap(map, file_size);
  return TRUE;
}

static gboolean
_load(PersistState *self, gboolean all_errors_are_fatal, gboolean load_all_entries)
{
  FILE *persist_file;
  gboolean success = FALSE;

  persist_file = fopen(self->committed_filename, "r");
  if (persist_file)
    {
      SerializeArchive *sa;
      gchar magic[4];
      gint version;

      sa = serialize_file_archive_new(persist_file);
      serialize_read_blob(sa, magic, 4);
      if (memcmp(magic, "SLP", 3) != 0)
        {
          msg_error("Persistent configuration file is in invalid format, ignoring");
          success = all_errors_are_fatal ? FALSE : TRUE;
          goto close_and_exit;
        }
      version = magic[3] - '0';
      if (version >= 2 && version <= 3)
        {
          success = _load_v23(self, version, sa);
        }
      else if (version == 4)
        {
          success = _load_v4(self, load_all_entries);
        }
      else
        {
          msg_error("Persistent configuration file has an unsupported major version, ignoring",
                    evt_tag_int("version", version));
          success = TRUE;
        }
close_and_exit:
      fclose(persist_file);
      serialize_archive_free(sa);
    }
  else
    {
      if (all_errors_are_fatal)
        {
          msg_error("Failed to open persist file!",
                    evt_tag_str("filename", self->committed_filename),
                    evt_tag_str("error", strerror(errno)));
          success = FALSE;
        }
      else
        success = TRUE;
    }
  return success;
}


/**
 * All persistent data accesses must be guarded by a _map and _unmap
 * call in order to get a dereferencable pointer. It is not safe to
 * save this pointer for longer terms as the underlying mapping may
 * change when the file grows.
 *
 * Threading NOTE: this can be called from any kind of threads.
 *
 * NOTE: it is not safe to keep an entry mapped while synchronizing with the
 * main thread (e.g.  mutexes, condvars, main_loop_call()), because
 * map_entry() may block the main thread in _grow_store().
 **/
gpointer
persist_state_map_entry(PersistState *self, PersistEntryHandle handle)
{
  /* we count the number of mapped entries in order to know if we're
   * safe to remap the file region */
  g_mutex_lock(self->mapped_lock);
  self->mapped_counter++;
  g_mutex_unlock(self->mapped_lock);
  return (gpointer) (((gchar *) self->current_map) + (guint32) handle);
}

/*
 * Threading NOTE: this can be called from any kind of threads.
 */
void
persist_state_unmap_entry(PersistState *self, PersistEntryHandle handle)
{
  g_mutex_lock(self->mapped_lock);
  g_assert(self->mapped_counter >= 1);
  self->mapped_counter--;
  if (self->mapped_counter == 0)
    {
      g_cond_signal(self->mapped_release_cond);
    }
  g_mutex_unlock(self->mapped_lock);
}

static PersistValueHeader *
_map_header_of_entry(PersistState *self, const gchar *persist_name, PersistEntryHandle *handle)
{
  if (!persist_state_lookup_key(self, persist_name, handle))
    return NULL;

  return _map_header_of_entry_from_handle(self, *handle);

}

PersistEntryHandle
persist_state_alloc_entry(PersistState *self, const gchar *persist_name, gsize alloc_size)
{
  PersistEntryHandle handle;

  persist_state_remove_entry(self, persist_name);

  handle = _alloc_value(self, alloc_size, TRUE, self->version);
  if (!handle)
    return 0;

  if (!_add_key(self, persist_name, handle))
    {
      _free_value(self, handle);
      return 0;
    }

  return handle;
}

PersistEntryHandle
persist_state_lookup_entry(PersistState *self, const gchar *key, gsize *size, guint8 *version)
{
  PersistEntryHandle handle;
  PersistValueHeader *header;

  header = _map_header_of_entry(self, key, &handle);
  if (header)
    {
      header->in_use = TRUE;
      *size = GUINT32_FROM_BE(header->size);
      *version = header->version;
      persist_state_unmap_entry(self, handle);
      return handle;
    }
  else
    return 0;
}

gboolean
persist_state_remove_entry(PersistState *self, const gchar *key)
{
  PersistEntryHandle handle;

  if (!persist_state_lookup_key(self, key, &handle))
    return FALSE;

  _free_value(self, handle);
  return TRUE;
}

typedef struct _PersistStateKeysForeachData
{
  PersistStateForeachFunc func;
  gpointer userdata;
  PersistState *storage;
} PersistStateKeysForeachData;

static void
_foreach_entry_func(gpointer key, gpointer value, gpointer userdata)
{
  PersistStateKeysForeachData *data = (PersistStateKeysForeachData *) userdata;
  PersistEntry *entry = (PersistEntry *) value;
  gchar *name = (gchar *) key;

  PersistValueHeader *header = persist_state_map_entry(data->storage, entry->ofs - sizeof(PersistValueHeader));
  gint size = GUINT32_FROM_BE(header->size);
  persist_state_unmap_entry(data->storage, entry->ofs);

  gpointer *state = persist_state_map_entry(data->storage, entry->ofs);
  data->func(name, size, state, data->userdata);
  persist_state_unmap_entry(data->storage, entry->ofs);
}

void
persist_state_foreach_entry(PersistState *self, PersistStateForeachFunc func, gpointer userdata)
{
  PersistStateKeysForeachData data;

  data.func = func;
  data.userdata = userdata;
  data.storage = self;

  g_hash_table_foreach(self->keys, _foreach_entry_func, &data);
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

const gchar *
persist_state_get_filename(PersistState *self)
{
  return self->committed_filename;
}

gboolean
persist_state_start_dump(PersistState *self)
{
  if (!_create_store(self))
    return FALSE;
  if (!_load(self, TRUE, TRUE))
    return FALSE;
  return TRUE;
}

gboolean
persist_state_start_edit(PersistState *self)
{
  if (!_create_store(self))
    return FALSE;
  if (!_load(self, FALSE, TRUE))
    return FALSE;
  return TRUE;
}

gboolean
persist_state_start(PersistState *self)
{
  if (!_create_store(self))
    return FALSE;
  if (!_load(self, FALSE, FALSE))
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
  if (!_commit_store(self))
    return FALSE;
  return TRUE;
}

static void
_destroy(PersistState *self)
{
  g_mutex_lock(self->mapped_lock);
  g_assert(self->mapped_counter == 0);
  g_mutex_unlock(self->mapped_lock);

  if (self->fd >= 0)
    close(self->fd);
  if (self->current_map)
    munmap(self->current_map, self->current_size);
  unlink(self->temp_filename);

  g_mutex_free(self->mapped_lock);
  g_cond_free(self->mapped_release_cond);
  g_free(self->temp_filename);
  g_free(self->committed_filename);
  g_hash_table_destroy(self->keys);
}

static void
_init(PersistState *self, gchar *committed_filename, gchar *temp_filename)
{
  self->keys = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  self->current_ofs = sizeof(PersistFileHeader);
  self->mapped_lock = g_mutex_new();
  self->mapped_release_cond = g_cond_new();
  self->version = 4;
  self->fd = -1;
  self->committed_filename = committed_filename;
  self->temp_filename = temp_filename;
}

/*
 * This routine should revert to the persist_state_new() state,
 * e.g. just like the PersistState object wasn't started yet.
 */
void
persist_state_cancel(PersistState *self)
{
  gchar *committed_filename, *temp_filename;
  committed_filename = g_strdup(self->committed_filename);
  temp_filename = g_strdup(self->temp_filename);

  _destroy(self);

  memset(self, 0, sizeof(*self));

  _init(self, committed_filename, temp_filename);
}

PersistState *
persist_state_new(const gchar *filename)
{
  PersistState *self = g_new0(PersistState, 1);

  _init(self,  g_strdup(filename), g_strdup_printf("%s-", filename));
  return self;
}

void
persist_state_free(PersistState *self)
{
  _destroy(self);
  g_free(self);
}

void
persist_state_set_global_error_handler(PersistState *self, void (*handler)(gpointer user_data), gpointer user_data)
{
  self->error_handler.handler = handler;
  self->error_handler.cookie = user_data;
}
