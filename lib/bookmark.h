#ifndef BOOKMARK_H_INCLUDED
#define BOOKMARK_H_INCLUDED

#include "persist-state.h"

//TODO: rename MaxBookmarkContainerSize
#define MAX_STATE_DATA_LENGTH 128

typedef struct _BookmarkContainer
{
  char other_state[MAX_STATE_DATA_LENGTH];
} BookmarkContainer;

typedef struct _Bookmark
{
  BookmarkContainer container;
  void (*save)(struct _Bookmark *self);
  void (*destroy)(struct _Bookmark *self);
} Bookmark;

static inline void
bookmark_init(Bookmark *self)
{
  self->save = NULL;
}

#endif
