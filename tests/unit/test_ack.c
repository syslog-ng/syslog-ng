#include "testutils.h"
#include "logsource.h"
#include "apphook.h"

GlobalConfig *cfg;

typedef struct _TestLogSource
{
  LogSource super;
  guint32 destroy_counter;
  guint32 ack_counter;
  gint64 required_ack_id;
} TestLogSource;

LogSourceOptions options;

typedef struct _AckDataTestState
{
  AckDataBase super;
  union{
    struct {
      gpointer data;
    } test_state;
    char other_state[MAX_STATE_DATA_LENGTH];
  };
} AckDataTestState;

void
test_ack_destroy(LogSource *s, gpointer user_data)
{
  AckDataTestState *t = (AckDataTestState *)user_data;
  TestLogSource *self = (TestLogSource *)s;
  g_free(t->test_state.data);
  self->destroy_counter++;
  memset(t, 0xFF, sizeof(AckData));
}

gboolean
test_ack_state(LogSource *s, gpointer user_data, gboolean need_to_save)
{
  TestLogSource *self = (TestLogSource *)s;
  AckDataTestState *t = (AckDataTestState *)user_data;
  self->ack_counter++;
  assert_gint64(t->super.id, self->required_ack_id, ASSERTION_ERROR("Not the correct message was acked"));
  return TRUE;
}

void
test_get_state(LogSource *s, gpointer user_data)
{
  AckDataTestState *t = (AckDataTestState *)user_data;

  t->test_state.data = g_strdup("LEAK");
}

void
test_source_init(TestLogSource *self)
{
  gint i = 0;
  log_source_options_defaults(&options);

  log_source_options_init(&options, cfg, "test");
  log_source_init_instance(&self->super);
  log_source_set_options(&self->super, &options, 0, 0, "test", "test", TRUE);
  log_source_set_pos_tracking(&self->super, TRUE);
  log_pipe_init(&self->super.super, cfg);
  self->super.destroy_ack_data = test_ack_destroy;
  self->super.get_state = test_get_state;
  self->super.ack = test_ack_state;
  self->super.ack_list = g_new(AckData,self->super.options->init_window_size);

  for(i = 0; i<self->super.options->init_window_size; i++)
    {
      memset(&self->super.ack_list[i], 0xFF, sizeof(AckData));
    }

}

void
test_ack_destroy_data(void)
{
  TestLogSource *src = g_new0(TestLogSource, 1);
  LogPathOptions po0 = LOG_PATH_OPTIONS_INIT;
  LogPathOptions po1 = LOG_PATH_OPTIONS_INIT;
  LogMessage *msg0;
  LogMessage *msg1;

  test_source_init(src);

  msg0 = log_msg_new_mark();
  msg1 = log_msg_new_mark();

  log_msg_add_ack(msg0, &po0);

  log_pipe_queue(&src->super.super, msg0, &po0);
  log_pipe_queue(&src->super.super, msg1, &po1);

  assert_guint32(src->destroy_counter, 0, ASSERTION_ERROR("destroy_counter should be 0"));
  src->required_ack_id = 1;
  log_msg_ack(msg0, &po0, TRUE);
  assert_guint32(src->destroy_counter, 2, ASSERTION_ERROR("destroy_counter should be 2"));
  assert_guint32(src->ack_counter, 1, ASSERTION_ERROR("destroy_counter should be 2"));

  log_pipe_deinit(&src->super.super);
  log_pipe_unref(&src->super.super);
}

void
test_ack_ringbuffer_sequential(void)
{
  TestLogSource *src = g_new0(TestLogSource, 1);
  test_source_init(src);

  guint32 i;
  guint32 num_msg = src->super.options->init_window_size * 2;

  for (i = 0; i < num_msg; i++)
    {
      LogMessage *msg = log_msg_new_mark();
      LogPathOptions po = LOG_PATH_OPTIONS_INIT;
      src->required_ack_id = i % src->super.options->init_window_size;
      log_pipe_queue(&src->super.super, msg, &po);
    }

  assert_guint32(src->ack_counter, num_msg, ASSERTION_ERROR("FAILED"));
  assert_guint32(src->destroy_counter, num_msg, ASSERTION_ERROR("FAILED"));



  log_pipe_deinit(&src->super.super);
  log_pipe_unref(&src->super.super);
}

void
test_ack_ringbuffer_not_sequential(void)
{
  TestLogSource *src = g_new0(TestLogSource, 1);
  test_source_init(src);

  guint32 i;
  guint32 late_ack_id = src->super.options->init_window_size - 10;
  guint32 num_msg = late_ack_id + src->super.options->init_window_size;
  LogMessage *late_ack_message;
  LogPathOptions late_po = LOG_PATH_OPTIONS_INIT;


  for (i = 0; i < num_msg; i++)
      {
        LogMessage *msg = log_msg_new_mark();
        LogPathOptions po = LOG_PATH_OPTIONS_INIT;
        if (i == late_ack_id)
          {
            late_ack_message = log_msg_ref(msg);
            log_msg_add_ack(late_ack_message, &late_po);
          }
        src->required_ack_id = i % src->super.options->init_window_size;
        log_pipe_queue(&src->super.super, msg, &po);
      }

  assert_guint32(src->ack_counter, late_ack_id, ASSERTION_ERROR("FAILED"));
  assert_guint32(src->destroy_counter, late_ack_id, ASSERTION_ERROR("FAILED"));

  log_msg_drop(late_ack_message, &late_po);

  assert_guint32(src->ack_counter, late_ack_id + 1, ASSERTION_ERROR("FAILED"));
  assert_guint32(src->destroy_counter, num_msg, ASSERTION_ERROR("FAILED"));

  log_pipe_deinit(&src->super.super);
  log_pipe_unref(&src->super.super);
}


gint
main(gint argc, gchar **argv)
{
	app_startup();
	cfg = cfg_new(0x500);
	test_ack_destroy_data();
	test_ack_ringbuffer_sequential();
	test_ack_ringbuffer_not_sequential();
	app_shutdown();
	cfg_free(cfg);
	return 0;
}
