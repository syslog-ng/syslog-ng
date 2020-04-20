/*
 * Copyright (c) 2002-2010 Balabit
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

#include "syslog-ng.h"

#if SYSLOG_NG_ENABLE_MEMTRACE

#define REALLY_TRACE_MALLOC 1
#define SYSLOG_NG_ENABLE_HEAP_TRACE 1

#include <dlfcn.h>
#include <assert.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>

#define MEMTRACE_BACKTRACE_LEN 64
#define MEMTRACE_BACKTRACE_BUF_LEN (MEMTRACE_BACKTRACE_LEN * 11 + 1)

#define MEMTRACE_CANARY_SIZE 2
#define MEMTRACE_CANARY_FILL 0xcd
#define MEMTRACE_CANARY_CHECK 0xcdcdcdcd
#define MEMTRACE_CANARY_OVERHEAD sizeof(ZMemTraceCanary) * 2

typedef struct _ZMemTraceCanary
{
  gint size;
  gint neg_size;
  guint32 canary[MEMTRACE_CANARY_SIZE];
} ZMemTraceCanary;

typedef struct _ZMemTraceEntry
{
  guint32 next;
  guint32 ptr;
  guint32 size;
  gpointer backtrace[MEMTRACE_BACKTRACE_LEN];
} ZMemTraceEntry;

typedef struct _ZMemTraceHead
{
  guint32 list;
  GStaticMutex lock;
  gulong size;
} ZMemTraceHead;

#define MEMTRACE_HASH_SIZE 32768
#define MEMTRACE_HASH_MASK (32767 << 3)
#define MEMTRACE_HASH_SHIFT 3

/* at most this amount of blocks can be allocated at the same time
 * This preallocates MEMTRACE_HEAP_SIZE * sizeof(ZMemTraceEntry),
 * which is 268 bytes currently. (17MB)
 */
#define MEMTRACE_HEAP_SIZE 65536

#define MEMTRACE_TEMP_HEAP_SIZE 32768

ZMemTraceHead mem_trace_hash[MEMTRACE_HASH_SIZE];
ZMemTraceEntry mem_trace_heap[MEMTRACE_HEAP_SIZE];
guint32 mem_trace_free_list = -1;
guint32 mem_block_count = 0, mem_allocated_size = 0, mem_alloc_count = 0;
gboolean mem_trace_initialized = FALSE;
GStaticMutex mem_trace_lock = G_STATIC_MUTEX_INIT;
gint mem_trace_log_fd = -1;
gchar *mem_trace_filename = "/var/tmp/zorp-memtrace.log";
gboolean mem_trace_canaries = TRUE;

gchar temp_heap[MEMTRACE_TEMP_HEAP_SIZE];
gint temp_brk = 0;
gint mem_trace_recurse = 0;

#define TMP_ALLOCATED(ptr) (((guint32) ((char *) ptr - temp_heap)) < MEMTRACE_TEMP_HEAP_SIZE)

void *(*old_malloc)(size_t size);
void (*old_free)(void *ptr);
void *(*old_realloc)(void *ptr, size_t size);
void *(*old_calloc)(size_t nmemb, size_t size);

void
z_mem_trace_init(gchar *tracefile)
{
  int i;

  if (!mem_trace_initialized)
    {
      mem_trace_initialized = TRUE;

      for (i = 0; i < MEMTRACE_HEAP_SIZE; i++)
        {
          mem_trace_heap[i].next = i+1;
        }
      mem_trace_heap[MEMTRACE_HEAP_SIZE - 1].next = -1;
      mem_trace_free_list = 0;

      for (i = 0; i < MEMTRACE_HASH_SIZE; i++)
        {
          mem_trace_hash[i].list = -1;
          g_static_mutex_init(&mem_trace_hash[i].lock);
        }
      old_malloc = dlsym(RTLD_NEXT, "malloc");
      old_free = dlsym(RTLD_NEXT, "free");
      old_realloc = dlsym(RTLD_NEXT, "realloc");
      old_calloc = dlsym(RTLD_NEXT, "calloc");
      if (tracefile)
        {
          mem_trace_filename = g_new0(gchar, strlen("/var/tmp/") + strlen(tracefile) + 1);
          g_snprintf(mem_trace_filename,
                     strlen("/var/tmp/") + strlen(tracefile) + 1,
                     "%s%s",
                     "/var/tmp/",
                     tracefile);
        }
    }
}

static guint32
z_mem_trace_hash(guint32 ptr)
{
  return (ptr & MEMTRACE_HASH_MASK) >> MEMTRACE_HASH_SHIFT;
}

void
z_mem_trace_bt(gpointer backtrace[])
{
  /* NOTE: this is i386 specific */
  gpointer x;
  gpointer *ebp = &x+1;
  gint i = 0;

  while ((ebp > &x) && *ebp && i < MEMTRACE_BACKTRACE_LEN - 1)
    {
      gpointer value = *(ebp + 1);

      backtrace[i] = value;
      i++;
      ebp = *ebp;
    }
  backtrace[i] = NULL;
}

static char *
z_mem_trace_format_bt(gpointer backtrace[], gchar *buf, gint buflen)
{
  gchar *p = buf;
  gint i, len;

  for (i = 0; i < MEMTRACE_BACKTRACE_LEN && buflen >= 12 && backtrace[i]; i++)
    {
      len = sprintf(buf, "%p,", backtrace[i]);
      buf += len;
      buflen -= len;
    }
  return p;
}

static void
z_mem_trace_printf(char *format, ...)
{
  gchar buf[1024];
  gint len;
  va_list l;

  va_start(l, format);
  len = vsnprintf(buf, sizeof(buf), format, l);
  va_end(l);
  mem_trace_log_fd = open(mem_trace_filename, O_CREAT | O_WRONLY | O_APPEND, 0600);
  if (mem_trace_log_fd != -1)
    {
      write(mem_trace_log_fd, buf, len);
      close(mem_trace_log_fd);
    }
}

void
z_mem_trace_stats(void)
{
  z_mem_trace_printf("time: %d, allocs: %ld, blocks: %ld, size: %ld\n", time(NULL), mem_alloc_count, mem_block_count,
                     mem_allocated_size);
}


static gpointer z_mem_trace_check_canaries(gpointer ptr);

void
z_mem_trace_dump(void)
{
  int i;

  z_mem_trace_printf("memdump begins\n");
  for (i = 0; i < MEMTRACE_HASH_SIZE; i++)
    {
      ZMemTraceHead *head = &mem_trace_hash[i];
      ZMemTraceEntry *entry;
      int cur;

      g_static_mutex_lock(&head->lock);
      cur = head->list;
      while (cur != -1)
        {
          char backtrace_buf[MEMTRACE_BACKTRACE_BUF_LEN];

          entry = &mem_trace_heap[cur];

          z_mem_trace_printf("ptr=%p, size=%d, backtrace=%s\n", entry->ptr, entry->size, z_mem_trace_format_bt(entry->backtrace,
                             backtrace_buf, sizeof(backtrace_buf)));

          if (mem_trace_canaries)
            {
              z_mem_trace_check_canaries((gpointer)entry->ptr);
            }
          cur = entry->next;
        }
      g_static_mutex_unlock(&head->lock);
    }
}

/**
 * @ptr raw pointer
 * @size original size
 *
 * returns the pointer to be returned
 */
static gpointer
z_mem_trace_fill_canaries(gpointer ptr, gint size)
{
  if (!ptr)
    return ptr;
  if (mem_trace_canaries)
    {
      ZMemTraceCanary *p_before = (ZMemTraceCanary *) ptr;
      ZMemTraceCanary *p_after = (ZMemTraceCanary *)(((gchar *) ptr) + sizeof(ZMemTraceCanary) + size);

      memset(p_before->canary, MEMTRACE_CANARY_FILL, sizeof(p_before->canary));
      memset(p_after->canary, MEMTRACE_CANARY_FILL, sizeof(p_after->canary));
      p_before->size = p_after->size = size;
      p_before->neg_size = p_after->neg_size = -size;
      return (gpointer) (p_before + 1);
    }
  else
    return ptr;
}

/**
 * @ptr user pointer
 *
 * returns the pointer to be freed
 *
 * Aborts if the canaries are touched.
 *
 */
static gpointer
z_mem_trace_check_canaries(gpointer ptr)
{
  if (!ptr)
    return ptr;
  if (mem_trace_canaries)
    {
      ZMemTraceCanary *p_before = ((ZMemTraceCanary *) ptr) - 1;
      ZMemTraceCanary *p_after;
      int i;

      if (p_before->size != -p_before->neg_size)
        {
          z_mem_trace_printf("Inconsystency in canaries; ptr=%p\n", ptr);
          abort();
        }
      p_after = (ZMemTraceCanary *) (((gchar *) ptr) + p_before->size);
      if (p_after->size != p_before->size ||
          p_after->neg_size != p_before->neg_size)
        {
          z_mem_trace_printf("Inconsystency in canaries; ptr=%p\n", ptr);
          abort();
        }
      for (i = 0; i < MEMTRACE_CANARY_SIZE; i++)
        {
          if (p_before->canary[i] != p_after->canary[i] ||
              p_before->canary[i] != MEMTRACE_CANARY_CHECK)
            {
              z_mem_trace_printf("Touched canary; ptr=%p\n", ptr);
              abort();
            }
        }
      return (gpointer) p_before;
    }
  return ptr;
}

static gboolean
z_mem_trace_add(gpointer ptr, gint size, gpointer backtrace[])
{
  guint32 hash, new_ndx;
  ZMemTraceEntry *new;
  ZMemTraceHead *head;
  gchar backtrace_buf[8192];

  hash = z_mem_trace_hash((guint32) ptr);
  g_static_mutex_lock(&mem_trace_lock);
  if (mem_trace_free_list == -1)
    {
      return FALSE;
    }

  mem_block_count++;
  mem_alloc_count++;

  if ((mem_alloc_count % 1000) == 0)
    z_mem_trace_stats();

  mem_allocated_size += size;
  new_ndx = mem_trace_free_list;
  new = &mem_trace_heap[new_ndx];
  mem_trace_free_list = mem_trace_heap[mem_trace_free_list].next;

  g_static_mutex_unlock(&mem_trace_lock);

  new->ptr = (guint32) ptr;
  new->size = size;
  memmove(new->backtrace, backtrace, sizeof(new->backtrace));
  head = &mem_trace_hash[hash];

  g_static_mutex_lock(&head->lock);

  new->next = head->list;
  head->list = new_ndx;

  g_static_mutex_unlock(&head->lock);
#if REALLY_TRACE_MALLOC
  z_mem_trace_printf("memtrace addblock; ptr=%p, size=%d, backtrace=%s\n", ptr, size, z_mem_trace_format_bt(backtrace,
                     backtrace_buf, sizeof(backtrace_buf)));
#endif
  return TRUE;
}

static gboolean
z_mem_trace_del(gpointer ptr)
{
  guint32 hash, *prev, cur;
  ZMemTraceHead *head;
  ZMemTraceEntry *entry;
  gchar backtrace_buf[8192];

  hash = z_mem_trace_hash((guint32) ptr);
  head = &mem_trace_hash[hash];

  g_static_mutex_lock(&head->lock);

  prev = &head->list;
  cur = head->list;
  while (cur != -1 && mem_trace_heap[cur].ptr != (guint32) ptr)
    {
      prev = &mem_trace_heap[cur].next;
      cur = mem_trace_heap[cur].next;
    }

  if (cur == -1)
    {
      g_static_mutex_unlock(&head->lock);

      return FALSE;
    }

  *prev = mem_trace_heap[cur].next;
  g_static_mutex_unlock(&head->lock);

  g_static_mutex_lock(&mem_trace_lock);

  entry = &mem_trace_heap[cur];
#if REALLY_TRACE_MALLOC
  z_mem_trace_printf("memtrace delblock; ptr=%p, size=%d, backtrace=%s\n", (void *) entry->ptr, entry->size,
                     z_mem_trace_format_bt(entry->backtrace, backtrace_buf, sizeof(backtrace_buf)));
#endif

  mem_trace_heap[cur].next = mem_trace_free_list;
  mem_trace_free_list = cur;
  mem_block_count--;
  mem_allocated_size -= mem_trace_heap[cur].size;
  g_static_mutex_unlock(&mem_trace_lock);

  return TRUE;
}

static inline guint32
z_mem_trace_lookup_chain(gpointer ptr, ZMemTraceHead *head)
{
  guint32 cur = -1;

  cur = head->list;
  while (cur != -1 && mem_trace_heap[cur].ptr != (guint32) ptr)
    {
      cur = mem_trace_heap[cur].next;
    }

  return cur;
}

static int
z_mem_trace_getsize(gpointer ptr)
{
  guint32 hash, cur;
  int size;
  ZMemTraceHead *head;


  hash = z_mem_trace_hash((guint32) ptr);
  head = &mem_trace_hash[hash];

  g_static_mutex_lock(&head->lock);
  cur = z_mem_trace_lookup_chain(ptr, head);

  if (cur != -1)
    {
      size = mem_trace_heap[cur].size;
      g_static_mutex_unlock(&head->lock);

      return size;
    }

  g_static_mutex_unlock(&head->lock);

  return -1;
}

void *
z_malloc(size_t size, gpointer backtrace[])
{
  gpointer *raw_ptr, *user_ptr;

  raw_ptr = old_malloc(size + mem_trace_canaries * MEMTRACE_CANARY_OVERHEAD);

  user_ptr = z_mem_trace_fill_canaries(raw_ptr, size);

  if (user_ptr && !z_mem_trace_add(user_ptr, size, backtrace))
    {
      gchar buf[MEMTRACE_BACKTRACE_BUF_LEN];

      old_free(raw_ptr);
      z_mem_trace_printf("Out of free memory blocks; backtrace='%s'\n", z_mem_trace_format_bt(backtrace, buf, sizeof(buf)));
      z_mem_trace_stats();
      return NULL;
    }
  return user_ptr;
}

void
z_free(void *user_ptr, gpointer backtrace[])
{
  gchar backtrace_buf[MEMTRACE_BACKTRACE_BUF_LEN];
  gpointer raw_ptr;
  gint size;

  raw_ptr = z_mem_trace_check_canaries(user_ptr);

  size = z_mem_trace_getsize(user_ptr);
  if (size != -1)
    memset(user_ptr, 0xcd, size);
  if (user_ptr && !z_mem_trace_del(user_ptr))
    {
      z_mem_trace_printf("Trying to free a non-existing memory block; ptr=%p, backtrace='%s'\n", user_ptr,
                         z_mem_trace_format_bt(backtrace, backtrace_buf, sizeof(backtrace_buf)));
      assert(0);
    }

  if (!TMP_ALLOCATED(raw_ptr))
    old_free(raw_ptr);
}

void  *
z_realloc(void *user_ptr, size_t size, gpointer backtrace[])
{
  void *new_ptr, *raw_ptr = NULL;
  int old_size = 0;
  gchar buf[MEMTRACE_BACKTRACE_BUF_LEN];

  if (user_ptr)
    {
      raw_ptr = z_mem_trace_check_canaries(user_ptr);
      old_size = z_mem_trace_getsize(user_ptr);
      if (old_size == -1 || !z_mem_trace_del(user_ptr))
        {
          z_mem_trace_printf("Trying to realloc a non-existing memory block; ptr=%p, size='%d', info='%s'", user_ptr, size,
                             z_mem_trace_format_bt(backtrace, buf, sizeof(buf)));
          assert(0);
        }
    }
  if (TMP_ALLOCATED(raw_ptr))
    {
      /* this ptr was allocated on the temp heap, move it to real heap */

      z_mem_trace_printf("reallocing space on the temp heap, moving..., ptr=%p, temp_heap=%p, diff=%d, old_size=%d\n",
                         raw_ptr, temp_heap, (char *) raw_ptr-temp_heap, old_size);
      new_ptr = old_malloc(size + mem_trace_canaries * MEMTRACE_CANARY_OVERHEAD);
      if (new_ptr)
        {
          new_ptr = z_mem_trace_fill_canaries(new_ptr, size);
          /* copy user data */
          memmove(new_ptr, user_ptr, old_size);
        }
    }
  else
    {
      new_ptr = old_realloc(raw_ptr, size + mem_trace_canaries * MEMTRACE_CANARY_OVERHEAD);

      /* fill_canaries doesn't touch data, only fills the canary info */
      new_ptr = z_mem_trace_fill_canaries(new_ptr, size);
    }
  if (new_ptr)
    {
      z_mem_trace_add(new_ptr, size, backtrace);
    }
  return new_ptr;
}

void *
z_calloc(size_t nmemb, size_t size, gpointer backtrace[])
{
  void *user_ptr, *raw_ptr;

  if (old_calloc == NULL)
    {
      raw_ptr = &temp_heap[temp_brk];
      temp_brk += nmemb * size + mem_trace_canaries * MEMTRACE_CANARY_OVERHEAD;
      assert(temp_brk < MEMTRACE_TEMP_HEAP_SIZE);
    }
  else
    raw_ptr = old_calloc(nmemb, size + mem_trace_canaries * MEMTRACE_CANARY_OVERHEAD);

  user_ptr = z_mem_trace_fill_canaries(raw_ptr, nmemb * size);
  z_mem_trace_add(user_ptr, nmemb * size, backtrace);
  return user_ptr;
}

#undef malloc
#undef free
#undef realloc
#undef calloc

/* look up return address */

void *
malloc(size_t size)
{
  gpointer backtrace[MEMTRACE_BACKTRACE_LEN];

  z_mem_trace_bt(backtrace);
  return z_malloc(size, backtrace);
}

void
free(void *ptr)
{
  gpointer backtrace[MEMTRACE_BACKTRACE_LEN];

  z_mem_trace_bt(backtrace);
  return z_free(ptr, backtrace);
}

void *
realloc(void *ptr, size_t size)
{
  gpointer backtrace[MEMTRACE_BACKTRACE_LEN];

  z_mem_trace_bt(backtrace);

  return z_realloc(ptr, size, backtrace);
}

void *
calloc(size_t nmemb, size_t size)
{
  gpointer backtrace[MEMTRACE_BACKTRACE_LEN];

  z_mem_trace_bt(backtrace);
  return z_calloc(nmemb, size, backtrace);
}

#else

void
z_mem_trace_init(const gchar *memtrace_file)
{
}

void
z_mem_trace_stats(void)
{
}

void
z_mem_trace_dump(void)
{
}

#endif
