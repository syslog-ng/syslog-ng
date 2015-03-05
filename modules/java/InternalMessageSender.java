/*
 * Copyright (c) 2014 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2014 Viktor Juhasz <viktor.juhasz@balabit.com>
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


package org.syslog_ng;

public class InternalMessageSender {
  private static final int MsgFatal = 2;
  private static final int MsgError = 3;
  private static final int MsgWarning = 4;
  private static final int MsgNotice = 5;
  private static final int MsgInfo = 6;
  private static final int MsgDebug = 7;

  public static void fatal(String message) {
    createInternalMessage(MsgFatal, message);
  }

  public static void error(String message) {
    createInternalMessage(MsgError, message);
  }
  
  public static void warning(String message) {
    createInternalMessage(MsgWarning, message);
  }
  
  public static void notice(String message) {
    createInternalMessage(MsgNotice, message);
  }

  public static void info(String message) {
    createInternalMessage(MsgInfo, message);
  }
  
  public static void debug(String message) {
    createInternalMessage(MsgDebug, message);
  }
  
  private native static void createInternalMessage(int level, String message);
};
