# syslogng-hypr

## Purpose

This is a python syslogng.LogFetcher implementation designed to fetch events from Hypr via their [REST API](https://apidocs.hypr.com/) for ingestion by syslog-ng.

## Components

### hypr.py

This is the syslogng.LogFetcher implementation which can be configured as a standalone source in syslog-ng. To install and configure it, copy it to a location on the syslog-ng system and set the the parent directory int /etc/sysconfig/syslog-ng:

    PYTHONPATH="<path/to/parent/directory>"

As an example, if you store hypr.py in /opt/syslog-ng/etc/hypr.py you would add this to /etc/sysconfig/syslog-ng:

    PYTHONPATH="/opt/syslog-ng/etc"

To configure the source, certain parameters are required:

    source s_hypr {
      python-fetcher(
		  class("hypr.Hypr")
		  options(
			  "url","https://<domain>.hypr.com"
			  "rp_app_id" "<rpAppId>"
			  "bearer_token" "<base64 encoded bearer token>"
			  )
		  flags(no-parse)
	    );
    };

Additional options can also be specified:

    source s_hypr {
      python-fetcher(
		  class("hypr.Hypr")
		  options(
			  "url","https://<domain>.hypr.com"
			  "rp_app_id","<rpAppId>"
			  "bearer_token","<base64 encoded bearer token>"
			  "page_size","<number of results to return in a single page>"
			  "initial_hours","<number of hours to search backward on initial fetch>"
			  "log_level","<DEBUG|INFO|WARN|ERROR>"
              "max_performance","<True|False>"
			  )
		  flags(no-parse)
		  fetch-no-data-delay(<seconds to wait before attempting a fetch after no results are returned>)
	    );
    };

Here are sample values as a reference:

    source s_hypr {
      python-fetcher(
		  class("hypr.Hypr")
		  options(
			  "url","https://my-domain.hypr.com"
			  "rp_app_id","WindowsLogin"
			  "bearer_token","xxxx"
			  "page_size","1000"
			  "initial_hours","24"
			  "log_level","DEBUG"
              "max_performance","False"
			  )
		  flags(no-parse)
		  fetch-no-data-delay(60)
	    );
    };

### gen-hypr.py

This Python script can be used to dynamically generate a syslog-ng configuration for a Hypr API that will fetch events from all rpAppIds not specifically blocked by configuration. Each configuration will fall under the same top level source and include a Hypr driver configured for a specific rpAppId that is independent of all other Hypr drivers. This is only required for runtime detection or available rpAppId values, to statically configure the Hypr driver for an rpAppId(s) the gen-hypr confgen utility is not needed.

To utilize the gen-hypr.py configuration generator, the following steps are needed:
1. Create a new SCL directory named hypr (e.g., /opt/syslog-ng/share/syslog-ng/include/scl/hypr/)
2. Save gen-hypr.py to /opt/syslog-ng/share/syslog-ng/include/scl/hypr/gen-hypr.py
3. Save plugin.conf to /opt/syslog-ng/share/syslog-ng/include/scl/hypr/plugin.conf
4. Create a new syslog-ng source with the required parameters

Required minimum parameters:

    source s_hypr {
        hypr(
            url('https://<custom domain>.hypr.com')
            bearer_token('<base64 encoded bearer token>')
            persist_name('<unique name>')
        );
    };

or with full options:

    source s_hypr {
        hypr(
            url('https://<custom domain>.hypr.com')
            bearer_token('<base64 encoded bearer token>')
            page_size(<number of results to return in a single page>)
            initial_hours(<number of hours to search backward on initial fetch>)
            persist_name('<unique name>')
            log_level('<DEBUG|INFO|WARN|ERROR>')
            sleep(<seconds to wait before attempting a fetch after no results are returned>)
            application_skip_list('HYPRDefaultApplication,HYPRDefaultWorkstationApplication')
            max_performance('<True|False>')
        );
    };

As a configured example:

    source s_hypr {
        hypr(
            url('https://my-domain.hypr.com')
            bearer_token('xxxx')
            page_size(1000)
            initial_hours(24)
            persist_name('s_hypr')
            log_level('debug')
            sleep(60)
            application_skip_list('HYPRDefaultApplication,HYPRDefaultWorkstationApplication')
            max_performance('False')
        );
    };

### Driver options

url - custom URL for Hypr API access ('https://<custom domain>.hypr.com')

bearer_token - base64 encoded authentication token from Hypr

page_size - number of results to return in a single page (optional - defaults to 100)

initial_hours - number of hours to search backward on initial fetch (optional - defaults to 4)

persist_name - a unique name for this driver (optional - defaults to include rp_app_id)

log_level - the logging level (DEBUG, INFO, WARN, ERROR, or CRIT) to output from syslog-ng (optional - defaults to INFO)

sleep - seconds to wait before attempting a fetch after no results are returned (optional, defaults to 60)

application_skip_list - list of rpAppIds not to retrieve from Hypr (optional - defaults to 'HYPRDefaultApplication,HYPRDefaultWorkstationApplication')

max_performance - Disables json parsing of message for timestamp extraction if True (optional - defaults to False)