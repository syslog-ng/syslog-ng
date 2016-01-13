package org.syslog_ng.elasticsearch_v2.client;

@SuppressWarnings("serial")
public class UnknownESClientModeException extends Exception {
	public UnknownESClientModeException(String client_mode) {
		super("Unkown client_mode: " + client_mode);
	}
}
