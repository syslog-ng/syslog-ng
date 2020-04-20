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

public class LogTemplate {
  private long configHandle;
  private long templateHandle;

  public static final int LTZ_LOCAL = 0;
  public static final int LTZ_SEND = 1;

  public LogTemplate(long configHandle) {
    this.configHandle = configHandle;
    templateHandle = create_new_template_instance(configHandle);
  }

  public boolean compile(String template) {
    return compile(templateHandle, template);
  }

  public String format(LogMessage msg) {
    return format(msg, 0, LTZ_SEND);
  }

  public String format(LogMessage msg, long logTemplateOptionsHandle) {
    return format(msg, logTemplateOptionsHandle, LTZ_SEND);
  }

  public String format(LogMessage msg, long logTemplateOptionsHandle, int timezone) {
    return format(templateHandle, msg.getHandle(), logTemplateOptionsHandle, timezone, 0);
  }

  public String format(LogMessage msg, long logTemplateOptionsHandle, int timezone, int seqnum) {
    return format(templateHandle, msg.getHandle(), logTemplateOptionsHandle, timezone, seqnum);
  }

  public void release() {
    unref(templateHandle);
  }

  private native long create_new_template_instance(long configHandle);
  private native boolean compile(long templateHandle, String template);
  private native String format(long templateHandle, long msgHandle, long templateOptionsHandle, int timezone, int seqnum);
  private native void unref(long templateHandle);
}
