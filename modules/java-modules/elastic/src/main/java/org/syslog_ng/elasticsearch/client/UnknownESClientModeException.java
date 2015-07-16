package org.syslog_ng.elasticsearch.client;

@SuppressWarnings("serial")
public class UnknownESClientModeException extends Exception {
	public UnknownESClientModeException(String client_mode) {
		super("Unkown client_mode: " + client_mode);
	}
}
