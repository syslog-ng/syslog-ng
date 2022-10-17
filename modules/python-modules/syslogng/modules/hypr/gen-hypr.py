#!/usr/bin/env python3

"""
Copyright (c) 2022 novaSOC

Use of this source code is governed by an MIT-style
license that can be found in the LICENSE file or at
https://opensource.org/licenses/MIT.

Original development by Dan Elder (delder@novacoast.com)
"""

import argparse
import os
import base64
import sys
import requests


def sanitize(variable):
    """Filter out characters that would break syslog-ng configuration"""
    return variable.translate({ord(i): None for i in '\'\"\r'})


# Global parser for access by functions
parser = argparse.ArgumentParser(description='This is a simple tool \
    to generate Hypr API sources for available applications')
parser.add_argument('--debug', help='Enable debug mode', action="store_true")
args = parser.parse_args()

# Debug log file always available even if debugging is disabled
DEBUG_LOG = "/tmp/hypr-scl.log"
debug = open(DEBUG_LOG,"a")

# Enable debug logging if requested
if args.debug:
    debug.write("Initializing debug log\n")

# Capture environment variables for syslog-ng configuration
confgen_url = sanitize(os.environ.get('confgen_url', ""))
confgen_bearer_token = sanitize(os.environ.get('confgen_bearer_token', ""))
confgen_page_size = sanitize(os.environ.get('confgen_page_size',"100"))
confgen_initial_hours = sanitize(os.environ.get('confgen_initial_hours', "4"))
confgen_sleep = sanitize(os.environ.get('confgen_sleep', "60"))
confgen_log_level = sanitize(os.environ.get('confgen_log_level', "INFO"))
confgen_application_skiplist = os.environ.get('confgen_application_skiplist', \
    "HYPRDefaultApplication,HYPRDefaultWorkstationApplication")
confgen_persist_name = sanitize(os.environ.get('confgen_persist_name', ""))
confgen_max_performance = sanitize(os.environ.get('confgen_max_performance', "False"))

# Log environment variables
if args.debug:
    debug.write("confgen_url : %s\n" % confgen_url)
    debug.write("confgen_bearer_token : %s\n" %  confgen_bearer_token)
    debug.write("confgen_page_size : %s\n" %  confgen_page_size)
    debug.write("confgen_initial_hours : %s\n" %  confgen_initial_hours)
    debug.write("confgen_sleep : %s\n" %  confgen_sleep)
    debug.write("confgen_log_level : %s\n" %  confgen_log_level)
    debug.write("confgen_application_skiplist : %s\n" %  confgen_application_skiplist)
    debug.write("confgen_persist_name : %s\n" %  confgen_persist_name)
    debug.write("confgen_max_performance : %s\n" %  confgen_max_performance)

# Deobfuscate
try:
    bearer_token = base64.b64decode(confgen_bearer_token).decode("utf-8")
except Exception as e_all:
    debug.write("Unable to decode bearer_token %s : %s\n" % (confgen_bearer_token, e_all))
    sys.exit(1)

# Start with empty list of applications
applications = []

# Get list of available applications from Hypr through API
get_url = confgen_url + "/cc/api/application"
headers =  {"Content-Type":"application/application-json",
"Authorization":"Bearer %s" % bearer_token}
response = requests.get(get_url, headers=headers)

# Check for valid response
if response.status_code != 200:
    debug.write("Hypr API fetch returned:\n%s" % response.text)
    debug.write("Unable to retrieve applications from Hypr : HTTP %s\n", response.status_code)
    debug.write("Decoded bearer_token is %s\n" % bearer_token)
    debug.write("Requested URL is %s\n" % get_url)
    sys.exit(1)

# Loop through json response to pull all appIDs
try:
    result = response.json()

    # Add appID to class list
    for entry in result:
        if entry['appID'] not in confgen_application_skiplist:
            applications.append(entry['appID'])
            if args.debug:
                debug.write("Adding %s to monitored Hypr applications\n" % entry['appID'])

except Exception as e_all:
    debug.write("Unable to retrieve list of applications from Hypr : %s\n" % e_all)
    sys.exit(1)

# Setup source for each Hypr application
sources = ""
for application in applications:
    sources = sources + """
    python-fetcher(
        class("hypr.Hypr")
        options(
            "url","%s"
            "rp_app_id","%s"
            "bearer_token","%s"
            "page_size","%s"
            "initial_hours","%s"
            "log_level","%s"
            "max_performance","%s"
        )
        flags(no-parse)
        persist-name(%s-%s)
        fetch-no-data-delay(%s)
    );
""" % (confgen_url, application, confgen_bearer_token, confgen_page_size, \
    confgen_initial_hours, confgen_log_level, confgen_max_performance, \
    confgen_persist_name, application, confgen_sleep)

if args.debug:
    debug.write("Final configuration is:\n\n %s" % sources)

print(sources)
