/*
 * Copyright (c) 2015 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2015 Adam Arsenault <adam.arsenault@balabit.com>
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

package org.syslog_ng.http;

import java.util.Arrays;
import java.util.HashSet;

import org.syslog_ng.LogDestination;
import org.syslog_ng.options.*;

public class HTTPDestinationOptions {

  public static String URL = "url";

  private LogDestination owner;
  private Options options;

	public HTTPDestinationOptions(LogDestination owner) {
		this.owner = owner;
		options = new Options();
		fillOptions();
	}

	public void init() throws InvalidOptionException {
		options.validate();
	}

	public void deinit() {
		options.deinit();
	}

  public Option getOption(String optionName) {
    return options.get(optionName);
  }

  public String getURL() {
    return options.get(URL).getValue();
  }

  private void fillOptions() {
    fillStringOptions();
  }

  private void fillStringOptions() {
		options.put(new RequiredOptionDecorator(new StringOption(owner, URL)));
  }

}
