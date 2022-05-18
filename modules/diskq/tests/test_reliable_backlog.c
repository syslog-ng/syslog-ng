/*
 * Copyright (c) 2002-2016 Balabit
 * Copyright (c) 2016 Viktor Juhasz <viktor.juhasz@balabit.com>
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
#include "libtest/queue_utils_lib.h"
#include "test_diskq_tools.h"

#include "qdisk.c"
#include "logqueue-disk-reliable.h"
#include "apphook.h"
#include "plugin.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>



#define TEST_DISKQ_SIZE QDISK_RESERVED_SPACE + 1000 /* 4096 + 1000 */

static gint num_of_ack;

static gint mark_message_serialized_size;

DiskQueueOptions options;

#define NUMBER_MESSAGES_IN_QUEUE(n) (n * 3)

static void
_dummy_ack(LogMessage *lm,  AckType ack_type)
{
  num_of_ack++;
}

static LogQueueDiskReliable *
_init_diskq_for_test(const gchar *filename, gint64 size, gint64 membuf_size)
{
  LogQueueDiskReliable *dq;

  _construct_options(&options, size, membuf_size, TRUE);
  LogQueue *q = log_queue_disk_reliable_new(&options, NULL);
  struct stat st;
  num_of_ack = 0;
  unlink(filename);
  log_queue_disk_load_queue(q, filename);
  dq = (LogQueueDiskReliable *)q;
  lseek(dq->super.qdisk->fd, size - 1, SEEK_SET);
  ssize_t written = write(dq->super.qdisk->fd, "", 1);
  cr_assert_eq(written, 1, "%s", "Can't write to diskq file");
  fstat(dq->super.qdisk->fd, &st);
  cr_assert_eq(st.st_size, size, "%s", "INITIALIZATION FAILED");
  dq->super.super.use_backlog = TRUE;
  return dq;
}

static void
_common_cleanup(LogQueueDiskReliable *dq, const gchar *file_name)
{
  log_queue_unref(&dq->super.super);
  unlink(file_name);
  disk_queue_options_destroy(&options);
}

static gint
get_serialized_message_size(LogMessage *msg)
{
  GString *serialized;
  SerializeArchive *sa;
  gint result;

  serialized = g_string_sized_new(64);
  sa = serialize_string_archive_new(serialized);

  cr_assert(log_msg_serialize(msg, sa, 0));

  result = serialized->len;

  serialize_archive_free(sa);
  g_string_free(serialized, TRUE);

  return result + sizeof(guint32);
}

static void
set_mark_message_serialized_size(void)
{
  LogMessage *mark_message = log_msg_new_mark();
  mark_message_serialized_size = get_serialized_message_size(mark_message);
  log_msg_unref(mark_message);
}

static void
_prepare_eof_test(LogQueueDiskReliable *dq, LogMessage **msg1, LogMessage **msg2)
{
  LogPathOptions local_options = LOG_PATH_OPTIONS_INIT;
  gint64 start_pos = TEST_DISKQ_SIZE - 1;

  *msg1 = log_msg_new_mark();
  *msg2 = log_msg_new_mark();

  (*msg1)->ack_func = _dummy_ack;
  log_msg_add_ack(*msg1, &local_options);

  (*msg2)->ack_func = _dummy_ack;
  log_msg_add_ack(*msg2, &local_options);

  dq->super.qdisk->hdr->write_head = start_pos;
  dq->super.qdisk->hdr->read_head = QDISK_RESERVED_SPACE + mark_message_serialized_size + 1;
  dq->super.qdisk->hdr->backlog_head = dq->super.qdisk->hdr->read_head;

  log_queue_push_tail(&dq->super.super, *msg1, &local_options);
  log_queue_push_tail(&dq->super.super, *msg2, &local_options);

  cr_assert_eq(dq->qreliable->length, NUMBER_MESSAGES_IN_QUEUE(2), "%s", "Messages aren't in qreliable");
  cr_assert_eq(dq->super.qdisk->hdr->write_head, QDISK_RESERVED_SPACE + mark_message_serialized_size,
               "%s", "Bad write head");
  cr_assert_eq(num_of_ack, 0, "%s", "Messages are acked");

  dq->super.qdisk->hdr->read_head = start_pos;
  dq->super.qdisk->hdr->backlog_head = dq->super.qdisk->hdr->read_head;

}

static void
test_read_over_eof(LogQueueDiskReliable *dq, LogMessage *msg1, LogMessage *msg2)
{
  LogPathOptions read_options;
  LogMessage *read_message1;
  LogMessage *read_message2;

  read_message1 = log_queue_pop_head(&dq->super.super, &read_options);
  cr_assert_not_null(read_message1, "%s", "Can't read message from queue");
  read_message2 = log_queue_pop_head(&dq->super.super, &read_options);
  cr_assert_not_null(read_message2, "%s", "Can't read message from queue");
  cr_assert_eq(dq->qreliable->length, 0, "%s", "Queue reliable isn't empty");
  cr_assert_eq(dq->qbacklog->length, NUMBER_MESSAGES_IN_QUEUE(2), "%s", "Messages aren't in the qbacklog");
  cr_assert_eq(dq->super.qdisk->hdr->read_head, dq->super.qdisk->hdr->write_head,
               "%s", "Read head in bad position");
  cr_assert_eq(msg1, read_message1, "%s", "Message 1 isn't read from qreliable");
  cr_assert_eq(msg2, read_message2, "%s", "Message 2 isn't read from qreliable");
}

static void
test_rewind_over_eof(LogQueueDiskReliable *dq)
{
  LogMessage *msg3 = log_msg_new_mark();
  LogMessage *read_message3;

  LogPathOptions local_options = LOG_PATH_OPTIONS_INIT;
  msg3->ack_func = _dummy_ack;

  log_queue_push_tail(&dq->super.super, msg3, &local_options);
  gint64 previous_read_head = dq->super.qdisk->hdr->read_head;
  read_message3 = log_queue_pop_head(&dq->super.super, &local_options);
  cr_assert_not_null(read_message3, "%s", "Can't read message from queue");
  cr_assert_eq(dq->super.qdisk->hdr->read_head, dq->super.qdisk->hdr->write_head,
               "%s", "Read head in bad position");

  cr_assert_eq(msg3, read_message3, "%s", "Message 3 isn't read from qreliable");
  log_msg_unref(read_message3);

  log_queue_rewind_backlog(&dq->super.super, 1);

  cr_assert_eq(dq->super.qdisk->hdr->read_head, previous_read_head, "%s", "Read head is corrupted");

  read_message3 = log_queue_pop_head(&dq->super.super, &local_options);
  cr_assert_not_null(read_message3, "%s", "Can't read message from queue");
  cr_assert_eq(dq->super.qdisk->hdr->read_head, dq->super.qdisk->hdr->write_head,
               "%s", "Read head in bad position");
  cr_assert_eq(msg3, read_message3, "%s", "Message 3 isn't read from qreliable");

  log_msg_drop(msg3, &local_options, AT_PROCESSED);
}

static void
test_ack_over_eof(LogQueueDiskReliable *dq, LogMessage *msg1, LogMessage *msg2)
{
  log_queue_ack_backlog(&dq->super.super, 3);
  cr_assert_eq(dq->qbacklog->length, 0, "%s", "Messages are in the qbacklog");
  cr_assert_eq(dq->super.qdisk->hdr->backlog_head, dq->super.qdisk->hdr->read_head,
               "%s", "Backlog head in bad position");
}

/* TestCase:
   * write two messages into the diskq
   *  - 1st into the end of the queue,
   *  - 2nd the beginning of the queue
   * the read and ack_backlog and rewind_backlog mechanism must detect,
   * that the read head and backlog head are on the end of the queue,
   * so they will set to the beginning of the queue
   */
/* TODO: add 3 messages and rewind 1 instead of 0 */
/* TODO: split this test into 3 tests (read ack rewind mechanism  (setup method) */
Test(diskq_reliable, test_over_EOF)
{
  const gchar *file_name = "test_over_EOF.rqf";

  LogQueueDiskReliable *dq = _init_diskq_for_test(file_name, TEST_DISKQ_SIZE, TEST_DISKQ_SIZE);
  LogMessage *msg1;
  LogMessage *msg2;

  LogPathOptions read_options = LOG_PATH_OPTIONS_INIT;

  _prepare_eof_test(dq, &msg1, &msg2);

  test_read_over_eof(dq, msg1, msg2);

  test_rewind_over_eof(dq);

  test_ack_over_eof(dq, msg1, msg2);

  log_msg_drop(msg1, &read_options, AT_PROCESSED);
  log_msg_drop(msg2, &read_options, AT_PROCESSED);
  cr_assert_eq(num_of_ack, 2, "%s", "Messages aren't acked");
  _common_cleanup(dq, file_name);
}

/*
 * The method make the following situation
 * the backlog contains 6 messages
 * the qbacklog contains 3 messages,
 * but messages in qbacklog are the end of the backlog
 */
void
_prepare_rewind_backlog_test(LogQueueDiskReliable *dq, gint64 *start_pos)
{
  gint i;

  for (i = 0; i < 8; i++)
    {
      LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
      LogMessage *mark_message;
      mark_message = log_msg_new_mark();
      mark_message->ack_func = _dummy_ack;
      log_queue_push_tail(&dq->super.super, mark_message, &path_options);
    }

  /* Lets read the messages and leave them in the backlog */
  for (i = 0; i < 8; i++)
    {
      LogMessage *msg;
      LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
      msg = log_queue_pop_head(&dq->super.super, &path_options);
      log_msg_unref(msg);
    }

  /* Ack the messages which are not in the qbacklog */
  log_queue_ack_backlog(&dq->super.super, 5);
  cr_assert_eq(dq->qbacklog->length, NUMBER_MESSAGES_IN_QUEUE(3),
               "%s", "Incorrect number of items in the qbacklog");

  *start_pos = dq->super.qdisk->hdr->read_head;

  /* Now write 3 more messages and read them from buffer
   * the number of messages in the qbacklog should not be changed
   * The backlog should contain 6 messages
   * from these 6 messages 3 messages are cached in the qbacklog
   * No readable messages are in the queue
   */
  for (i = 0; i < 3; i++)
    {
      LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
      LogMessage *mark_message;
      mark_message = log_msg_new_mark();
      mark_message->ack_func = _dummy_ack;
      log_queue_push_tail(&dq->super.super, mark_message, &path_options);
      mark_message = log_queue_pop_head(&dq->super.super, &path_options);
      cr_assert_eq(dq->qreliable->length, 0,
                   "%s", "Incorrect number of items in the qreliable");
      cr_assert_eq(dq->qbacklog->length, NUMBER_MESSAGES_IN_QUEUE(3),
                   "%s", "Incorrect number of items in the qbacklog");
      log_msg_unref(mark_message);
    }
  cr_assert_eq(dq->super.qdisk->hdr->backlog_len, 6,
               "%s", "Incorrect number of messages in the backlog");
  cr_assert_eq(dq->super.qdisk->hdr->length, 0,
               "%s", "Reliable diskq isn't empty");
}

void
test_rewind_backlog_without_using_qbacklog(LogQueueDiskReliable *dq, gint64 old_read_pos)
{
  /*
     * Rewind the last 2 messages
     * - the read_head should be moved to the good position
     * - the qbacklog and qreliable should be untouched
     */
  log_queue_rewind_backlog(&dq->super.super, 2);
  cr_assert_eq(dq->super.qdisk->hdr->read_head, old_read_pos + mark_message_serialized_size,
               "%s", "Bad reader position");
  cr_assert_eq(dq->qreliable->length, 0, "%s", "Incorrect number of items in the qreliable");
  cr_assert_eq(dq->qbacklog->length, NUMBER_MESSAGES_IN_QUEUE(3),
               "%s", "Incorrect number of items in the qbacklog");
}

void
test_rewind_backlog_partially_used_qbacklog(LogQueueDiskReliable *dq, gint64 old_read_pos)
{
  /*
   * Rewind more 2 messages
   * - the reader the should be moved to the good position
   * - the qreliable should contain 1 items
   * - the qbackbacklog should contain 2 items
   */
  log_queue_rewind_backlog(&dq->super.super, 2);
  cr_assert_eq(dq->super.qdisk->hdr->read_head, old_read_pos - mark_message_serialized_size,
               "%s", "Bad reader position");
  cr_assert_eq(dq->qreliable->length, NUMBER_MESSAGES_IN_QUEUE(1),
               "%s", "Incorrect number of items in the qreliable");
  cr_assert_eq(dq->qbacklog->length, NUMBER_MESSAGES_IN_QUEUE(2),
               "%s", "Incorrect number of items in the qbacklog");
}

void
test_rewind_backlog_use_whole_qbacklog(LogQueueDiskReliable *dq)
{
  /*
   * Rewind more 2 messages
   * - the reader the should be moved to the backlog head
   * - the qreliable should contain 3 items
   * - the qbackbacklog should be empty
   */
  log_queue_rewind_backlog(&dq->super.super, 2);
  cr_assert_eq(dq->super.qdisk->hdr->read_head, dq->super.qdisk->hdr->backlog_head,
               "%s", "Bad reader position");
  cr_assert_eq(dq->qreliable->length, NUMBER_MESSAGES_IN_QUEUE(3),
               "%s", "Incorrect number of items in the qreliable");
  cr_assert_eq(dq->qbacklog->length, 0,
               "%s", "Incorrect number of items in the qbacklog");

}

/*
 * TestCase
 * backlog contains messages: 1 2 3 4 5 6
 * qbacklog contains messages: 1 2 3
 * qbacklog must be always in sync with backlog
 */
Test(diskq_reliable, test_rewind_backlog)
{
  const gchar *file_name = "test_rewind_backlog.rqf";
  LogQueueDiskReliable *dq = _init_diskq_for_test(file_name, QDISK_RESERVED_SPACE + mark_message_serialized_size * 10 + 1,
                                                  mark_message_serialized_size * 5);
  gint64 old_read_pos;

  _prepare_rewind_backlog_test(dq, &old_read_pos);

  test_rewind_backlog_without_using_qbacklog(dq, old_read_pos);

  test_rewind_backlog_partially_used_qbacklog(dq, old_read_pos);

  test_rewind_backlog_use_whole_qbacklog(dq);

  _common_cleanup(dq, file_name);
}

static void
setup(void)
{
  app_startup();
  setenv("TZ", "MET-1METDST", TRUE);
  tzset();

  configuration = cfg_new_snippet();
  cfg_load_module(configuration, "disk-buffer");

  set_mark_message_serialized_size();
}

static void
teardown(void)
{
  cfg_free(configuration);
  app_shutdown();
}

TestSuite(diskq_reliable, .init = setup, .fini = teardown);
