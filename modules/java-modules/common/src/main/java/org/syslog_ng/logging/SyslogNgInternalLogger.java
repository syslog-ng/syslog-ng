/*
 * Copyright (c) 2015 Balabit
 * Copyright (c) 2015 Viktor Juhasz <viktor.juhasz@balabit.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

package org.syslog_ng.logging;

import org.apache.log4j.AppenderSkeleton;
import org.apache.log4j.Level;
import org.apache.log4j.Logger;
import org.apache.log4j.spi.LoggingEvent;
import org.syslog_ng.InternalMessageSender;

public class SyslogNgInternalLogger extends AppenderSkeleton {

    public static final String NAME = "syslog-ng-internal";

    public static void register(Logger logger) {
        if (logger.getAppender(SyslogNgInternalLogger.NAME) == null) {
            logger.removeAllAppenders();
            logger.addAppender(new SyslogNgInternalLogger());
            logger.setLevel(SyslogNgInternalLogger.getLevel());
        }
    }

    public SyslogNgInternalLogger() {
        super();
        this.name = NAME;
    }

    @Override
    public void close() {
    }

    @Override
    public boolean requiresLayout() {
        return false;
    }

    @Override
    protected void append(LoggingEvent event) {

        String message = event.getMessage().toString();

        switch(event.getLevel().toInt()) {
        case Level.INFO_INT:
            InternalMessageSender.info(message);
            break;
        case Level.DEBUG_INT:
            InternalMessageSender.debug(message);
            break;
        case Level.ERROR_INT:
            InternalMessageSender.error(message);
            break;
        case Level.FATAL_INT:
            InternalMessageSender.fatal(message);
            break;
        case Level.WARN_INT:
            InternalMessageSender.warning(message);
            break;
        case Level.TRACE_INT:
            InternalMessageSender.debug(message);
            break;
        default:
            break;
        }
    }

    public static Level getLevel() {
        switch (InternalMessageSender.getLevel()) {
        case InternalMessageSender.LevelFatal:
            return Level.FATAL;
        case InternalMessageSender.LevelError:
            return Level.ERROR;
        case InternalMessageSender.LevelWarning:
            return Level.WARN;
        case InternalMessageSender.LevelNotice:
            return Level.INFO;
        case InternalMessageSender.LevelInfo:
            return Level.INFO;
        case InternalMessageSender.LevelDebug:
            return Level.DEBUG;
        }

        return Level.OFF;
    }
}
