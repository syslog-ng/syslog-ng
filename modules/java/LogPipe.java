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

import java.io.StringWriter;
import java.io.PrintWriter;

import org.syslog_ng.InternalMessageSender;

public abstract class LogPipe {
	private long pipeHandle;
	private long configHandle;

	public LogPipe(long pipeHandle) {
		this.pipeHandle = pipeHandle;
		configHandle = 0;
	}

	public long getConfigHandle() {
		if (configHandle == 0) {
			configHandle = getConfigHandle(pipeHandle);
		}
		return configHandle;
	}

	protected abstract boolean init();
	protected abstract void deinit();

	protected String getStackTrace(Exception e) {
		StringWriter sw = new StringWriter();
		PrintWriter pw = new PrintWriter(sw);
		e.printStackTrace(pw);
		return sw.toString();
	}

	protected void sendExceptionMessage(Exception e) {
		InternalMessageSender.error("Exception occured: " + getStackTrace(e));
	}

	public boolean initProxy() {
		try {
			return init();
		}
		catch (Exception e) {
			sendExceptionMessage(e);
			return false;
		}
	}

	public void deinitProxy() {
		try {
			deinit();
		}
		catch (Exception e) {
			sendExceptionMessage(e);
		}
	}

	public long getHandle() {
		return pipeHandle;
	}
	private native long getConfigHandle(long ptr);
}
