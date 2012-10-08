#include "cache.h"

struct _Cache
{
  GHashTable *hash_table;
  Provider *provider;
};

Cache *
cache_new(Provider *provider,GDestroyNotify value_destroy_func)
{
  Cache *self = g_new0(Cache,1);
  self->provider = provider;
  self->hash_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, value_destroy_func);
  return self;
}

void
cache_free(Cache *self)
{
  provider_free(self->provider);
  g_hash_table_unref(self->hash_table);
  g_free(self);
}

void *
cache_lookup(Cache *self, gchar *key)
{
  void *result = g_hash_table_lookup(self->hash_table, key);
  if (!result)
    {
      result = provider_fetch(self->provider, key);
      if (result)
        {
          g_hash_table_insert(self->hash_table, g_strdup(key), result);
        }
    }
  return result;
}
