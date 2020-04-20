/*
 * Copyright (c) 2015 Balabit
 * Copyright (c) 2015 Tibor Benke
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

package org.syslog_ng.test;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.syslog_ng.options.BooleanOptionDecorator;
import org.syslog_ng.options.StringOption;

import java.util.Arrays;

public class TestBooleanOptionDecorator extends TestOption {

    @Before
    public void setUp() throws Exception {
        super.setUp();
    }

    @After
    public void tearDown() {
        options.clear();
    }

    @Test
    public void testTrueValues() {
        for (String value: Arrays.asList("true", "on", "yes", "Yes", "YES")) {
            options.put("key", value);
            StringOption option = new StringOption(owner, "key");
            BooleanOptionDecorator decorator = new BooleanOptionDecorator(option);
            assertInitOptionSuccess(decorator);
            Assert.assertEquals(decorator.getValueAsBoolean(), Boolean.TRUE);
        }
    }

    @Test
    public void testFalseValues() {
        for (String value: Arrays.asList("false", "off", "no", "NO", "OFF")) {
            options.put("key", value);
            StringOption option = new StringOption(owner, "key");
            BooleanOptionDecorator decorator = new BooleanOptionDecorator(option);
            assertInitOptionSuccess(decorator);
            Assert.assertEquals(decorator.getValueAsBoolean(), Boolean.FALSE);
        }
    }

    @Test
    public void testFailingValues() {
        for (String value: Arrays.asList("alpha", "beta")) {
            options.put("key", value);
            StringOption option = new StringOption(owner, "key");
            BooleanOptionDecorator decorator = new BooleanOptionDecorator(option);
            assertInitOptionFailed(decorator);
            Assert.assertEquals(decorator.getValueAsBoolean(), null);
        }
    }
}