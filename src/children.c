#include "children.h"

typedef struct _ChildEntry
{
  pid_t pid;
  gpointer callback_data;
  void (*exit_callback)(pid_t pid, int status, gpointer user_data);
} ChildEntry;

GHashTable *child_hash;

void
child_manager_register(pid_t pid, void (*callback)(pid_t, int, gpointer), gpointer user_data)
{
  ChildEntry *ce = g_new0(ChildEntry, 1);

  ce->pid = pid;
  ce->exit_callback = callback;
  ce->callback_data = user_data;

  g_hash_table_insert(child_hash, &ce->pid, ce);
}

void
child_manager_sigchild(pid_t pid, int status)
{
  ChildEntry *ce;

  ce = g_hash_table_lookup(child_hash, &pid);
  if (ce)
    {
      ce->exit_callback(pid, status, ce->callback_data);
      g_hash_table_remove(child_hash, &pid);
      g_free(ce);
    }
}

void
child_manager_init(void)
{
  child_hash = g_hash_table_new(g_int_hash, g_int_equal);
}

void
child_manager_deinit(void)
{
  g_hash_table_destroy(child_hash);
}
