package org.syslog_ng.options.test;

import org.junit.Before;
import org.junit.Test;
import org.syslog_ng.options.IntegerOptionDecorator;
import org.syslog_ng.options.Option;
import org.syslog_ng.options.StringOption;

public class TestIntegerOptionDecorator extends TestOption {
	Option stringOption;
	Option intdecorator;
	@Before
	public void setUp() throws Exception {
		super.setUp();
		stringOption = new StringOption(owner, "port");
		intdecorator = new IntegerOptionDecorator(stringOption);

		options.put("name", "value");
	}

	@Test
	public void testNormal() {
		options.put("port", "9300");
		assertInitOptionSuccess(intdecorator);
	}

	@Test
	public void testInvalid() {
		options.put("port", "value");
		assertInitOptionFailed(intdecorator, "option port must be numerical");

		options.remove("port");
		assertInitOptionFailed(intdecorator, "option port must be numerical");
	}

}
