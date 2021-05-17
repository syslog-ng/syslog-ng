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

public class InternalMessageSender {
  public static final int LevelFatal = 2;
  public static final int LevelError = 3;
  public static final int LevelWarning = 4;
  public static final int LevelNotice = 5;
  public static final int LevelInfo = 6;
  public static final int LevelDebug = 7;

  public static void fatal(String message) {
    createInternalMessage(LevelFatal, message);
  }

  public static void error(String message) {
    createInternalMessage(LevelError, message);
  }

  public static void warning(String message) {
    createInternalMessage(LevelWarning, message);
  }

  public static void notice(String message) {
    createInternalMessage(LevelNotice, message);
  }

  public static void info(String message) {
    createInternalMessage(LevelInfo, message);
  }

  public static void debug(String message) {
    createInternalMessage(LevelDebug, message);
  }

  public native static int getLevel();

  private native static void createInternalMessage(int level, String message);
};
