"""
Copyright (c) 2022 novaSOC

Use of this source code is governed by an MIT-style
license that can be found in the LICENSE file or at
https://opensource.org/licenses/MIT.

Original development by Dan Elder (delder@novacoast.com)
Syslog-ng python fetcher for Hypr API (https://apidocs.hypr.com)

Additional documentation available at:
https://www.syslog-ng.com/technical-documents/doc/syslog-ng-open-source-edition/3.36/administration-guide/26#TOPIC-1768583
"""

import time
import json
import logging
import base64
import re
from datetime import datetime, timezone
import requests

import syslogng

class HyprAuditSource(syslogng.LogFetcher):
    """
    class for python syslog-ng log fetcher
    """

    def init(self, options):
        """
        Initialize Hypr driver
        (optional for Python LogFetcher)
        """

        # Ensure rp_app_id parameter is defined
        if "rp_app_id" in options:
            self.rp_app_id = options["rp_app_id"]
        else:
            print("Missing rp_app_id configuration option for Hypr driver")
            self.exit = True
            return False

        # Initialize logger for driver
        self.logger = logging.getLogger('Hypr-' + self.rp_app_id)
        stream_logger = logging.StreamHandler()

        # Standard log format
        log_format = " - ".join((
            "Hypr-%s" % self.rp_app_id,
            "%(levelname)s",
            "%(message)s"
        ))

        # Configure logging for standard log format
        formatter = logging.Formatter(log_format)
        stream_logger.setFormatter(formatter)
        self.logger.addHandler(stream_logger)

        # Check for valid log level and set loggers
        if "log_level" in options:
            if options["log_level"].upper() == "DEBUG":
                self.logger.setLevel(logging.DEBUG)
            elif options["log_level"].upper() == "INFO":
                self.logger.setLevel(logging.INFO)
            elif options["log_level"].upper() == "WARN":
                self.logger.setLevel(logging.WARNING)
            elif options["log_level"].upper() == "ERROR":
                self.logger.setLevel(logging.ERROR)
            elif options["log_level"].upper() == "CRIT":
                self.logger.setLevel(logging.CRITICAL)
        else:
            self.logger.setLevel(logging.INFO)
            self.logger.warning("Invalid or no log level specified, setting log level to INFO")

        self.logger.info("Starting Hypr API fetch driver for %s", self.rp_app_id)

        # Initialize empty array of log messages
        self.logs = []

        # Start with last search window end time or ignore
        ignore_persistence = False

        # Ensure url parameter is defined
        if "url" in options:
            self.url = options["url"]
            self.logger.debug("Initializing Hypr %s syslog-ng driver against URL %s" \
                , self.rp_app_id, self.url)
        else:
            self.logger.error("Missing url configuration option for %s", self.rp_app_id)
            self.exit = True
            return False

        # Ensure bearer_token parameter is defined
        if "bearer_token" in options:
            self.token = options["bearer_token"]
            self.logger.debug("Initializing Hypr syslog-ng driver with bearer_token %s for %s", \
                self.token, self.rp_app_id)
        else:
            self.logger.error("Missing bearer_token configuration option for %s", self.rp_app_id)
            self.exit = True
            return False

        # Set page size if defined
        self.page_size = 100
        if "page_size" in options:
            self.page_size = options["page_size"]
            self.logger.debug("Initializing Hypr syslog-ng driver with pageSize %s for %s" \
                , self.page_size, self.rp_app_id)

        # Set max_performance if defined
        self.max_performance = False
        if "max_performance" in options:
            if options["max_performance"].lower() == "true":
                self.max_performance = True

        # Default time to go back is 4 hours
        initial_hours = 4
        if "initial_hours" in options:

            # If r is set as an initial_hours value, ignore persistence
            if "r" in options["initial_hours"]:
                self.logger.info("Disabling persistence due to special initial_hours setting (%s)", options["initial_hours"])
                ignore_persistence = True
            # Extract decimal value from initial_hours setting
            try:
                initial_hours = int(re.search('.*?(\d+).*', options["initial_hours"]).group(1))
            except Exception as ex:
                self.logger.error("Invalid value (%s) for initial_hours : %s", options["initial_hours"], ex)

            self.logger.debug("Initializing Hypr syslog-ng driver with initial_hours %i hours ago for %s" \
                , initial_hours, self.rp_app_id)

        # Convert initial_hours to milliseconds and subtract from current time
        self.start_time = int(time.time()* 1000) - (initial_hours * 3600000)

        # Setup persist_name with defined persist_name or use URL and rpAppId if none specified
        try:
            self.persist_name
        except:
            self.persist_name = "hypr-%s-%s" % (self.url, self.rp_app_id)

        # Initialize persistence
        self.logger.debug("Initializing Hypr syslog-ng driver with persist_name %s", \
                self.persist_name)
        self.persist = syslogng.Persist(persist_name=self.persist_name, defaults={"last_read": self.start_time})

        # Convert persistence timestamp and reset if invalid data is in persistence
        try:
            last_run = datetime.utcfromtimestamp(int(self.persist["last_read"])/1000)
            self.logger.debug("Read %s from persistence as last run time", last_run)
        except:
            self.logger.error("Invalid last_read detected in persistence, resetting to %s hours ago", initial_hours)
            ignore_persistence = True

        # Ignore persistence if configured
        if not ignore_persistence:
            # Start search at last fetch window end time
            self.start_time = int(self.persist["last_read"])

        self.logger.debug("Driver initialization complete, fetch window starts at %i (%s)", \
            self.start_time, datetime.utcfromtimestamp(self.start_time/1000))

        return True


    def parse_log(self, log):
        """
        Parse an event into a syslog LogMessage
        (custom function for message parsing)
        """

        # Do not parse message as json for higher performance if set
        if self.max_performance is True:

            # Convert python dict to json for message
            msg = syslogng.LogMessage(str(log))
            program = "Hypr-" + self.rp_app_id
            msg["PROGRAM"] = program.replace(" ", "-")
            return msg

        # Parse everything we can out of the message
        else:

            # Convert python dict to json for message
            msg = syslogng.LogMessage("%s" % json.dumps(log))

            # Try to get program/rpAppId from message
            if "rpAppId" in log:
                msg["PROGRAM"] = log['rpAppId']

            # Try to get timestamp information from message
            if "eventTimeInUTC" in log:
                try:
                    timestamp = datetime.fromtimestamp(int(log['eventTimeInUTC'] / 1000.0), \
                        tz=timezone.utc)
                    msg.set_timestamp(timestamp)

                except Exception as e_all:
                    self.logger.debug("Unable to convert %s to timestamp from %s : %s", \
                        log['eventTimeInUTC'], self.rp_app_id, e_all)

            # Return LogMessage
            return msg


    def fetch(self):
        """
        Return a log message by pulling from the internal list or pulling from the Hypr API
        (mandatory function for Python LogFetcher)
        """

        # Retrieve log messages from memory if present
        if self.logs:
            log = self.logs.pop(0)
            msg = self.parse_log(log)
            return syslogng.LogFetcher.FETCH_SUCCESS, msg

        # Get current time in milliseconds since epoch
        self.end_time = int(time.time() * 1000)

        # Retrieve log messages from Hypr API
        subscription_url = self.url + "/cc/api/versioned/audit/search?" + \
            "rpAppId=" + self.rp_app_id + \
                "&startTSUTC=" + str(self.start_time) + \
                    "&endTSUTC=" + str(self.end_time) + \
                        "&pageSize=" + str(self.page_size)

        headers =  {"Content-Type":"application/application-json", \
            "Accept":"application/json", "Authorization": "Bearer %s" \
            % self.bearer_token}

        # Perform HTTP request
        response = requests.get(subscription_url, headers=headers)

        # Ingore 504 errors
        if response.status_code == 504:
            self.logger.info("Gateway Timeout from Hypr")
            return syslogng.LogFetcher.FETCH_TRY_AGAIN, "Gateway Timeout from Hypr"

        # If the API call returns successfully, parse the retrieved json data
        if response.status_code == 200:

            try:
                result = response.json()
                total_records = result['metadata']['totalRecords']
                total_pages = result['metadata']['totalPages']
                current_page = result['metadata']['currentPage']

                # Set internal log buffer to all returned events
                self.logs = result['data']
                self.logger.debug("%i events available from Hypr API %s fetch" \
                    , total_records, self.rp_app_id)
            except Exception as e_all:
                return syslogng.LogFetcher.FETCH_ERROR, "%s - %s access failure : %s\n%s", \
                    self.url, self.rp_app_id, e_all, response.text

            # If there are more pages of events to process
            while current_page < total_pages:

                # increment page counter
                current_page = current_page + 1

                # Retrieve log messages from Hypr API
                subscription_url = self.url + "/cc/api/versioned/audit/search?" + \
                    "rpAppId=" + self.rp_app_id + \
                        "&startTSUTC=" + str(self.start_time) + \
                            "&endTSUTC=" + str(self.end_time) + \
                                "&pageSize=" + str(self.page_size) + \
                                    "&pageNumber=" + str(current_page)

                headers =  {"Content-Type":"application/application-json", \
                    "Accept":"application/json", "Authorization": "Bearer %s" \
                    % self.bearer_token}

                # Perform HTTP request
                response = requests.get(subscription_url, headers=headers)

                # If we were successful, parse the json result
                if response.status_code == 200:
                    try:
                        result = response.json()
                    except Exception as e_all:
                        return syslogng.LogFetcher.FETCH_ERROR, "%s - %s access failure : %s\n%s", \
                            self.url, self.rp_app_id, e_all, response.text

                    # Add each event to our internal logs list
                    for entry in result['data']:
                        self.logs.append(entry)

                # If something went wrong with the query
                else:
                    return syslogng.LogFetcher.FETCH_ERROR, "%s - %s access failure : %s\n%s", \
                        self.url, self.rp_app_id, e_all, response.text

            # Set start time to end time
            self.start_time = self.end_time

            # If there are new logs
            if self.logs:

                # Process each log message
                log = self.logs.pop(0)
                msg = self.parse_log(log)
                return syslogng.LogFetcher.FETCH_SUCCESS, msg

            # If there aren't new logs
            self.persist["last_read"] = self.end_time
            return syslogng.LogFetcher.FETCH_NO_DATA, "No new Hypr events available"

        # If the bearer token is invalid
        if response.status_code == 403:
            self.logger.error("Bearer token invalid for %s", self.rp_app_id)
            return syslogng.LogFetcher.FETCH_ERROR, "Hypr API bearer token expired or invalid - %s", \
                response.text

        # If the response code isn't 504 or 200 (or isn't even set)
        return syslogng.LogFetcher.FETCH_ERROR, "%s - %s access failure : %s\n%s", \
            self.url, self.rp_app_id, e_all, response.text


    def open(self):
        """
        Retrieve bearer token for Hypr API
        (optional for Python LogFetcher)
        """
        self.logger.info("Retreiving bearer token for Hypr API for %s", self.rp_app_id)
        try:
            self.bearer_token = base64.b64decode(self.token).decode("utf-8")
        except Exception as e_all:
            self.logger.error("Unable to decode bearer_token %s : %s", self.token, e_all)
            self.exit = True
            return False

        return True


    def deinit(self):
        """
        Driver de-initialization routine
        (optional for Python LogFetcher)
        """

        # Only update persistence if all logs in memory were processed
        if len(self.logs) > 0:
            self.logger.warning("Deinitializing with %i %s events in memory buffer", \
                len(self.logs), self.rp_app_id)
        else:
            self.persist["last_read"] = self.end_time
