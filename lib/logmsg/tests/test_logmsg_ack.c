/*
 * Copyright (c) 2002-2016 Balabit
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
#include "logmsg/logmsg.h"
#include "apphook.h"
#include "logpipe.h"
#include <criterion/criterion.h>
#include <criterion/parameterized.h>

typedef struct _TestAckRecord TestAckRecord;

struct _TestAckRecord
{
  AckRecord super;
  LogMessage *original;
  LogPathOptions path_options;
  gboolean acked;
  void (*init)(TestAckRecord *);
  void (*deinit)(TestAckRecord *);
  void (*ack_message)(LogMessage *lm, AckType ack_type);
};

static void
_init(TestAckRecord *self)
{
  self->acked = FALSE;
  log_msg_ref(self->original);

  log_msg_refcache_start_producer(self->original);
  log_msg_add_ack(self->original, &self->path_options);
  log_msg_ref(self->original);
  log_msg_write_protect(self->original);
}

static void
_deinit(TestAckRecord *self)
{
  log_msg_drop(self->original, &self->path_options, AT_PROCESSED);
  log_msg_refcache_stop();
}

static void
_ack_message(LogMessage *msg, AckType type)
{
  TestAckRecord *self = (TestAckRecord *)msg->ack_record;
  self->acked = TRUE;
}

static void
ack_record_free(TestAckRecord *self)
{
  log_msg_unref(self->original);
  g_free(self);
}

static TestAckRecord *
ack_record_new(void)
{
  TestAckRecord *self = g_new0(TestAckRecord, 1);
  self->init = _init;
  self->deinit = _deinit;
  self->ack_message = _ack_message;
  self->original = log_msg_new_empty();
  self->original->ack_func = self->ack_message;
  self->original->ack_record = &self->super;
  self->path_options.ack_needed = TRUE;
  return self;
}

static void
setup(void)
{
  app_startup();
}

static void
teardown(void)
{
  app_shutdown();
}

static LogMessage *
create_clone(LogMessage *msg, LogPathOptions *path_options)
{
  LogMessage *cloned = log_msg_ref(msg);
  cloned = log_msg_make_writable(&cloned, path_options);
  log_msg_add_ack(msg, path_options);
  return cloned;
}

TestSuite(msg_ack, .init = setup, .fini = teardown);

Test(msg_ack, normal_ack)
{
  TestAckRecord *t = ack_record_new();
  t->init(t);
  t->deinit(t);
  cr_assert(t->acked);
  ack_record_free(t);
}


Test(msg_ack, clone_ack)
{
  TestAckRecord *t = ack_record_new();
  t->init(t);

  LogMessage *cloned = create_clone(t->original, &t->path_options);

  log_msg_drop(cloned, &t->path_options, AT_PROCESSED);
  cr_assert_not(t->acked);

  t->deinit(t);
  cr_assert(t->acked);
  ack_record_free(t);
}

struct nv_pair
{
  const gchar *name;
  const gchar *value;
};

ParameterizedTestParameters(msg_ack, test_cloned_clone)
{
  static struct nv_pair params[] =
  {
    /* This ensures, that the clone message has own payload */
    {"test", "value"},
    /* Using these parameters the clone message won't has own payload */
    {"", ""}
  };

  size_t nb_params = sizeof (params) / sizeof (struct nv_pair);
  return cr_make_param_array(struct nv_pair, params, nb_params);
}

/*
 * This tests that the clone does not break the acknowledgement or the reference counting,
 * whether the cloned message has own payload or not
 */
ParameterizedTest(struct nv_pair *param, msg_ack, test_cloned_clone)
{
  TestAckRecord *t = ack_record_new();
  t->init(t);

  LogMessage *cloned = create_clone(t->original, &t->path_options);

  log_msg_set_value_by_name(cloned, param->name, param->value, -1);
  log_msg_write_protect(cloned);

  LogMessage *cloned_clone1 = create_clone(cloned,  &t->path_options);
  LogMessage *cloned_clone2 = create_clone(cloned,  &t->path_options);

  log_msg_drop(cloned_clone1, &t->path_options, AT_PROCESSED);
  cr_assert_not(t->acked);

  log_msg_drop(cloned_clone2, &t->path_options, AT_PROCESSED);
  cr_assert_not(t->acked);

  log_msg_drop(cloned, &t->path_options, AT_PROCESSED);
  cr_assert_not(t->acked);

  t->deinit(t);
  cr_assert(t->acked);
  ack_record_free(t);
}
