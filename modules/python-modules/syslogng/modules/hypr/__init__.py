#############################################################################
# Copyright (c) 2022 novaSOC
# Copyright (c) 2022 Balazs Scheidler <bazsi77@gmail.com>
# Copyright (c) 2023 Attila Szakacs
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as published
# by the Free Software Foundation, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
# As an additional exemption you are allowed to compile & link against the
# OpenSSL libraries as published by the OpenSSL project. See the file
# COPYING for details.
#
#############################################################################
#
# This was the original license on the code taken from:
#     https://github.com/danelder/syslogng-hypr
#
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

try:
    import requests
except ImportError:
    requests = None

import syslogng


class HyprAuditSource(syslogng.LogFetcher):
    """
    class for python syslog-ng log fetcher
    """

    # mandatory settings
    url = None
    bearer_token = None
    rp_app_id = None
    persist_name = None

    # optional settings
    page_size = 100
    initial_hours = 4

    # state
    start_time = 0
    end_time = 0

    #
    logger = logging.getLogger('hypr')
    persist = None

    def init(self, options):
        """
        Initialize Hypr driver
        (optional for Python LogFetcher)
        """

        if requests is None:
            self.logger.error("Unable to start HyprAuditSource class, the required Python dependency of `requests` is missing")
            return False

        # Ensure rp_app_id parameter is defined
        if "rp_app_id" in options:
            self.rp_app_id = options["rp_app_id"]
        else:
            self.logger.error("Missing rp_app_id configuration option for Hypr driver")
            return False

        # Initialize rp_app_id specific logger
        self.logger = logging.getLogger('hypr.' + self.rp_app_id)
        if "log_level" in options:
            self.logger.setLevel(options["log_level"])

        self.logger.info("Starting Hypr API fetch driver for %s", self.rp_app_id)

        # Initialize empty array of log messages
        self.logs = []

        # Ensure url parameter is defined
        if "url" in options:
            self.url = options["url"]
            self.logger.debug("Initializing Hypr %s syslog-ng driver against URL %s", self.rp_app_id, self.url)
        else:
            self.logger.error("Missing url configuration option for %s", self.rp_app_id)
            return False

        # Ensure bearer_token parameter is defined
        if "bearer_token" in options:
            self.token = options["bearer_token"]
            self.logger.debug("Initializing Hypr syslog-ng driver with bearer_token %s for %s",
                              self.token, self.rp_app_id)
        else:
            self.logger.error("Missing bearer_token configuration option for %s", self.rp_app_id)
            return False

        # Set page size if defined
        self.page_size = 100
        if "page_size" in options:
            self.page_size = options["page_size"]
            self.logger.debug("Initializing Hypr syslog-ng driver with pageSize %s for %s",
                              self.page_size, self.rp_app_id)

        # Default time to go back is 4 hours
        initial_hours = 4
        if "initial_hours" in options:
            initial_hours = options["initial_hours"]
            self.logger.debug("Initializing Hypr syslog-ng driver with initial_hours %i hours ago for %s",
                              initial_hours, self.rp_app_id)

        # Start with last search window end time or ignore
        ignore_persistence = False
        if "ignore_persistence" in options:
            ignore_persistence = options["ignore_persistence"]
            self.logger.debug("Initializing Hypr syslog-ng driver with %spersistence for %s",
                              "ignoring " if ignore_persistence else "", self.rp_app_id)

        # Convert initial_hours to milliseconds and subtract from current time
        self.start_time = int(time.time() * 1000) - (initial_hours * 3600000)

        # Setup persist_name with defined persist_name or use URL and rpAppId if none specified
        if self.persist_name is None:
            self.persist_name = "hypr-%s-%s" % (self.url, self.rp_app_id)

        # Initialize persistence
        self.logger.debug("Initializing Hypr syslog-ng driver with persist_name %s", self.persist_name)
        self.persist = syslogng.Persist(persist_name=self.persist_name, defaults={"last_read": self.start_time})

        # Initialize ack tracing
        self.ack_tracker = syslogng.ConsecutiveAckTracker(ack_callback=self.message_acked)

        # Convert persistence timestamp and reset if invalid data is in persistence
        try:
            last_run = datetime.utcfromtimestamp(int(self.persist["last_read"]) / 1000)
            self.logger.debug("Read %s from persistence as last run time", last_run)
        except (OverflowError, OSError):
            self.logger.error("Invalid last_read detected in persistence, resetting to %s hours ago and ignoring "
                              "persistence for %s", initial_hours, self.rp_app_id)
            ignore_persistence = True

        # Ignore persistence if configured
        if not ignore_persistence:
            # Start search at last fetch window end time
            self.start_time = int(self.persist["last_read"])

        self.set_transport_name("hypr-api")
        self.logger.debug("Driver initialization complete, fetch window starts at %i (%s)",
                          self.start_time, datetime.utcfromtimestamp(self.start_time / 1000))

        return True

    def message_acked(self, acked_message_bookmark):
        self.persist["last_read"] = acked_message_bookmark

    def extract_nvpairs(self, msg, log, prefix, parent_name=""):
        for name, value in log.items():
            new_name = "%s.%s" % (parent_name, name) if parent_name else name
            value_type = type(value)

            if value_type == dict:
                self.extract_nvpairs(msg, value, prefix, new_name)
            elif ((value_type in (str, bytes, int, float, bool, datetime)) or
                  (value is None) or
                  (value_type == list and all(type(element) in (str, bytes) for element in value))):
                msg[prefix + new_name] = value
            else:
                msg[prefix + new_name] = str(value)

    def parse_log(self, log):
        """
        Parse an event into a syslog LogMessage
        (custom function for message parsing)
        """

        def simple_parse_log(log, rp_app_id):
            msg = syslogng.LogMessage("%s" % json.dumps(log))
            program = "Hypr-" + rp_app_id
            msg["PROGRAM"] = program.replace(" ", "-")

            return msg

        # Do not extract fields, if flags(no-parse) is set
        if not self.flags.get("parse", True):
            return simple_parse_log(log, self.rp_app_id)

        # Parse everything we can out of the message
        else:
            try:
                msg = syslogng.LogMessage(log["message"])
            except KeyError:
                self.logger.debug("'message' field is missing, dumping JSON data to ${MESSAGE} from %s", self.rp_app_id)
                return simple_parse_log(log, self.rp_app_id)

            # Try to get program/rpAppId from message
            if "rpAppId" in log:
                msg["PROGRAM"] = log['rpAppId']

            # Try to get timestamp information from message
            if "eventTimeInUTC" in log:
                try:
                    timestamp = datetime.fromtimestamp(int(log['eventTimeInUTC'] / 1000.0), tz=timezone.utc)
                    msg.set_timestamp(timestamp)

                except Exception as e_all:
                    self.logger.debug("Unable to convert %s to timestamp from %s : %s",
                                      log['eventTimeInUTC'], self.rp_app_id, e_all)

            self.extract_nvpairs(msg=msg, log=log, prefix=".hypr.")

        return msg

    def fetch(self):
        """
        Return a log message by pulling from the internal list or pulling from the Hypr API
        (mandatory function for Python LogFetcher)
        """

        # Retrieve log messages from memory if present
        if self.logs:
            log = self.logs.pop(0)

            if "eventTimeInUTC" not in log:
                self.logger.error("'eventTimeInUTC' field is missing, skipping message: %s" % str(log))
                return self.ERROR, None

            msg = self.parse_log(log)
            msg.set_bookmark(log["eventTimeInUTC"])
            return self.SUCCESS, msg

        # Get current time in milliseconds since epoch
        self.end_time = int(time.time() * 1000)

        # Retrieve log messages from Hypr API
        subscription_url = (self.url + "/cc/api/versioned/audit/search?" +
                            "rpAppId=" + self.rp_app_id +
                            "&startTSUTC=" + str(self.start_time) +
                            "&endTSUTC=" + str(self.end_time) +
                            "&pageSize=" + str(self.page_size) +
                            "&orderBy=eventTimeInUTC" +
                            "&sortDir=asc")

        headers = {"Content-Type": "application/application-json",
                   "Accept": "application/json",
                   "Authorization": "Bearer %s" % self.bearer_token}

        # Perform HTTP request
        response = requests.get(subscription_url, headers=headers)

        # Ingore 504 errors
        if response.status_code == 504:
            self.logger.info("Gateway Timeout from Hypr")
            return self.TRY_AGAIN, None

        # If the API call returns successfully, parse the retrieved json data
        if response.status_code == 200:

            try:
                # This JSON parsing is unfortunately necessary, as we get a batch of messages in one response.
                # There is no way to limit the level of the parsing, so we get each message as a python dict.
                result = response.json()
                total_records = result['metadata']['totalRecords']
                total_pages = result['metadata']['totalPages']
                current_page = result['metadata']['currentPage']

                # Set internal log buffer to all returned events
                self.logs = result['data']
                self.logger.debug("%i events available from Hypr API %s fetch", total_records, self.rp_app_id)
            except Exception as e_all:
                self.logger.error("%s - %s access failure : %s\n%s" % (self.url, self.rp_app_id, e_all, response.text))
                return self.ERROR, None

            # If there are more pages of events to process
            while current_page < total_pages:

                # increment page counter
                current_page = current_page + 1

                # Retrieve log messages from Hypr API
                subscription_url = (self.url + "/cc/api/versioned/audit/search?" +
                                    "rpAppId=" + self.rp_app_id +
                                    "&startTSUTC=" + str(self.start_time) +
                                    "&endTSUTC=" + str(self.end_time) +
                                    "&pageSize=" + str(self.page_size) +
                                    "&pageNumber=" + str(current_page))

                headers = {"Content-Type": "application/application-json",
                           "Accept": "application/json",
                           "Authorization": "Bearer %s" % self.bearer_token}

                # Perform HTTP request
                response = requests.get(subscription_url, headers=headers)

                # If we were successful, parse the json result
                if response.status_code == 200:
                    try:
                        result = response.json()
                    except Exception as e_all:
                        self.logger.error("%s - %s access failure : %s\n%s" % (self.url, self.rp_app_id, e_all, response.text))
                        return self.ERROR, None

                    # Add each event to our internal logs list
                    for entry in result['data']:
                        self.logs.append(entry)

                # If something went wrong with the query
                else:
                    self.logger.error("%s - %s access failure : %s\n%s" % (self.url, self.rp_app_id, response.status_code, response.text))
                    return self.ERROR, None

            # Set start time to end time
            self.start_time = self.end_time

            # If there are new logs
            if self.logs:
                return self.fetch()

            # If there aren't new logs
            self.logger.debug("No new Hypr events available")
            return self.NO_DATA, None

        # If the bearer token is invalid
        if response.status_code == 403:
            self.logger.error("Bearer token invalid for %s [%s]" % (self.rp_app_id, response.text))
            return self.ERROR, None

        # If the response code isn't 504 or 200 (or isn't even set)
        self.logger.error("%s - %s access failure : %s\n%s" % (self.url, self.rp_app_id, response.status_code, response.text))
        return self.ERROR, None

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
            return False

        return True


class HyprError(Exception):
    pass


def _hypr_config_generator(args):
    if requests is None:
        raise HyprError("Unable to run confgen for hypr-audit-trail(), the required Python dependency `requests` is missing")
    logger = logging.getLogger("Hypr-Confgen")

    def sanitize(variable):
        """Filter out characters that would break syslog-ng configuration"""
        return variable.translate({ord(i): None for i in '\'\"\r'})

    # Capture environment variables for syslog-ng configuration
    url = sanitize(args.get('url', ""))
    bearer_token = sanitize(args.get('bearer_token', ""))
    page_size = args.get('page_size', "100")
    initial_hours = args.get('initial_hours', "4")
    fetch_no_data_delay = args.get('fetch_no_data_delay', "60")
    log_level = sanitize(args.get('log_level', "INFO"))
    application_skiplist = sanitize(args.get('application_skiplist',
                                    "'HYPRDefaultApplication' 'HYPRDefaultWorkstationApplication'")).replace(",", " ").split()
    ignore_persistence = args.get('ignore_persistence', 'no')
    persist_name = sanitize(args.get('persist_name', "hypr-%s" % url))
    flags = sanitize(args.get('flags', ''))

    # Log environment variables
    logger.debug("url : %s" % url)
    logger.debug("bearer_token : %s" % bearer_token)
    logger.debug("page_size : %s" % page_size)
    logger.debug("initial_hours : %s" % initial_hours)
    logger.debug("fetch_no_data_delay : %s" % fetch_no_data_delay)
    logger.debug("log_level : %s" % log_level)
    logger.debug("application_skiplist : %s" % application_skiplist)
    logger.debug("ignore_persistence: %s" % ignore_persistence)
    logger.debug("persist_name : %s" % persist_name)
    logger.debug("flags : %s" % flags)

    # Deobfuscate
    try:
        decoded_bearer_token = base64.b64decode(bearer_token).decode("utf-8")
    except Exception as e_all:
        raise ValueError("Unable to decode bearer_token %s : %s\n" % (bearer_token, e_all))

    # Start with empty list of applications
    applications = []

    # Get list of available applications from Hypr through API
    get_url = url + "/cc/api/application"
    headers = {
        "Content-Type": "application/application-json",
        "Authorization": "Bearer %s" % decoded_bearer_token
    }

    response = requests.get(get_url, headers=headers)

    # Check for valid response
    if response.status_code != 200:
        logger.debug("Hypr API fetch returned:%s" % response.text)
        logger.debug("Unable to retrieve applications from Hypr : HTTP %s" % response.status_code)
        logger.debug("Decoded bearer_token is %s" % decoded_bearer_token)
        logger.debug("Requested URL is %s" % get_url)
        raise HyprError("Unable to process hypr() source, HTTP error while retrieving application list: HTTP {} [{}]".format(response.status_code, response.text))

    # Loop through json response to pull all appIDs
    try:
        result = response.json()

        # Add appID to class list
        for entry in result:
            if entry['appID'] not in application_skiplist:
                applications.append(entry['appID'])
                logger.debug("Adding %s to monitored Hypr applications" % entry['appID'])

    except Exception as e_all:
        raise HyprError("Unable to retrieve list of applications from Hypr : %s" % e_all)

    # Setup source for each Hypr application
    sources = ""
    for application in applications:
        sources = sources + """
        python-fetcher(
            class("syslogng.modules.hypr.HyprAuditSource")
            options(
                "url" => "%s"
                "rp_app_id" => "%s"
                "bearer_token" => "%s"
                "page_size" => %s
                "initial_hours" => %s
                "ignore_persistence" => %s
                "log_level" => "%s"
            )
            flags(%s)
            persist-name("%s-%s")
            fetch-no-data-delay(%s)
        );
    """ % (url, application, bearer_token, page_size,
           initial_hours, ignore_persistence, log_level, flags,
           persist_name, application, fetch_no_data_delay)

    logger.debug("Final configuration is: %s" % sources)

    return sources


def register_hypr_config_generator():
    syslogng.register_config_generator(context="source", name="hypr-audit-trail", config_generator=_hypr_config_generator)
