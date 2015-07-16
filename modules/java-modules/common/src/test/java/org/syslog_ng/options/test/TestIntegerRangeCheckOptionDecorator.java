package org.syslog_ng.options.test;

import org.junit.Before;
import org.junit.Test;
import org.syslog_ng.options.IntegerRangeCheckOptionDecorator;
import org.syslog_ng.options.Option;
import org.syslog_ng.options.StringOption;

public class TestIntegerRangeCheckOptionDecorator extends TestOption {
	Option stringOption;
	Option decorator;

	@Before
	public void setUp() throws Exception {
		super.setUp();
		stringOption = new StringOption(owner, "range");
		decorator = new IntegerRangeCheckOptionDecorator(stringOption, 0, 1000);
	}

	@Test
	public void testNormal() {
		for (int i = 0; i < 1001; i++) {
			options.put("range", new Integer(i).toString());
			assertInitOptionSuccess(decorator);
		}
	}

	@Test
	public void testInvalid() {
		options.put("range", "-1");
		assertInitOptionFailed(decorator, "option range should be between 0 and 1000");

		options.put("range", "1001");
		assertInitOptionFailed(decorator, "option range should be between 0 and 1000");

		options.put("range", "0.1");
		assertInitOptionFailed(decorator, "option range must be numerical");

		options.put("range", "aaa");
		assertInitOptionFailed(decorator, "option range must be numerical");

		options.remove("range");
		assertInitOptionFailed(decorator, "option range must be numerical");
	}

}
