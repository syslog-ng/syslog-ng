package org.syslog_ng.test;

import java.util.Hashtable;

import org.syslog_ng.LogDestination;

public class MockLogDestination extends LogDestination {

	private Hashtable<String, String> options;

	public MockLogDestination(Hashtable<String, String> options) {
		super(0);
		this.options = options;
	}

	@Override
	protected void close() {
		// TODO Auto-generated method stub

	}

	@Override
	public String getOption(String optionName) {
		return options.get(optionName);
	}

	@Override
	protected boolean isOpened() {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	protected boolean open() {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	protected void deinit() {
		// TODO Auto-generated method stub

	}

	@Override
	protected boolean init() {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	protected String getNameByUniqOptions() {
		// TODO Auto-generated method stub
		return "Dummy";
	}
}
