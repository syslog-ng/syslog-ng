/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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

#include "timerwheel.h"
#include <iv_list.h>

struct _TWEntry
{
  struct iv_list_head list;
  guint64 target;
  TWCallbackFunc callback;
  gpointer user_data;
  GDestroyNotify user_data_free;
};

void
tw_entry_add(struct iv_list_head *head, TWEntry *new)
{
  iv_list_add_tail(&new->list, head);
}

void
tw_entry_unlink(TWEntry *entry)
{
  iv_list_del_init(&entry->list);
}

static void
tw_entry_free(TWEntry *entry)
{
  if (entry->user_data && entry->user_data_free)
    entry->user_data_free(entry->user_data);
  g_free(entry);
}

#if SYSLOG_NG_ENABLE_DEBUG
static void
tw_entry_list_validate(struct iv_list_head *head)
{
  struct iv_list_head *lh, *llh;

  llh = head;
  for (lh = head->next; lh != head; lh = lh->next)
    {
      g_assert(lh->prev == llh);
      llh = lh;
    }
}

#else

static inline void
tw_entry_list_validate(struct iv_list_head *head)
{
}

#endif

typedef struct _TWLevel
{
  /* mask of the bits that index the current level */
  guint64 mask;
  /* */
  guint64 slot_mask;
  guint16 num;
  guint8  shift;
  struct iv_list_head slots[0];
} TWLevel;

TWLevel *
tw_level_new(gint bits, gint shift)
{
  TWLevel *self;
  gint num;
  gint i;

  num = (1 << bits);
  self = g_malloc0(sizeof(TWLevel) + num * sizeof(self->slots[0]));
  self->shift = shift;
  self->mask = (num - 1) << shift;
  self->slot_mask = (1 << shift) - 1;
  self->num = num;
  for (i = 0; i < num; i++)
    INIT_IV_LIST_HEAD(&self->slots[i]);
  return self;
}

void
tw_level_free(TWLevel *self)
{
  gint i;

  for (i = 0; i < self->num; i++)
    {
      TWEntry *entry;
      struct iv_list_head *lh, *lh_next;

      iv_list_for_each_safe(lh, lh_next, &self->slots[i])
      {
        entry = iv_list_entry(lh, TWEntry, list);
        tw_entry_free(entry);
      }
    }
  g_free(self);
}

struct _TimerWheel
{
  TWLevel *levels[4];
  struct iv_list_head future;
  guint64 now;
  guint64 base;
  gint num_timers;
  gpointer assoc_data;
  GDestroyNotify assoc_data_free;
};

void
timer_wheel_add_timer_entry(TimerWheel *self, TWEntry *entry)
{
  guint64 level_base;
  gint slot;
  gint level_ndx;

  for (level_ndx = 0; level_ndx < G_N_ELEMENTS(self->levels); level_ndx++)
    {
      TWLevel *level = self->levels[level_ndx];

      /* level_base contains the time of associated with level->slots[0] */
      level_base = self->base & ~level->slot_mask & ~level->mask;

      if (entry->target > level_base + (level->num << level->shift))
        {
          if (entry->target < level_base + (level->num << level->shift) * 2 &&
              (self->now & level->mask) > (entry->target & level->mask))
            {
              /* target is in the next slot at the next level and
                 we've already crossed the slot in the current
                 level */
            }
          else
            {
              /* target over the current array */
              continue;
            }
        }

      slot = (entry->target & level->mask) >> level->shift;
      tw_entry_add(&level->slots[slot], entry);
      tw_entry_list_validate(&level->slots[slot]);
      break;
    }
  if (level_ndx >= G_N_ELEMENTS(self->levels))
    {
      tw_entry_add(&self->future, entry);
      tw_entry_list_validate(&self->future);
    }

}

TWEntry *
timer_wheel_add_timer(TimerWheel *self, gint timeout, TWCallbackFunc cb, gpointer user_data,
                      GDestroyNotify user_data_free)
{
  TWEntry *entry;

  entry = g_new0(TWEntry, 1);
  entry->target = self->now + timeout;
  entry->callback = cb;
  entry->user_data = user_data;
  entry->user_data_free = user_data_free;
  INIT_IV_LIST_HEAD(&entry->list);

  timer_wheel_add_timer_entry(self, entry);
  self->num_timers++;
  return entry;
}

void
timer_wheel_del_timer(TimerWheel *self, TWEntry *entry)
{
  tw_entry_unlink(entry);
  tw_entry_free(entry);
  self->num_timers--;
}

void
timer_wheel_mod_timer(TimerWheel *self, TWEntry *entry, gint new_timeout)
{
  tw_entry_unlink(entry);
  entry->target = self->now + new_timeout;
  timer_wheel_add_timer_entry(self, entry);
}

guint64
timer_wheel_get_timer_expiration(TimerWheel *self, TWEntry *entry)
{
  return entry->target;
}

static void
timer_wheel_cascade(TimerWheel *self)
{
  gint level_ndx;
  TWLevel *source_level, *target_level;
  TWEntry *entry;
  gint source_slot, target_slot;
  guint64 target_level_base;
  struct iv_list_head *lh, *lh_next, *target_head, *source_head;

  for (level_ndx = 1; level_ndx < G_N_ELEMENTS(self->levels); level_ndx++)
    {
      source_level = self->levels[level_ndx];
      target_level = self->levels[level_ndx - 1];

      source_slot = ((self->now & source_level->mask) >> source_level->shift);
      if (source_slot == source_level->num - 1)
        source_slot = 0;
      else
        source_slot++;

      source_head = &source_level->slots[source_slot];
      iv_list_for_each_safe(lh, lh_next, source_head)
      {
        entry = iv_list_entry(lh, TWEntry, list);

        target_slot = (entry->target & target_level->mask) >> target_level->shift;
        target_head = &target_level->slots[target_slot];

        tw_entry_unlink(entry);
        tw_entry_list_validate(source_head);
        tw_entry_add(target_head, entry);
        tw_entry_list_validate(target_head);
      }

      if (source_slot < source_level->num - 1)
        break;
    }

  if (level_ndx == G_N_ELEMENTS(self->levels))
    {
      target_level = self->levels[level_ndx - 1];

      source_head = &self->future;
      iv_list_for_each_safe(lh, lh_next, source_head)
      {
        entry = iv_list_entry(lh, TWEntry, list);

        target_level_base = self->base & ~target_level->slot_mask & ~target_level->mask;
        if (entry->target < target_level_base + 2 * (target_level->num << target_level->shift))
          {
            target_slot = (entry->target & target_level->mask) >> target_level->shift;
            target_head = &target_level->slots[target_slot];

            /* unlink current entry */
            tw_entry_unlink(entry);
            tw_entry_list_validate(source_head);
            tw_entry_add(target_head, entry);
            tw_entry_list_validate(target_head);
          }
      }
    }

  self->base += self->levels[0]->num;
}


/*
 * Main time adjustment function
 */
void
timer_wheel_set_time(TimerWheel *self, guint64 new_now, gpointer caller_context)
{
  /* time is not allowed to go backwards */
  if (self->now >= new_now)
    return;

  if (self->num_timers == 0)
    {
      /* if there are no timers registered with the current base, we
         can simply switch to a new one */

      self->now = new_now;
      self->base = new_now & ~self->levels[0]->mask;
      return;
    }

  for (; self->now < new_now; self->now++)
    {
      TWEntry *entry;
      struct iv_list_head *head, *lh, *lh_next;
      gint slot;
      TWLevel *level = self->levels[0];

      slot = (self->now & level->mask) >> level->shift;

      head = &self->levels[0]->slots[slot];
      iv_list_for_each_safe(lh, lh_next, head)
      {
        entry = iv_list_entry(lh, TWEntry, list);

        tw_entry_unlink(entry);
        entry->callback(self, self->now, entry->user_data, caller_context);
        tw_entry_free(entry);
        self->num_timers--;
      }

      if (self->num_timers == 0)
        {
          self->now = new_now;
          self->base = new_now & ~self->levels[0]->mask;
          break;
        }

      if (slot == level->num - 1)
        timer_wheel_cascade(self);
    }
}

guint64
timer_wheel_get_time(TimerWheel *self)
{
  return self->now;
}

void
timer_wheel_expire_all(TimerWheel *self, gpointer caller_context)
{
  guint64 now;

  now = self->now;
  timer_wheel_set_time(self, (guint64) -1, caller_context);
  self->now = now;
}

static void
_free_assoc_data(TimerWheel *self)
{
  if (self->assoc_data && self->assoc_data_free)
    self->assoc_data_free(self->assoc_data);
  self->assoc_data = NULL;
}

void
timer_wheel_set_associated_data(TimerWheel *self, gpointer assoc_data, GDestroyNotify assoc_data_free)
{
  _free_assoc_data(self);
  self->assoc_data = assoc_data;
  self->assoc_data_free = assoc_data_free;
}

gpointer
timer_wheel_get_associated_data(TimerWheel *self)
{
  return self->assoc_data;
}

TimerWheel *
timer_wheel_new(void)
{
  TimerWheel *self;
  gint bits[] = { 10, 6, 6, 6, 0 };
  gint shift = 0;
  gint i;

  self = g_new0(TimerWheel, 1);
  for (i = 0; i < G_N_ELEMENTS(self->levels); i++)
    {
      self->levels[i] = tw_level_new(bits[i], shift);
      shift += bits[i];
    }
  INIT_IV_LIST_HEAD(&self->future);
  return self;
}

void
timer_wheel_free(TimerWheel *self)
{
  gint i;
  for (i = 0; i < G_N_ELEMENTS(self->levels); i++)
    tw_level_free(self->levels[i]);
  _free_assoc_data(self);
  g_free(self);
}
