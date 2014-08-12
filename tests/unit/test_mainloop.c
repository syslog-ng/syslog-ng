#include "testutils.h"
#include "cfg.h"
#include "logpipe.h"
#include "center.h"
#include "messages.h"
#include "sgroup.h"

extern GlobalConfig *main_loop_old_config;
extern GlobalConfig *main_loop_new_config;

typedef struct _TestPipe {
  LogSrcDriver super;
  gint state;
} TestPipe;

gint number_of_times_test_source_init_called = 0;
gint number_of_times_test_source_deinit_called = 0;

gboolean
log_pipe_test_source_init(LogPipe *s)
{
  TestPipe *self = (TestPipe *)s;
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super.super);

  self->state = GPOINTER_TO_INT(cfg_persist_config_fetch(cfg, "alma"));

  self->state++;

  number_of_times_test_source_init_called++;

  assert_gint(self->state, number_of_times_test_source_init_called, "State couldn't be stored properly");
  return TRUE;
};

gboolean
log_pipe_test_source_deinit(LogPipe *s)
{
  TestPipe *self = (TestPipe *)s;
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super.super);

  number_of_times_test_source_deinit_called++;

  cfg_persist_config_add(cfg, "alma", GINT_TO_POINTER(self->state), NULL, FALSE);
  return TRUE;
};

gboolean
log_pipe_test_dest_init(LogPipe *self)
{
  return TRUE;
};

gboolean
log_pipe_test_dest_fail_init(LogPipe *self)
{
  return FALSE;
};

LogPipe* create_source()
{
  LogPipe *pipe = g_new0(TestPipe, 1);
  log_src_driver_init_instance(pipe);
  pipe->init = log_pipe_test_source_init;
  pipe->deinit = log_pipe_test_source_deinit;
  LogSourceGroup *sgroup = log_source_group_new("s_test", pipe);
  return sgroup;
};

LogPipe* create_good_dest()
{
  LogPipe *dpipe = g_new0(LogPipe, 1);
  log_pipe_init_instance(dpipe);
  dpipe->init = log_pipe_test_dest_init;
  return dpipe;
};

LogPipe* create_bad_dest()
{
  LogPipe *dpipe = g_new0(LogPipe, 1);
  log_pipe_init_instance(dpipe);
  dpipe->init = log_pipe_test_dest_fail_init;
  return dpipe;
};

GlobalConfig* create_config(LogPipe *spipe, LogPipe *dpipe)
{
  GlobalConfig* cfg = cfg_new(0x0);
  cfg->center = log_center_new();

  g_hash_table_insert(cfg->sources, "s_test", spipe);

  g_hash_table_insert(cfg->destinations, "d_test", dpipe);

  LogPipeItem *logitem = log_pipe_item_new(EP_SOURCE, "s_test");
  log_pipe_item_append_tail(logitem, log_pipe_item_new(EP_DESTINATION, "d_test"));

  LogConnection* logconn = log_connection_new(logitem, 0);
  cfg_add_connection(cfg, logconn);
  return cfg;
};

GlobalConfig* create_and_init_old_configuration()
{
  LogPipe *spipe = create_source();
  LogPipe *dpipe = create_good_dest();

  GlobalConfig *cfg = create_config(spipe, dpipe);

  main_loop_initialize_state(cfg, "test.persist");

  stats_timer_init(cfg);

  return cfg;
};

GlobalConfig* create_new_configuration()
{
  LogPipe *spipe = create_source();
  LogPipe *dpipe = create_bad_dest();

  return create_config(spipe, dpipe);
};

int main()
{
  app_startup();

  log_stderr = TRUE;
  debug_flag = TRUE;

  main_loop_old_config = create_and_init_old_configuration();
  main_loop_new_config = create_new_configuration();

  main_loop_reload_config_apply();

  assert_gint(number_of_times_test_source_init_called, 3,  "init() of TestPipe didn't called properly");
  assert_gint(number_of_times_test_source_deinit_called, 2, "deinit() of TestPipe didn't called properly");

  app_shutdown();

  return 0;
};
