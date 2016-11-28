/*
 * Copyright (c) 2015 Balabit
 * Copyright (c) 2015 Viktor Juhasz <viktor.juhasz@balabit.com>
 * Copyright (c) 2016 Viktor Tusa <tusavik@gmail.com>
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

package org.syslog_ng.elasticsearch_http.messageprocessor;

import org.apache.log4j.Logger;
import io.searchbox.core.Index;
import io.searchbox.core.Bulk;
import io.searchbox.client.JestClient;
import org.syslog_ng.elasticsearch_http.ElasticSearchHTTPOptions;

import java.io.IOException;

public class ESHTTPBulkMessageProcessor extends ESHTTPMessageProcessor {

	private Bulk.Builder bulk;
	private int flushLimit;
	private int messageCounter;

	public ESHTTPBulkMessageProcessor(ElasticSearchHTTPOptions options, JestClient client) {
		super(options, client);
	}

	@Override
	public void init() {
		bulk = new Bulk.Builder();
		messageCounter = 0;
		flushLimit = options.getFlushLimit();
	}

	@Override
	public boolean send(Index index) {
		if (messageCounter >= flushLimit)
		{
			flush();
		}
		bulk = bulk.addAction(index);
		messageCounter++;
		return true;
	}

        @Override
	public void flush() {
		logger.debug("Flushing messages for ESHTTP destination");
		Bulk bulkActions = bulk.build();
		try {
                        client.execute(bulkActions);
                }
                catch (IOException e)
                {
                        logger.error(e.getMessage());
                }
		bulk = new Bulk.Builder();
		messageCounter = 0;
	}

}
