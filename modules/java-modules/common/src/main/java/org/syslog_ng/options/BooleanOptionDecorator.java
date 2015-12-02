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

package org.syslog_ng.options;

import java.util.Arrays;
import java.util.List;

public class BooleanOptionDecorator extends OptionDecorator {
    public static List ON_LIST = Arrays.asList("true", "on", "yes");
    public static List OFF_LIST = Arrays.asList("false", "off", "no");

    public BooleanOptionDecorator(Option decoratedOption) {
        super(decoratedOption);
    }

    public void validate() throws InvalidOptionException {
        decoratedOption.validate();

        Boolean value = getValueAsBoolean();

        if (value == null) {
            String errorMessage = String.format("invalid value of '%s': '%s'. Valid values are: 'true', 'on', 'yes', " +
                    "'false', 'off', 'no'", decoratedOption.getName(), value);
            throw new InvalidOptionException(errorMessage);
        }
    }

    public Boolean getValueAsBoolean() {
        String value = decoratedOption.getValue().toLowerCase();

        if (ON_LIST.contains(value)) {
            return true;
        } else if (OFF_LIST.contains(value)) {
            return false;
        } else {
            return null;
        }
    }
}
