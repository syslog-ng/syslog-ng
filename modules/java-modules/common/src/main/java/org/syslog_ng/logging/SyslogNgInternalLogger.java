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

import org.apache.logging.log4j.core.config.plugins.Plugin;
import org.apache.logging.log4j.core.config.plugins.PluginAttribute;
import org.apache.logging.log4j.core.config.plugins.PluginBuilderAttribute;
import org.apache.logging.log4j.core.config.plugins.PluginElement;
import org.apache.logging.log4j.core.config.plugins.PluginBuilderFactory;
import org.apache.logging.log4j.core.config.plugins.validation.constraints.Required;
import org.apache.logging.log4j.core.Appender;
import org.apache.logging.log4j.core.LogEvent;
import org.apache.logging.log4j.core.ErrorHandler;
import org.apache.logging.log4j.core.Filter;
import org.apache.logging.log4j.core.Layout;
import org.apache.logging.log4j.core.config.Property;
import org.apache.logging.log4j.Level;
import org.apache.logging.log4j.Logger;
import java.io.Serializable;


import org.syslog_ng.InternalMessageSender;


@Plugin(name = "SyslogNgInternalLogger", category = "Core", elementType = "appender", printObject = true)
public class SyslogNgInternalLogger implements Appender {

    public final String name;

    public static void register(Logger logger) {
    }

    public SyslogNgInternalLogger(String name) {
       this.name=name;
    }

    @Override
    public void append(LogEvent event) {

        String message = event.getMessage().getFormattedMessage();

        Level level = event.getLevel();

        if (level.equals(Level.INFO)) {
            InternalMessageSender.info(message);
        } else if (level.equals(Level.DEBUG)) {
            InternalMessageSender.debug(message);
        } else if (level.equals(Level.ERROR)) {
            InternalMessageSender.error(message);
        } else if (level.equals(Level.FATAL)) {
            InternalMessageSender.fatal(message);
        } else if (level.equals(Level.WARN)) {
            InternalMessageSender.warning(message);
        } else if (level.equals(Level.TRACE)) {
            InternalMessageSender.debug(message);
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

   /* Boilerplate for Appender */

   @Override public String getName() { return name; }
   @Override public ErrorHandler getHandler() { return null; }
   @Override public void setHandler(ErrorHandler handler) { }
   @Override public boolean ignoreExceptions() { return false; }
   @Override public Layout<? extends Serializable> getLayout() { return null; }

   /* Boilerplate for LifeCycle (Appender extends LifeCycle) */

   @Override public boolean isStopped() { return true; }
   @Override public boolean isStarted() { return true; }
   @Override public void stop() {  }
   @Override public void start() {  }
   @Override public void initialize() {  }
   @Override public State getState() { return null; }

   @PluginBuilderFactory
   public static Builder newBuilder() {
       return new Builder();
   }
    
   public static class Builder implements org.apache.logging.log4j.core.util.Builder<SyslogNgInternalLogger> {

       @PluginBuilderAttribute
       @Required(message = "No name provided for SyslogNgInternalLogger")
       private String name;
    
       public Builder setName(final String name) {
           this.name = name;
           return this;
       }
    
       @Override
       public SyslogNgInternalLogger build() {
           return new SyslogNgInternalLogger(name);
       }
    }
}
