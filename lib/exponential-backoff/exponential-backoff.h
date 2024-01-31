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

#ifndef EXPONENTIAL_BACKOFF_H
#define EXPONENTIAL_BACKOFF_H

#include "syslog-ng.h"

#define EXPONENTIAL_BACKOFF_OPTIONS_INITIAL_SECONDS_DEFAULT (0.1)
#define EXPONENTIAL_BACKOFF_OPTIONS_MAXIMUM_SECONDS_DEFAULT (1)
#define EXPONENTIAL_BACKOFF_OPTIONS_MULTIPLIER_DEFAULT (2)

typedef struct ExponentialBackoffOptions_
{
  gdouble initial_seconds;
  gdouble maximum_seconds;
  gdouble multiplier;
} ExponentialBackoffOptions;

gboolean exponential_backoff_options_validate(const ExponentialBackoffOptions *self);

static inline void
exponential_backoff_options_set_defaults(ExponentialBackoffOptions *self)
{
  self->initial_seconds = EXPONENTIAL_BACKOFF_OPTIONS_INITIAL_SECONDS_DEFAULT;
  self->maximum_seconds = EXPONENTIAL_BACKOFF_OPTIONS_MAXIMUM_SECONDS_DEFAULT;
  self->multiplier = EXPONENTIAL_BACKOFF_OPTIONS_MULTIPLIER_DEFAULT;
}

static inline void
exponential_backoff_options_set_initial_seconds(ExponentialBackoffOptions *self, gdouble initial_seconds)
{
  self->initial_seconds = initial_seconds;
}

static inline void
exponential_backoff_options_set_maximum_seconds(ExponentialBackoffOptions *self, gdouble maximum_seconds)
{
  self->maximum_seconds = maximum_seconds;
}

static inline void
exponential_backoff_options_set_multiplier(ExponentialBackoffOptions *self, gdouble multiplier)
{
  self->multiplier = multiplier;
}


typedef struct ExponentialBackoff_ ExponentialBackoff;

ExponentialBackoff *exponential_backoff_new(ExponentialBackoffOptions options);
ExponentialBackoff *exponential_backoff_new_default(void);
void exponential_backoff_free(ExponentialBackoff *self);

ExponentialBackoffOptions *exponential_backoff_get_options(ExponentialBackoff *self);
gboolean exponential_backoff_validate_options(ExponentialBackoff *self);

gdouble exponential_backoff_peek_next_wait_seconds(ExponentialBackoff *self);
gdouble exponential_backoff_get_next_wait_seconds(ExponentialBackoff *self);
void exponential_backoff_reset(ExponentialBackoff *self);

#endif
