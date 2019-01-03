/*
 * Copyright (c) 2014 Balabit
 * Copyright (c) 2014 Viktor Juhasz <viktor.juhasz@balabit.com>
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
package org.syslog_ng;

public class DummyStructuredDestination extends StructuredLogDestination {

  private String name;

  public DummyStructuredDestination(long arg0) {
    super(arg0);
  }

  public void deinit() {
    System.out.println("Deinit");
  }

  public int flush() {
    System.out.println("Flush");
    return WORKER_INSERT_RESULT_SUCCESS;
  }

  public boolean init() {
    name = getOption("name");
    if (name == null) {
      System.err.println("Name is a required option for this destination");
      return false;
    }
    System.out.println("Init " + name);
    return true;
  }

  public boolean open() {
    return true;
  }

  public boolean isOpened() {
    return true;
  }

  public void close() {
    System.out.println("close");
  }

  public int send(LogMessage arg0) {
    arg0.release();
    return WORKER_INSERT_RESULT_SUCCESS;
  }

  @Override
  public String getNameByUniqOptions() {
    InternalMessageSender.debug("getNameByUniqOptions");
    return "DummyStructuredDestination";
  }

}
