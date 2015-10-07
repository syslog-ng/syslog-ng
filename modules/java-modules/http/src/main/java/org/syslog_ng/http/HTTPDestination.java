package org.syslog_ng.http;

import org.syslog_ng.*;

import java.io.*;

import org.syslog_ng.options.InvalidOptionException;
import org.syslog_ng.logging.SyslogNgInternalLogger;
import org.apache.log4j.Level;
import org.apache.log4j.Logger;

import java.lang.SecurityException;
import java.lang.IllegalStateException;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;


public class HTTPDestination extends TextLogDestination {
    private URL url;
    Logger logger;
    private HTTPDestinationOptions options;

    public HTTPDestination(long handle) {
        super(handle);
        logger = Logger.getRootLogger();
        SyslogNgInternalLogger.register(logger);
        options = new HTTPDestinationOptions(this);
    }

    @Override
    public String getNameByUniqOptions() {
        return String.format("HTTPDestination,%s", options.getURL());
    }


    @Override
    public boolean init() {
        try {
            options.init();
        } catch (InvalidOptionException e) {
            return false;
        }
        try {
            this.url = new URL(options.getURL());
        } catch (MalformedURLException e) {
            logger.error("A properly formatted URL is a required option for this destination");
            return false;
        }
        return true;
    }

    @Override
    public boolean open() {
        return isOpened();
    }

    @Override
    public boolean isOpened() {
        return true;
    }


    @Override
    public boolean send(String message) {
        int responseCode = 0;
        try {
            HttpURLConnection connection = (HttpURLConnection) this.url.openConnection();
            connection.setRequestMethod(options.getMethod());
            connection.setRequestProperty("content-length", Integer.toString(message.length()));
            connection.setDoOutput(true);
            try (OutputStreamWriter osw = new OutputStreamWriter(connection.getOutputStream())) {
                osw.write(message);
                osw.close();
            } catch (IOException e) {
		logger.error("error in writing message.");
		return false;
            }
            responseCode = connection.getResponseCode();
        } catch (IOException | SecurityException | IllegalStateException e) {
            logger.debug("error in writing message." +
                    (responseCode != 0 ? "Response code is " + responseCode : ""));
            return false;
        }
        return true;
    }

    @Override
    public void onMessageQueueEmpty() {
    }

    @Override
    public void close() {
    }

    @Override
    public void deinit() {
    }


}

