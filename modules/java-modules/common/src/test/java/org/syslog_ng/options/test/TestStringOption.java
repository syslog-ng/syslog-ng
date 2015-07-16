package org.syslog_ng.options.test;

import org.junit.Before;
import org.junit.Test;
import org.syslog_ng.options.Option;
import org.syslog_ng.options.StringOption;

import static org.junit.Assert.*;

public class TestStringOption extends TestOption {


	@Before
	public void setUp() throws Exception {
		super.setUp();
		options.put("cluster", "syslog-ng");
		options.put("port", "9300");
		options.put("server", "localhost remote_host");
	}

	@Test
	public void testExisting() {
		Option stringOption = new StringOption(owner, "cluster");
		assertInitOptionSuccess(stringOption);
		assertEquals("syslog-ng", stringOption.getValue());
	}

	@Test
	public void testExistingWithDefaultValue() {
		Option stringOption = new StringOption(owner, "cluster", "something else");
		assertInitOptionSuccess(stringOption);
		assertEquals("syslog-ng", stringOption.getValue());
	}

	@Test
	public void testNonExisting() {
		Option stringOption = new StringOption(owner, "badcluster");
		assertInitOptionSuccess(stringOption);
		assertEquals(null, stringOption.getValue());
	}

	@Test
	public void testNonExistingWithDefaultValue() {
		Option stringOption = new StringOption(owner, "badcluster", "something else");
		assertInitOptionSuccess(stringOption);
		assertEquals("something else", stringOption.getValue());
	}

	@Test
	public void testWithValidInteger() {
		Option stringOption = new StringOption(owner, "port");
		assertInitOptionSuccess(stringOption);
		assertEquals(new Integer(9300), stringOption.getValueAsInteger());
	}

	@Test
	public void testWithInValidInteger() {
		Option stringOption = new StringOption(owner, "cluster");
		assertInitOptionSuccess(stringOption);
		assertEquals(null, stringOption.getValueAsInteger());
	}

	@Test
	public void testWithValidStringList() {
		Option stringOption = new StringOption(owner, "server");
		assertInitOptionSuccess(stringOption);
		assertArrayEquals(new String[] {"localhost", "remote_host"} , stringOption.getValueAsStringList(" "));
	}

	@Test
	public void testWithInValidStringList() {
		Option stringOption = new StringOption(owner, "badcluster");
		assertInitOptionSuccess(stringOption);
		assertArrayEquals(null, stringOption.getValueAsStringList(" "));
	}

	@Test
	public void testWithSingleValidStringList() {
		Option stringOption = new StringOption(owner, "cluster");
		assertInitOptionSuccess(stringOption);
		assertArrayEquals(new String[]{"syslog-ng"}, stringOption.getValueAsStringList(" "));
	}

}
