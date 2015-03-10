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

public class LogTemplate {
  private long handle;
  private long configHandle;

  public LogTemplate(long configHandle) {
    this.configHandle = configHandle;
    handle = create_new_template_instance(configHandle);
  }

  public boolean compile(String template) {
    return compile(handle, template);
  }

  public String format(LogMessage msg) {
    return format(handle, msg.getHandle());
  }

  public void release() {
    unref(handle);
  }

  private native long create_new_template_instance(long configHandle);
  private native boolean compile(long handle, String template);
  private native String format(long handle, long msg_handle);
  private native void unref(long handle);
}
