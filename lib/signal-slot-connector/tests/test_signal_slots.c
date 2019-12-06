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

typedef struct _TestData TestData;

struct _TestData
{
  gint custom_signal_ctr;
  gint slot_ctr;
};

void
test_data_init(TestData *data)
{
  data->custom_signal_ctr = 0;
  data->slot_ctr = 0;
}

SIGNAL(test1_default_signal, TestData *);

void test1_custom_signal(TestData *user_data)
{
  user_data->custom_signal_ctr++;
}

void test1_slot(TestData *user_data)
{
  user_data->slot_ctr++;
}

Test(signal_slots, basic_signal_slot_connections)
{
  SignalSlotConnector *ssc = signal_slot_connector_new();
  TestData test_data;
  test_data_init(&test_data);

  CONNECT(ssc, test1_default_signal, test1_slot);
  CONNECT(ssc, test1_custom_signal, test1_slot);
  EMIT(ssc, test1_default_signal, &test_data);

  cr_expect_eq(test_data.custom_signal_ctr, 0);
  cr_expect_eq(test_data.slot_ctr, 1);

  EMIT(ssc, test1_custom_signal, &test_data);
  
  cr_expect_eq(test_data.custom_signal_ctr, 1);
  cr_expect_eq(test_data.slot_ctr, 2);

  signal_slot_connector_free(ssc);
}

Test(signal_slots, emit_after_disconnect)
{
  SignalSlotConnector *ssc = signal_slot_connector_new();
  TestData test_data;
  test_data_init(&test_data);

  CONNECT(ssc, test1_default_signal, test1_slot);
  CONNECT(ssc, test1_custom_signal, test1_slot);

  DISCONNECT(ssc, test1_default_signal, test1_slot);
  EMIT(ssc, test1_default_signal, &test_data);

  cr_expect_eq(test_data.custom_signal_ctr, 0);
  cr_expect_eq(test_data.slot_ctr, 0);

  EMIT(ssc, test1_custom_signal, &test_data);
  
  cr_expect_eq(test_data.custom_signal_ctr, 1);
  cr_expect_eq(test_data.slot_ctr, 1);

  DISCONNECT(ssc, test1_custom_signal, test1_slot);
  EMIT(ssc, test1_custom_signal, &test_data);

  cr_expect_eq(test_data.custom_signal_ctr, 1);
  cr_expect_eq(test_data.slot_ctr, 1);

  signal_slot_connector_free(ssc);
}

