#ifndef BOOKMARK_H_INCLUDED
#define BOOKMARK_H_INCLUDED

#include "persist-state.h"

//TODO: rename MaxBookmarkContainerSize
#define MAX_STATE_DATA_LENGTH (128-4*sizeof(void *)) /* because the Bookmark structure should be aligned on ia64 (ie. HPUX-11v2) */

typedef struct _BookmarkContainer
{
  char other_state[MAX_STATE_DATA_LENGTH];
} BookmarkContainer;

typedef struct _Bookmark
{
  PersistState *persist_state;
  void (*save)(struct _Bookmark *self);
  void (*destroy)(struct _Bookmark *self);

  void *padding;

  BookmarkContainer container;
  /* take care of the size of the structure because of the required alignment on some platforms (see comment above) */
} Bookmark;

static inline void
bookmark_init(Bookmark *self)
{
  self->save = NULL;
}

#endif
