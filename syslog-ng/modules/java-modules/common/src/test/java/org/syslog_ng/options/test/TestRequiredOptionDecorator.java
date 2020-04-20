/*
 * Copyright (c) 2015 Balabit
 * Copyright (c) 2015 Viktor Juhasz <viktor.juhasz@balabit.com>
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

package org.syslog_ng.options.test;

import org.junit.Before;
import org.junit.Test;
import org.syslog_ng.options.Option;
import org.syslog_ng.options.RequiredOptionDecorator;
import org.syslog_ng.options.StringOption;

public class TestRequiredOptionDecorator extends TestOption {
	Option stringOption;
	Option decorator;

	@Before
	public void setUp() throws Exception {
		super.setUp();
		stringOption = new StringOption(owner, "required");
		decorator = new RequiredOptionDecorator(stringOption);
	}

	@Test
	public void testNormal() {
		options.put("required", "test");
		assertInitOptionSuccess(decorator);

		options.remove("required");
		stringOption = new StringOption(owner, "required", "default");
		decorator = new RequiredOptionDecorator(stringOption);
		assertInitOptionSuccess(decorator);
	}

	@Test
	public void testInvalid() {
		assertInitOptionFailed(decorator, "option required is a required option");
	}


}
