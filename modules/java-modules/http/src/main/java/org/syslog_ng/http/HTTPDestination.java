/*
 * Copyright (c) 2015 Balabit
 * Copyright (c) 2015 Adam Arsenault <adam.arsenault@balabit.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

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


public class HTTPDestination extends StructuredLogDestination {
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
        return String.format("HTTPDestination,%s", options.getURLTemplate());
    }


    @Override
    public boolean init() {
        try {
            options.init();
        } catch (InvalidOptionException e) {
            logger.error(e);
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
    public int send(LogMessage msg) {
        URL url;
        int responseCode = 0;
        String message;
        try {
            url = new URL(options.getURLTemplate().getResolvedString(msg, getTemplateOptionsHandle(), LogTemplate.LTZ_SEND));
            message = options.getMessageTemplate().getResolvedString(msg, getTemplateOptionsHandle(), LogTemplate.LTZ_SEND);
            HttpURLConnection connection = (HttpURLConnection) url.openConnection();
            connection.setRequestMethod(options.getMethod());
            connection.setRequestProperty("content-length", Integer.toString(message.length()));
            connection.setDoOutput(true);
            try (OutputStreamWriter osw = new OutputStreamWriter(connection.getOutputStream())) {
                osw.write(message);
                osw.close();
            } catch (IOException e) {
                logger.error("error in writing message.");
                return ERROR;
            }
            responseCode = connection.getResponseCode();
            if (isHTTPResponseError(responseCode)) {
                logger.error("HTTP response code error: " + responseCode);
                return ERROR;
            }
        } catch (IOException | SecurityException | IllegalStateException e) {
            logger.debug("error in writing message." +
                    (responseCode != 0 ? "Response code is " + responseCode : ""));
            return ERROR;
        }
        return SUCCESS;
    }

    private static boolean isHTTPResponseError(int responseCode) {
        return responseCode >= 400 && responseCode <= 599;
    }

    @Override
    public int flush() {
        return SUCCESS;
    }

    @Override
    public void close() {
    }

    @Override
    public void deinit() {
    }

}
