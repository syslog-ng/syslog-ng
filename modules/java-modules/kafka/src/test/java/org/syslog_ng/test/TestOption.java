package org.syslog_ng.test;

import static org.junit.Assert.*;

import java.util.Hashtable;

import org.syslog_ng.LogDestination;
import org.syslog_ng.options.Option;
import org.syslog_ng.options.InvalidOptionException;

public class TestOption {
	public LogDestination owner;
	public Hashtable<String, String> options;

	public void setUp() throws Exception {
		options = new Hashtable<String, String>();
		owner = new MockLogDestination(options);
	}


	public void assertInitOptionSuccess(Option option) {
		try {
			option.validate();
		}
		catch (InvalidOptionException e) {
			throw new AssertionError("Initialization failed: " + e.getMessage());
		}
	}

	public void assertInitOptionFailed(Option option) {
		assertInitOptionFailed(option, null);
	}

	public void assertInitOptionFailed(Option option, String expectedErrorMessage) {
		try {
			option.validate();
			throw new AssertionError("Initialization should be failed");
		}
		catch (InvalidOptionException e) {
			if (expectedErrorMessage != null) {
				assertEquals(expectedErrorMessage, e.getMessage().substring(0, expectedErrorMessage.length()));
			}
		}
	}
}
