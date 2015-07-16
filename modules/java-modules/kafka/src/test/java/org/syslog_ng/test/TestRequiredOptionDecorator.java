package org.syslog_ng.test;

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
