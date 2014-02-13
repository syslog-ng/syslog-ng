/*
 * Copyright (c) 2013 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2013 Gergely Nagy <algernon@balabit.hu>
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

#include "cfg-tree.h"
#include "testutils.h"
#include "apphook.h"
#include "logpipe.h"


/*
 * The Always Pipe. Always returns the same thing at init time.
 */

typedef struct
{
  LogPipe super;

  gboolean return_value;
  gboolean init_called;
  gboolean deinit_called;
} AlmightyAlwaysPipe;

static gboolean
almighty_always_pipe_init (LogPipe *s)
{
  AlmightyAlwaysPipe *self = (AlmightyAlwaysPipe *)s;

  self->init_called = TRUE;
  return self->return_value;
}

static gboolean
almighty_always_pipe_deinit (LogPipe *s)
{
  AlmightyAlwaysPipe *self = (AlmightyAlwaysPipe *)s;

  self->deinit_called = TRUE;
  return TRUE;
}

static LogPipe *
almighty_always_pipe_new (gboolean return_value)
{
  AlmightyAlwaysPipe *self;

  self = g_new0 (AlmightyAlwaysPipe, 1);

  log_pipe_init_instance (&self->super, NULL);

  self->super.init = almighty_always_pipe_init;
  self->super.deinit = almighty_always_pipe_deinit;

  self->return_value = return_value;

  return &self->super;
}


/*
 * Helper functions
 */

static AlmightyAlwaysPipe *
create_and_attach_almighty_pipe (CfgTree *tree, gboolean pipe_value)
{
  AlmightyAlwaysPipe *pipe;

  pipe = (AlmightyAlwaysPipe *)almighty_always_pipe_new (pipe_value);
  cfg_tree_add_object (tree, log_expr_node_new_pipe (&pipe->super, NULL));

  return pipe;
}

static void
assert_almighty_always_pipe (gboolean always_pipe_value,
                             gboolean tree_start_expected,
                             gboolean tree_stop_expected,
                             gboolean was_init_called,
                             gboolean was_deinit_called)
{
  CfgTree tree;
  AlmightyAlwaysPipe *pipe;

  cfg_tree_init_instance (&tree, NULL);

  pipe = create_and_attach_almighty_pipe (&tree, always_pipe_value);

  assert_gboolean (cfg_tree_start (&tree), tree_start_expected,
                   "cfg_tree_start() did not return the expected value");
  assert_gboolean (cfg_tree_stop (&tree), tree_stop_expected,
                   "cfg_tree_stop() did not return the epxected value");

  assert_gboolean (pipe->init_called, was_init_called,
                   "->init was called state");
  assert_gboolean (pipe->deinit_called, was_deinit_called,
                   "->deinit was called state");

  cfg_tree_free_instance (&tree);
}


/*
 * Test cases
 */

static void
test_pipe_init_success (void)
{
  testcase_begin ("A pipe that always succeeds, results in a startable tree");

  assert_almighty_always_pipe (TRUE, TRUE, TRUE, TRUE, TRUE);

  testcase_end ();
}

static void
test_pipe_init_fail (void)
{
  testcase_begin ("A pipe that always fails, results in an unstartable tree");

  assert_almighty_always_pipe (FALSE, FALSE, TRUE, TRUE, FALSE);

  testcase_end ();
}

static void
test_pipe_init_multi_success (void)
{
  CfgTree tree;

  testcase_begin ("A pipe of all good nodes will start & stop properly");

  cfg_tree_init_instance (&tree, NULL);

  create_and_attach_almighty_pipe (&tree, TRUE);
  create_and_attach_almighty_pipe (&tree, TRUE);
  create_and_attach_almighty_pipe (&tree, TRUE);

  assert_true (cfg_tree_start (&tree),
               "Starting a tree of all-good nodes works");
  assert_true (cfg_tree_stop (&tree),
               "Stopping a tree of all-good nodes works");

  cfg_tree_free_instance (&tree);

  testcase_end ();
}

static void
test_pipe_init_multi_with_bad_node (void)
{
  AlmightyAlwaysPipe *pipe1, *pipe2, *pipe3;
  CfgTree tree;

  testcase_begin ("A pipe of some bad nodes will not start, but cleans up nicely.");

  cfg_tree_init_instance (&tree, NULL);

  pipe1 = create_and_attach_almighty_pipe (&tree, TRUE);
  pipe2 = create_and_attach_almighty_pipe (&tree, FALSE);
  pipe3 = create_and_attach_almighty_pipe (&tree, TRUE);

  assert_false (cfg_tree_start (&tree),
               "Starting a tree of all-good nodes works");
  assert_true (cfg_tree_stop (&tree),
               "Stopping a tree of all-good nodes works");

  assert_true (pipe1->init_called,
               "The initializer of the first good pipe is called");
  assert_true (pipe2->init_called,
               "The initializer of the bad pipe is called");
  assert_false (pipe3->init_called,
                "The initializer of the second good pipe is NOT called");

  assert_true (pipe1->deinit_called,
               "The deinitializer of the first good pipe is called");
  assert_false (pipe2->deinit_called,
               "The deinitializer of the bad pipe is NOT called");
  assert_false (pipe3->deinit_called,
               "The deinitializer of the second good pipe is NOT called");

  cfg_tree_free_instance (&tree);

  testcase_end ();
}


/*
 * The main program.
 */

int
main (void)
{
  app_startup ();

  test_pipe_init_success ();
  test_pipe_init_fail ();
  test_pipe_init_multi_success ();
  test_pipe_init_multi_with_bad_node ();

  app_shutdown ();

  return 0;
}
