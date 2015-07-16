package org.syslog_ng.options;

public class InvalidOptionException extends Exception {
	private static final long serialVersionUID = 1L;

	public InvalidOptionException(String message) {
		super(message);
	}

}
