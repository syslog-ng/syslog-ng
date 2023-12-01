#include "filterx/filterx-weakrefs.h"

/*
 * Weakrefs can be used to break up circular references between objects
 * without having to do expensive loop discovery.  Although we are not
 * implementing complete weakref support, the interface mimics that, so we
 * could eventually implement it.
 *
 * Weakrefs work as follows:
 *   - each object can have a combination of hard refs and weak refs
 *   - the object can be freed when the hard references are gone
 *   - all weak refs will be cleared (e.g. set to NULL) when the object is freed
 *
 * This means that if we have circular references between a objects (not
 * necessarily two), the circle can be broken by just one weakref in the loop.
 *
 * Our weakref implementation is limited, as we don't have a GC mechanism.
 * We will destroy lingering objects at the end of our current scope and
 * weakrefs don't outlive scopes.
 *
 * With this assumption, our end-of-scope disposal of objects will work like this:
 *   - we get rid of all weakrefs
 *   - we get rid of all hardrefs
 *
 * Which should just get rid of all loops.  We currently _do not_ set the
 * weakref back to NULL at disposal, since no code is to be executed _after_
 * the disposal is done, e.g.  the invalidated pointers should not be
 * dereferenced.
 */

void
filterx_weakref_set(FilterXWeakRef *self, FilterXObject *object)
{
  FilterXScope *scope = filterx_eval_get_scope();
  if (scope)
    filterx_scope_store_weak_ref(scope, object);
  self->object = object;
}

void
filterx_weakref_clear(FilterXWeakRef *self)
{
  /* we do nothing with our ref here as that should be disposed at the end of the scope automatically */
  self->object = NULL;
}

/* NOTE: returns a hard reference if the weakref is still valid */
FilterXObject *
filterx_weakref_get(FilterXWeakRef *self)
{
  /* deref is now just returning the pointer, we don't have a means to
   * validate if it's valid, we just assume it is until our Scope  has not
   * been terminated yet */

  return filterx_object_ref(self->object);
}
