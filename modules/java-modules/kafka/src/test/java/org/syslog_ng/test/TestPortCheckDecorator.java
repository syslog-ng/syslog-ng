package org.syslog_ng.test;

import org.junit.Before;
import org.junit.Test;
import org.syslog_ng.options.Option;
import org.syslog_ng.options.PortCheckDecorator;
import org.syslog_ng.options.StringOption;

public class TestPortCheckDecorator extends TestOption {
	Option stringOption;
	Option decorator;

	@Before
	public void setUp() throws Exception {
		super.setUp();
		stringOption = new StringOption(owner, "port");
		decorator = new PortCheckDecorator(stringOption);
	}

	@Test
	public void test() {
		for (int i = 0; i < 65536; i++) {
			options.put("port", new Integer(i).toString());
			assertInitOptionSuccess(decorator);
		}
	}

}
