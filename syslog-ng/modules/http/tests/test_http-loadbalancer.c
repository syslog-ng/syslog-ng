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

#include <criterion/criterion.h>
#include <unistd.h>
#include "http-loadbalancer.h"

#define NUM_TARGETS 5
#define NUM_CLIENTS 16


static HTTPLoadBalancer *
_construct_load_balancer(void)
{
  HTTPLoadBalancer *lb;

  lb = http_load_balancer_new();
  for (gint i = 0; i < NUM_TARGETS; i++)
    {
      gchar url[256];
      g_snprintf(url, sizeof(url), "http://localhost:%d", 8000 + i);
      http_load_balancer_add_target(lb, url);
    }
  return lb;
}

static void
_setup_lb_clients(HTTPLoadBalancer *lb, HTTPLoadBalancerClient *lbc, gint num)
{
  for (gint i = 0; i < num; i++)
    http_lb_client_init(&lbc[i], lb);
}

static void
_teardown_lb_clients(HTTPLoadBalancer *lb, HTTPLoadBalancerClient *lbc, gint num)
{
  for (gint i = 0; i < num; i++)
    http_lb_client_deinit(&lbc[i]);
}

Test(http_loadbalancer, construct_and_free)
{
  HTTPLoadBalancer *lb = _construct_load_balancer();
  http_load_balancer_free(lb);
}

Test(http_loadbalancer, target_index_is_set_as_urls_are_added_to_the_array_index)
{
  HTTPLoadBalancer *lb = _construct_load_balancer();

  for (gint i = 0; i < lb->num_targets; i++)
    cr_assert(lb->targets[i].index == i);
  http_load_balancer_free(lb);
}

Test(http_loadbalancer, number_of_clients_is_tracked_in_num_clients)
{
  HTTPLoadBalancer *lb = _construct_load_balancer();
  HTTPLoadBalancerClient lbc[NUM_CLIENTS];

  for (gint i = 0; i < G_N_ELEMENTS(lbc); i++)
    {
      http_lb_client_init(&lbc[i], lb);
      cr_assert(lb->num_clients == i + 1);
    }
  http_load_balancer_free(lb);
}

Test(http_loadbalancer, choose_target_selects_the_first_operational_target)
{
  HTTPLoadBalancer *lb = _construct_load_balancer();
  HTTPLoadBalancerClient lbc;

  http_lb_client_init(&lbc, lb);

  HTTPLoadBalancerTarget *target = http_load_balancer_choose_target(lb, &lbc);
  cr_assert(target->url, "http://localhost:8000");
  cr_assert(target->state == HTTP_TARGET_OPERATIONAL);

  http_lb_client_deinit(&lbc);
  http_load_balancer_free(lb);
}

Test(http_loadbalancer, choose_target_balances_clients_to_targets)
{
  HTTPLoadBalancer *lb = _construct_load_balancer();
  HTTPLoadBalancerClient lbc[NUM_CLIENTS];
  gint target_counts[NUM_TARGETS] = {0};

  _setup_lb_clients(lb, lbc, G_N_ELEMENTS(lbc));

  /* count number of workers on each target */
  for (gint i = 0; i < G_N_ELEMENTS(lbc); i++)
    {
      HTTPLoadBalancerTarget *target = http_load_balancer_choose_target(lb, &lbc[i]);

      target_counts[target->index]++;
    }

  for (gint i = 0; i < NUM_TARGETS; i++)
    {
      gint expected_number_of_workers = NUM_CLIENTS / NUM_TARGETS;

      /* we might have one more client, if the number of workers is not
       * divisible by the number of targets. The load balancer allocates
       * the first couple of targets for the excess.
       */

      cr_expect(target_counts[i] - expected_number_of_workers <= 1 &&
                target_counts[i] - expected_number_of_workers >= 0,
                "The target %d is not balanced, expected_number_of_workers=%d, actual=%d",
                i, expected_number_of_workers, target_counts[i]);
    }

  _teardown_lb_clients(lb, lbc, G_N_ELEMENTS(lbc));
  http_load_balancer_free(lb);
}

Test(http_loadbalancer, choose_target_tries_to_stay_affine_to_the_current_target)
{
  HTTPLoadBalancer *lb = _construct_load_balancer();
  HTTPLoadBalancerClient lbc[NUM_CLIENTS];

  _setup_lb_clients(lb, lbc, G_N_ELEMENTS(lbc));

  for (gint i = 0; i < G_N_ELEMENTS(lbc); i++)
    {
      HTTPLoadBalancerTarget *initial_target = http_load_balancer_choose_target(lb, &lbc[i]);
      for (gint n = 0; n < 100; n++)
        {
          HTTPLoadBalancerTarget *target = http_load_balancer_choose_target(lb, &lbc[i]);
          cr_assert(initial_target == target);
        }
    }

  _teardown_lb_clients(lb, lbc, G_N_ELEMENTS(lbc));
  http_load_balancer_free(lb);
}

static inline gboolean
_should_fail_this_target(HTTPLoadBalancerTarget *target)
{
  return (target->index % 2) != 0;
}

Test(http_loadbalancer, failed_target_is_taken_out_of_rotation)
{
  HTTPLoadBalancer *lb = _construct_load_balancer();
  HTTPLoadBalancerClient lbc[NUM_CLIENTS];
  gint target_counts[NUM_TARGETS] = {0};
  gint failing_targets = 0;

  _setup_lb_clients(lb, lbc, G_N_ELEMENTS(lbc));

  for (gint i = 0; i < G_N_ELEMENTS(lbc); i++)
    {
      HTTPLoadBalancerTarget *target = http_load_balancer_choose_target(lb, &lbc[i]);

      cr_assert(target != NULL);
      /* fail every second */
      if (_should_fail_this_target(target))
        {
          http_load_balancer_set_target_failed(lb, target);
          failing_targets++;
        }
      else
        http_load_balancer_set_target_successful(lb, target);
    }

  /* check that our lbc's still get operational targets */
  for (gint i = 0; i < G_N_ELEMENTS(lbc); i++)
    {
      HTTPLoadBalancerTarget *target = http_load_balancer_choose_target(lb, &lbc[i]);

      cr_assert(_should_fail_this_target(target) == FALSE,
                "HTTPLoadBalancer returned a target that was marked as failed, index=%d",
                target->index);
      cr_assert(target->state == HTTP_TARGET_OPERATIONAL);
      target_counts[target->index]++;
    }

  /* check that we balance the load on the remaining targets properly */
  for (gint i = 0; i < NUM_TARGETS; i++)
    {
      if (_should_fail_this_target(&lb->targets[i]))
        {
          cr_expect(lb->targets[i].state == HTTP_TARGET_FAILED);
        }
      else
        {
          gint expected_number_of_workers = NUM_CLIENTS / (NUM_TARGETS - failing_targets);
          cr_expect(target_counts[i] - expected_number_of_workers <= 1 &&
                    target_counts[i] - expected_number_of_workers >= 0,
                    "The target %d is not balanced, expected_number_of_workers=%d, actual=%d",
                    i, expected_number_of_workers, target_counts[i]);
        }
    }

  _teardown_lb_clients(lb, lbc, G_N_ELEMENTS(lbc));
  http_load_balancer_free(lb);
}

Test(http_loadbalancer, number_of_failed_targets_is_tracked_even_if_the_same_target_is_failed_multiple_times)
{
  HTTPLoadBalancer *lb = _construct_load_balancer();
  HTTPLoadBalancerClient lbc;

  _setup_lb_clients(lb, &lbc, 1);

  /* make all targets to the failed state */
  for (gint i = 0; i < NUM_TARGETS; i++)
    {
      HTTPLoadBalancerTarget *target = &lb->targets[i];

      http_load_balancer_set_target_failed(lb, target);
      cr_assert(lb->num_failed_targets == i + 1);
      http_load_balancer_set_target_failed(lb, target);
      cr_assert(lb->num_failed_targets == i + 1);
      http_load_balancer_set_target_successful(lb, target);
      cr_assert(lb->num_failed_targets == i);
      http_load_balancer_set_target_successful(lb, target);
      cr_assert(lb->num_failed_targets == i);
      http_load_balancer_set_target_failed(lb, target);
      cr_assert(lb->num_failed_targets == i + 1);
    }

  _teardown_lb_clients(lb, &lbc, 1);
  http_load_balancer_free(lb);
}

Test(http_loadbalancer, if_all_targets_fail_the_least_recently_failed_one_is_tried)
{
  HTTPLoadBalancer *lb = _construct_load_balancer();
  HTTPLoadBalancerClient lbc;

  _setup_lb_clients(lb, &lbc, 1);

  /* make all targets to the failed state */
  for (gint i = NUM_TARGETS - 1; i >= 0; i--)
    {
      HTTPLoadBalancerTarget *target = &lb->targets[i];

      http_load_balancer_set_target_failed(lb, target);
      sleep(1);
    }

  /* check that we get the least recently failed target */
  HTTPLoadBalancerTarget *target = http_load_balancer_choose_target(lb, &lbc);

  cr_assert(target->state != HTTP_TARGET_OPERATIONAL);
  cr_assert(target->index == NUM_TARGETS - 1);
  http_load_balancer_set_target_failed(lb, target);

  target = http_load_balancer_choose_target(lb, &lbc);

  cr_assert(target->state != HTTP_TARGET_OPERATIONAL);
  cr_assert(target->index == NUM_TARGETS - 2);
  http_load_balancer_set_target_failed(lb, target);

  target = http_load_balancer_choose_target(lb, &lbc);

  cr_assert(target->state != HTTP_TARGET_OPERATIONAL);
  cr_assert(target->index == NUM_TARGETS - 3);

  _teardown_lb_clients(lb, &lbc, 1);
  http_load_balancer_free(lb);
}

Test(http_loadbalancer, failed_servers_are_reattempted_after_recovery_time)
{
  HTTPLoadBalancer *lb = _construct_load_balancer();
  HTTPLoadBalancerClient lbc[NUM_CLIENTS];
  HTTPLoadBalancerTarget *target;

  http_load_balancer_set_recovery_timeout(lb, 1);
  _setup_lb_clients(lb, lbc, G_N_ELEMENTS(lbc));

  http_load_balancer_set_target_failed(lb, &lb->targets[0]);

  for (gint i = 0; i < G_N_ELEMENTS(lbc); i++)
    {
      target = http_load_balancer_choose_target(lb, &lbc[i]);
      cr_assert(target->state == HTTP_TARGET_OPERATIONAL,
                "As it seems the test lost its race against the load"
                "balancer's 1 seconds recovery timeout.  You can always bump"
                "the timeout a bit higher, but hey this loop is 16 iterations"
                "long, that should be doable in 1 second.");
    }
  sleep(1);
  target = http_load_balancer_choose_target(lb, &lbc[0]);
  cr_assert(target->state == HTTP_TARGET_FAILED);

  _teardown_lb_clients(lb, lbc, G_N_ELEMENTS(lbc));
  http_load_balancer_free(lb);
}

Test(http_loadbalancer, drop_targets_resets_the_target_list)
{
  HTTPLoadBalancer *lb = _construct_load_balancer();

  cr_assert(lb->num_targets != 0);
  http_load_balancer_drop_all_targets(lb);
  cr_assert(lb->num_targets == 0);
  http_load_balancer_free(lb);
}

void
setup(void)
{
}

void
teardown(void)
{
}

TestSuite(http_loadbalancer, .init = setup, .fini = teardown);
