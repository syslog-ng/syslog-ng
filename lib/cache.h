#include <glib.h>

typedef struct _Provider Provider;
typedef struct _Cache Cache;

struct _Provider
{
  void *(*fetch)(Provider *self, gchar *key);
  void (*free_fn)(Provider *self);
};

static inline void*
provider_fetch(Provider *self, gchar *key)
{
  if (self->fetch)
    {
      return self->fetch(self, key);
    }
  return NULL;
}

static inline void
provider_free(Provider *self)
{
  if (self->free_fn)
    {
      self->free_fn(self);
    }
  g_free(self);
}

Cache *cache_new(Provider *provider, GDestroyNotify value_destroy_func);
void *cache_lookup(Cache *self, gchar *key);
void cache_free(Cache *self);
