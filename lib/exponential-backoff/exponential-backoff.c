/*
 * Copyright (c) 2024 Attila Szakacs
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

#include "exponential-backoff.h"
#include "messages.h"

gboolean
exponential_backoff_options_validate(const ExponentialBackoffOptions *self)
{
  if (self->initial_seconds < 0)
    {
      msg_error("Cannot initialize exponential backoff with negative initial seconds",
                evt_tag_printf("initial_seconds", "%.2f", self->initial_seconds));
      return FALSE;
    }

  if (self->maximum_seconds < 0)
    {
      msg_error("Cannot initialize exponential backoff with negative maximum seconds",
                evt_tag_printf("maximum_seconds", "%.2f", self->maximum_seconds));
      return FALSE;
    }

  if (self->initial_seconds > self->maximum_seconds)
    {
      msg_error("Cannot initialize exponential backoff with larger initial seconds than maximal seconds",
                evt_tag_printf("initial_seconds", "%.2f", self->initial_seconds),
                evt_tag_printf("maximum_seconds", "%.2f", self->maximum_seconds));
      return FALSE;
    }

  if (self->multiplier < 1)
    {
      msg_error("Cannot initialize exponential backoff with a multiplier smaller than 1",
                evt_tag_printf("multiplier", "%.2f", self->multiplier));
      return FALSE;
    }

  return TRUE;
}


struct ExponentialBackoff_
{
  ExponentialBackoffOptions options;
  gdouble next_wait_seconds;
};

gdouble
exponential_backoff_peek_next_wait_seconds(ExponentialBackoff *self)
{
  return self->next_wait_seconds;
}

gdouble
exponential_backoff_get_next_wait_seconds(ExponentialBackoff *self)
{
  gdouble wait_seconds = self->next_wait_seconds;

  self->next_wait_seconds = MAX(MIN(wait_seconds * self->options.multiplier, self->options.maximum_seconds),
                                self->options.initial_seconds);

  return wait_seconds;
}

void
exponential_backoff_reset(ExponentialBackoff *self)
{
  self->next_wait_seconds = 0;
}

ExponentialBackoffOptions *
exponential_backoff_get_options(ExponentialBackoff *self)
{
  return &self->options;
}

gboolean
exponential_backoff_validate_options(ExponentialBackoff *self)
{
  return exponential_backoff_options_validate(&self->options);
}

void
exponential_backoff_free(ExponentialBackoff *self)
{
  g_free(self);
}

ExponentialBackoff *
exponential_backoff_new(ExponentialBackoffOptions options)
{
  if (!exponential_backoff_options_validate(&options))
    return NULL;

  ExponentialBackoff *self = g_new0(ExponentialBackoff, 1);
  self->options = options;

  return self;
}

ExponentialBackoff *
exponential_backoff_new_default(void)
{
  ExponentialBackoff *self = g_new0(ExponentialBackoff, 1);
  exponential_backoff_options_set_defaults(&self->options);

  return self;
}
