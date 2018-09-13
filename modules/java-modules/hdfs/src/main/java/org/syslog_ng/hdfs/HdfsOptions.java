/*
 * Copyright (c) 2015 Balabit
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
    public static String KERBEROS_PRINCIPAL = "kerberos_principal";
    public static String KERBEROS_KEYTAB_FILE = "kerberos_keytab_file";
    public static String TEMPLATE = "template";
    public static String TEMPLATE_DEFAULT = "${ISODATE} ${HOST} ${MSGHDR}${MSG}\n";
    public static String APPEND_ENABLED = "hdfs_append_enabled";
    public static String APPEND_ENABLED_DEFAULT = "false";
    public static String TIME_REAP = "time_reap";
    public static String TIME_REAP_DEFAULT = "0";

    private LogDestination owner;
    private Options options;

    public HdfsOptions(LogDestination owner) {
        this.owner = owner;
        options = new Options();
        fillOptions();
        fillTemplateOptions();
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

    public String getArchiveDir() {
        return options.get(ARCHIVE_DIR).getValue();
    }

    public String[] getResouces() {
        return options.get(RESOURCES).getValueAsStringList(";");
    }

    public int getMaxFilenameLength() {
        return options.get(MAX_FILENAME_LENGTH).getValueAsInteger();
    }

    public String getKerberosPrincipal() {
        return options.get(KERBEROS_PRINCIPAL).getValue();
    }

    public String getKerberosKeytabFile() {
        return options.get(KERBEROS_KEYTAB_FILE).getValue();
    }

    public TemplateOption getTemplate() {
        return options.getTemplateOption(TEMPLATE);
    }

    public TemplateOption getFileNameTemplate() {
        return options.getTemplateOption(FILE);
    }

    private void fillOptions() {
        fillStringOptions();
        fillBooleanOptions();
    }

    public boolean getAppendEnabled() {
        return options.get(APPEND_ENABLED).getValueAsBoolean();
    }

    public int getTimeReap() {
        return 1000 * options.get(TIME_REAP).getValueAsInteger();
    }

    private void fillStringOptions() {
        options.put(new RequiredOptionDecorator(new StringOption(owner, URI)));
        options.put(new StringOption(owner, ARCHIVE_DIR));
        options.put(new StringOption(owner, RESOURCES));
        options.put(new StringOption(owner, MAX_FILENAME_LENGTH, MAX_FILENAME_LENGTH_DEFAULT));
        options.put(new StringOption(owner, KERBEROS_PRINCIPAL));
        options.put(new StringOption(owner, KERBEROS_KEYTAB_FILE));
        options.put(new IntegerRangeCheckOptionDecorator(new StringOption(owner, TIME_REAP, TIME_REAP_DEFAULT), 0, Integer.MAX_VALUE));
    }

    private void fillBooleanOptions() {
        options.put(new BooleanOptionDecorator(new StringOption(owner, APPEND_ENABLED, APPEND_ENABLED_DEFAULT)));
    }

    private void fillTemplateOptions() {
        options.put(new TemplateOption(owner.getConfigHandle(), new StringOption(owner, TEMPLATE, TEMPLATE_DEFAULT)));
        options.put(new TemplateOption(owner.getConfigHandle(),
                new RequiredOptionDecorator(new StringOption(owner, FILE))));
    }

}
