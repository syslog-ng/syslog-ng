/*
 * Copyright (c) 2002-2019 One Identity
 * Copyright (c) 2019 Laszlo Budai
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

#include <criterion/criterion.h>
#include "signal-slot-connector/signal-slot-connector.h"

#define signal_test1 SIGNAL(test, 1, TestData *)
#define signal_test2 SIGNAL(test, 2, TestData *)

typedef struct _TestData TestData;

struct _TestData
{
  gint slot_ctr;
};

typedef struct _SlotObj SlotObj;

struct _SlotObj
{
  gint ctr;
};

void
test_data_init(TestData *data)
{
  data->slot_ctr = 0;
}

void
slot_obj_init(SlotObj *obj)
{
  obj->ctr = 0;
}

void test1_slot(gpointer obj, TestData *user_data)
{
  if (obj)
    {
      SlotObj *slot_obj = (SlotObj *)obj;
      slot_obj->ctr++;
    }
  user_data->slot_ctr++;
}

void test2_slot(gpointer obj, TestData *user_data)
{
  if (obj)
    {
      SlotObj *slot_obj = (SlotObj *)obj;
      slot_obj->ctr++;
    }
  user_data->slot_ctr++;
}

Test(basic_signal_slots, when_the_signal_is_emitted_then_the_connected_slot_is_executed)
{
  SignalSlotConnector *ssc = signal_slot_connector_new();
  TestData test_data;
  test_data_init(&test_data);
  SlotObj slot_obj1, slot_obj2;
  slot_obj_init(&slot_obj1);
  slot_obj_init(&slot_obj2);

  CONNECT(ssc, signal_test1, test1_slot, &slot_obj1);
  CONNECT(ssc, signal_test2, test1_slot, &slot_obj2);
  EMIT(ssc, signal_test1, &test_data);

  cr_expect_eq(test_data.slot_ctr, 1);
  cr_expect_eq(slot_obj1.ctr, 1);
  cr_expect_eq(slot_obj2.ctr, 0);

  EMIT(ssc, signal_test2, &test_data);

  cr_expect_eq(test_data.slot_ctr, 2);
  cr_expect_eq(slot_obj1.ctr, 1);
  cr_expect_eq(slot_obj2.ctr, 1);

  signal_slot_connector_free(ssc);
}

Test(basic_signal_slots, when_trying_to_connect_multiple_times_the_same_connection_then_connect_only_the_first)
{
  SignalSlotConnector *ssc = signal_slot_connector_new();
  TestData test_data;
  test_data_init(&test_data);
  SlotObj slot_obj;
  slot_obj_init(&slot_obj);

  for (gint i = 0; i < 5; i++)
    CONNECT(ssc, signal_test1, test1_slot, &slot_obj);

  EMIT(ssc, signal_test1, &test_data);

  cr_expect_eq(test_data.slot_ctr, 1);
  cr_expect_eq(slot_obj.ctr, 1);

  signal_slot_connector_free(ssc);
}

Test(basic_signal_slots, when_trying_to_disconnect_multiple_times_the_same_connection_disconnect_only_the_first)
{
  SignalSlotConnector *ssc = signal_slot_connector_new();
  TestData test_data;
  test_data_init(&test_data);

  CONNECT(ssc, signal_test1, test1_slot, NULL);
  CONNECT(ssc, signal_test1, test2_slot, NULL);

  for (gint i = 0; i < 5; i++)
    DISCONNECT(ssc, signal_test1, test1_slot, NULL);

  EMIT(ssc, signal_test1, &test_data);

  cr_expect_eq(test_data.slot_ctr, 1);

  signal_slot_connector_free(ssc);
}

Test(basic_signal_slots,
     when_slot_is_disconnected_from_a_signal_and_the_signal_is_emitted_then_the_slot_should_not_be_executed)
{
  SignalSlotConnector *ssc = signal_slot_connector_new();
  TestData test_data;
  test_data_init(&test_data);
  SlotObj slot_obj;
  slot_obj_init(&slot_obj);

  CONNECT(ssc, signal_test1, test1_slot, &slot_obj);
  CONNECT(ssc, signal_test2, test1_slot, &slot_obj);

  DISCONNECT(ssc, signal_test1, test1_slot, &slot_obj);
  EMIT(ssc, signal_test1, &test_data);

  cr_expect_eq(test_data.slot_ctr, 0);
  cr_expect_eq(slot_obj.ctr, 0);

  EMIT(ssc, signal_test2, &test_data);

  cr_expect_eq(test_data.slot_ctr, 1);
  cr_expect_eq(slot_obj.ctr, 1);

  DISCONNECT(ssc, signal_test2, test1_slot, &slot_obj);
  EMIT(ssc, signal_test2, &test_data);

  cr_expect_eq(test_data.slot_ctr, 1);
  cr_expect_eq(slot_obj.ctr, 1);

  signal_slot_connector_free(ssc);
}

Test(basic_signal_slots, when_disconnect_the_connected_slot_from_a_signal_then_the_signal_is_unregistered)
{
  SignalSlotConnector *ssc = signal_slot_connector_new();
  TestData test_data;
  test_data_init(&test_data);
  SlotObj slot_obj;
  slot_obj_init(&slot_obj);

  CONNECT(ssc, signal_test1, test1_slot, &slot_obj);
  DISCONNECT(ssc, signal_test1, test1_slot, &slot_obj);
  EMIT(ssc, signal_test1, &test_data);

  cr_expect_eq(test_data.slot_ctr, 0);
  cr_expect_eq(slot_obj.ctr, 0);

  signal_slot_connector_free(ssc);
}

Test(basic_signal_slots,
     when_trying_to_disconnect_a_connected_slot_with_different_slot_object_then_slot_is_not_disconnected)
{
  SignalSlotConnector *ssc = signal_slot_connector_new();
  TestData test_data;
  test_data_init(&test_data);
  SlotObj slot_obj;
  slot_obj_init(&slot_obj);
  SlotObj slot_obj_another;
  slot_obj_init(&slot_obj_another);

  CONNECT(ssc, signal_test1, test1_slot, &slot_obj);
  DISCONNECT(ssc, signal_test1, test1_slot, &slot_obj_another);
  EMIT(ssc, signal_test1, &test_data);

  cr_expect_eq(test_data.slot_ctr, 1);
  cr_expect_eq(slot_obj.ctr, 1);
  cr_expect_eq(slot_obj_another.ctr, 0);

  signal_slot_connector_free(ssc);
}

#define DEFINE_TEST_SLOT(func_name) \
  void func_name(gpointer obj, TestData *user_data) \
  { \
    if (obj) \
      { \
         SlotObj *slot_obj = (SlotObj *)obj; \
         slot_obj->ctr++; \
      } \
    user_data->slot_ctr++; \
  }

DEFINE_TEST_SLOT(test_slot_0)
DEFINE_TEST_SLOT(test_slot_1)
DEFINE_TEST_SLOT(test_slot_2)
DEFINE_TEST_SLOT(test_slot_3)

#define test_signal_0 SIGNAL(test, func_0, TestData *)
#define test_signal_1 SIGNAL(test, func_1, TestData *)
#define test_signal_2 SIGNAL(test, func_2, TestData *)
#define test_signal_3 SIGNAL(test, func_3, TestData *)

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

Signal signals[4] =
{
  test_signal_0
};

Slot slots[4] =
{
  (Slot) test_slot_0,
  (Slot) test_slot_1,
  (Slot) test_slot_2,
  (Slot) test_slot_3
};

SlotObj slot_objects[4];

void
_connect_a_signal_with_all_test_slots(SignalSlotConnector *ssc, Signal s)
{
  for (gint i = 0; i < ARRAY_SIZE(slots); i++)
    CONNECT(ssc, s, slots[i], &slot_objects[i]);
}

void
_disconnect_all_test_slots_from_a_signal(SignalSlotConnector *ssc, Signal s)
{
  for (gint i = 0; i < ARRAY_SIZE(slots); i++)
    DISCONNECT(ssc, s, slots[i], &slot_objects[i]);
}

Test(multiple_signals_slots,
     given_one_signal_with_multiple_slots_when_the_signal_is_emitted_then_the_slots_are_executed)
{
  SignalSlotConnector *ssc = signal_slot_connector_new();
  TestData test_data;
  test_data_init(&test_data);

  _connect_a_signal_with_all_test_slots(ssc, signals[0]);

  EMIT(ssc, signals[0], &test_data);
  cr_expect_eq(test_data.slot_ctr, ARRAY_SIZE(slots));

  for (gsize i = 0; i < ARRAY_SIZE(slot_objects); i++)
    cr_expect_eq(slot_objects[i].ctr, 1);

  _disconnect_all_test_slots_from_a_signal(ssc, signals[0]);

  signal_slot_connector_free(ssc);
}

Test(multiple_signals_slots,
     given_a_signal_with_multiple_slots_when_slots_are_disconnected_1_by_1_then_remaining_slots_are_still_executed)
{
  SignalSlotConnector *ssc = signal_slot_connector_new();
  TestData test_data;
  test_data_init(&test_data);

  for (gint i = 0; i < 3; i++)
    CONNECT(ssc, signals[0], slots[i], &slot_objects[i]);

  DISCONNECT(ssc, signals[0], slots[0], &slot_objects[0]);
  EMIT(ssc, signals[0], &test_data);
  cr_expect_eq(test_data.slot_ctr, 2);
  cr_expect_eq(slot_objects[0].ctr, 0);
  cr_expect_eq(slot_objects[1].ctr, 1);
  cr_expect_eq(slot_objects[2].ctr, 1);

  DISCONNECT(ssc, signals[0], slots[1], &slot_objects[1]);
  EMIT(ssc, signals[0], &test_data);
  cr_expect_eq(test_data.slot_ctr, 3);
  cr_expect_eq(slot_objects[0].ctr, 0);
  cr_expect_eq(slot_objects[1].ctr, 1);
  cr_expect_eq(slot_objects[2].ctr, 2);

  DISCONNECT(ssc, signals[0], slots[2], &slot_objects[2]);
  EMIT(ssc, signals[0], &test_data);
  cr_expect_eq(test_data.slot_ctr, 3);
  cr_expect_eq(slot_objects[0].ctr, 0);
  cr_expect_eq(slot_objects[1].ctr, 1);
  cr_expect_eq(slot_objects[2].ctr, 2);

  signal_slot_connector_free(ssc);
}

