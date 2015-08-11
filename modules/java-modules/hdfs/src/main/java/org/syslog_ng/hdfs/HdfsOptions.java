/*
 * Copyright (c) 2015 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2015 Viktor Juhasz <viktor.juhasz@balabit.com>
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

package org.syslog_ng.hdfs;

import java.util.Arrays;
import java.util.HashSet;

import org.syslog_ng.LogDestination;
import org.syslog_ng.options.*;

public class HdfsOptions {

  public static String URI = "hdfs_uri";
  public static String FILE = "hdfs_file";
  public static String ARCHIVE_DIR = "hdfs_archive_dir";
  public static String RESOURCES = "hdfs_resources";
  public static String MAX_FILENAME_LENGTH = "hdfs_max_filename_length";
  public static String MAX_FILENAME_LENGTH_DEFAULT = "255";

  private LogDestination owner;
  private Options options;

	public HdfsOptions(LogDestination owner) {
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

  public String getUri() {
    return options.get(URI).getValue();
  }

  public String getFile() {
    return options.get(FILE).getValue();
  }

  public String getArchiveDir() {
    return options.get(ARCHIVE_DIR).getValue();
  }

  public String[] getResouces() {
    return options.get(RESOURCES).getValueAsStringList(";");
  }

  public int getMaxFilenameLength() {
     return options.get(MAX_FILENAME_LENGTH).getValueAsInteger();
  }

  private void fillOptions() {
    fillStringOptions();
  }

  private void fillStringOptions() {
		options.put(new RequiredOptionDecorator(new StringOption(owner, URI)));
		options.put(new RequiredOptionDecorator(new StringOption(owner, FILE)));
		options.put(new StringOption(owner, ARCHIVE_DIR));
		options.put(new StringOption(owner, RESOURCES));
		options.put(new StringOption(owner, MAX_FILENAME_LENGTH, MAX_FILENAME_LENGTH_DEFAULT));
  }

}
