/*
 * Copyright (c) 2018 Balazs Scheidler
 * Copyright (c) 2018 Balabit
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

#include "http-loadbalancer.h"
#include "messages.h"
#include <string.h>

/* HTTPLoadBalancerTarget */

void
http_lb_target_init(HTTPLoadBalancerTarget *self, const gchar *url, gint index_)
{
  memset(self, 0, sizeof(*self));

  self->url = g_strdup(url);
  self->state = HTTP_TARGET_OPERATIONAL;
  self->index = index_;
}

void
http_lb_target_deinit(HTTPLoadBalancerTarget *self)
{
  g_free(self->url);
}

/* HTTPLoadBalancerClient */

void
http_lb_client_init(HTTPLoadBalancerClient *self, HTTPLoadBalancer *lb)
{
  memset(self, 0, sizeof(*self));
  http_load_balancer_track_client(lb, self);
}

void
http_lb_client_deinit(HTTPLoadBalancerClient *self)
{
}

/* HTTPLoadBalancer */

static void
_recalculate_clients_per_target_goals(HTTPLoadBalancer *self)
{
  gint num_operational_targets = self->num_targets - self->num_failed_targets;

  if (num_operational_targets == 0)
    return;

  /* spread clients evenly */
  gint clients_per_target = self->num_clients / num_operational_targets;
  gint remainder = self->num_clients % num_operational_targets;

  /* assign the new targets */
  for (gint i = 0; i < self->num_targets; i++)
    {
      HTTPLoadBalancerTarget *target = &self->targets[i];

      if (target->state != HTTP_TARGET_OPERATIONAL)
        continue;

      target->max_clients = clients_per_target;
      if (remainder > 0)
        {
          target->max_clients++;
          remainder--;
        }
      msg_trace("Setting maximum number of workers for HTTP destination",
                evt_tag_str("url", target->url),
                evt_tag_int("max_clients", target->max_clients));
    }
}

static void
_switch_target(HTTPLoadBalancer *self, HTTPLoadBalancerClient *lbc, HTTPLoadBalancerTarget *new_target)
{
  /* self->lock must be held */
  if (lbc->target != new_target)
    {
      if (lbc->target)
        lbc->target->number_of_clients--;

      new_target->number_of_clients++;
      lbc->target = new_target;
    }
}

static HTTPLoadBalancerTarget *
_get_least_recently_tried_failing_target(HTTPLoadBalancer *self)
{
  time_t lru = 0;
  gint lru_index = -1;

  for (gint i = 0; i < self->num_targets; i++)
    {
      HTTPLoadBalancerTarget *target = &self->targets[i];

      if (target->state != HTTP_TARGET_FAILED)
        continue;

      if (lru_index < 0 || lru > target->last_failure_time)
        {
          lru = target->last_failure_time;
          lru_index = i;
        }
    }
  return &self->targets[lru_index >= 0 ? lru_index : 0];
}

static HTTPLoadBalancerTarget *
_recover_a_failed_target(HTTPLoadBalancer *self)
{
  self->last_recovery_attempt = time(NULL);
  /* allocation failed, try the target that failed the least recently. */
  return _get_least_recently_tried_failing_target(self);
}

static gboolean
_check_recovery(HTTPLoadBalancer *self, HTTPLoadBalancerClient *lbc, HTTPLoadBalancerTarget **new_target)
{
  if (self->num_failed_targets > 0)
    {
      time_t now = time(NULL);

      if (self->last_recovery_attempt == 0)
        self->last_recovery_attempt = now;

      if (now - self->last_recovery_attempt >= self->recovery_timeout)
        {
          *new_target = _recover_a_failed_target(self);
          return TRUE;
        }
    }
  return FALSE;
}

static HTTPLoadBalancerTarget *
_locate_target(HTTPLoadBalancer *self, HTTPLoadBalancerClient *lbc)
{
  gint start_index = lbc->target
                     ? (lbc->target->index + 1) % self->num_targets
                     : 0;
  for (gint i = 0; i < self->num_targets; i++)
    {
      HTTPLoadBalancerTarget *target = &self->targets[(i + start_index) % self->num_targets];

      if (target->state == HTTP_TARGET_OPERATIONAL &&
          target->number_of_clients < target->max_clients)
        return target;
    }

  return _recover_a_failed_target(self);
}

static gboolean
_check_rebalance(HTTPLoadBalancer *self, HTTPLoadBalancerClient *lbc, HTTPLoadBalancerTarget **new_target)
{
  /* Are we misbalanced? */
  if (lbc->target == NULL ||
      lbc->target->state != HTTP_TARGET_OPERATIONAL ||
      lbc->target->number_of_clients > lbc->target->max_clients)
    {
      *new_target = _locate_target(self, lbc);
      return TRUE;
    }
  return FALSE;
}

HTTPLoadBalancerTarget *
http_load_balancer_choose_target(HTTPLoadBalancer *self, HTTPLoadBalancerClient *lbc)
{
  HTTPLoadBalancerTarget *new_target;

  g_static_mutex_lock(&self->lock);

  new_target = lbc->target;
  if (_check_recovery(self, lbc, &new_target) ||
      _check_rebalance(self, lbc, &new_target))
    _switch_target(self, lbc, new_target);

  g_static_mutex_unlock(&self->lock);
  return lbc->target;
}

void
http_load_balancer_add_target(HTTPLoadBalancer *self, const gchar *url)
{
  gint n = self->num_targets++;

  self->targets = g_renew(HTTPLoadBalancerTarget, self->targets, self->num_targets);
  http_lb_target_init(&self->targets[n], url, n);
}

void
http_load_balancer_drop_all_targets(HTTPLoadBalancer *self)
{
  for (int i = 0; i < self->num_targets; i++)
    http_lb_target_deinit(&self->targets[i]);
  self->num_targets = 0;
}

void
http_load_balancer_track_client(HTTPLoadBalancer *self, HTTPLoadBalancerClient *lbc)
{
  g_static_mutex_lock(&self->lock);
  self->num_clients++;
  _recalculate_clients_per_target_goals(self);
  g_static_mutex_unlock(&self->lock);
}

void
http_load_balancer_set_target_failed(HTTPLoadBalancer *self, HTTPLoadBalancerTarget *target)
{
  g_static_mutex_lock(&self->lock);
  if (target->state != HTTP_TARGET_FAILED)
    {
      msg_debug("Load balancer target failed, removing from rotation",
                evt_tag_str("url", target->url));
      self->num_failed_targets++;
      target->state = HTTP_TARGET_FAILED;
      _recalculate_clients_per_target_goals(self);
    }
  target->last_failure_time = time(NULL);
  g_static_mutex_unlock(&self->lock);
}

void
http_load_balancer_set_target_successful(HTTPLoadBalancer *self, HTTPLoadBalancerTarget *target)
{
  g_static_mutex_lock(&self->lock);
  if (target->state != HTTP_TARGET_OPERATIONAL)
    {
      msg_debug("Load balancer target recovered, adding back to rotation",
                evt_tag_str("url", target->url));
      self->num_failed_targets--;
      target->state = HTTP_TARGET_OPERATIONAL;
      _recalculate_clients_per_target_goals(self);
    }
  g_static_mutex_unlock(&self->lock);
}

void
http_load_balancer_set_recovery_timeout(HTTPLoadBalancer *self, gint recovery_timeout)
{
  self->recovery_timeout = recovery_timeout;
}

HTTPLoadBalancer *
http_load_balancer_new(void)
{
  HTTPLoadBalancer *self = g_new0(HTTPLoadBalancer, 1);

  g_static_mutex_init(&self->lock);
  self->recovery_timeout = 60;
  return self;
}

void
http_load_balancer_free(HTTPLoadBalancer *self)
{
  http_load_balancer_drop_all_targets(self);
  g_free(self->targets);
  g_static_mutex_free(&self->lock);
  g_free(self);
}
