# hypr-audit-trail() and hypr-app-audit-trail() source drivers

This package implements a syslog-ng source driver for the Hypr Audit Trail

Events are fetched from Hypr via their [REST API](https://apidocs.hypr.com/)

## Usage

There are two drivers published by this package:

  * hypr-audit-trail(): is a source driver that pulls messages from the Hypr
    API, associated to any RP Application ID.
  * hypr-app-audit-trail(): is a source driver that pulls messages from the
    Hypr API, but only those associated to a specific RP Application ID.


### hypr-audit-trail()

The hypr-audit-trail() source would query the Hypr API for the list of
potential applications at startup and then would monitor the audit trail for
each of the detected applications.

Applications that are registered after syslog-ng is started will not be
recognized. To start following those audit trails, a restart of syslog-ng is
needed.

Example:

```
    source s_hypr {
        hypr-audit-trail(
            url('https://<custom domain>.hypr.com')
            bearer_token('<base64 encoded bearer token>')
        );
    };
```

A more complete example:

```
    source s_hypr {
        hypr-audit-trail(
            url('https://<custom domain>.hypr.com')
            bearer_token('<base64 encoded bearer token>')
            page_size(<number of results to return in a single page>)
            initial_hours(<number of hours to search backward on initial fetch>)
            application_skip_list('HYPRDefaultApplication,HYPRDefaultWorkstationApplication')
            max_performance('<True|False>')
        );
    };
```


Options:
  url: custom URL for Hypr API access ('https://<custom domain>.hypr.com')
  bearer_token: base64 encoded authentication token from Hypr
  page_size: number of results to return in a single page (optional - defaults to 100)
  initial_hours: number of hours to search backward on initial fetch (optional - defaults to 4)
  application_skip_list - list of rpAppIds not to retrieve from Hypr (optional - defaults to 'HYPRDefaultApplication,HYPRDefaultWorkstationApplication')
  max_performance - Disables json parsing of message for timestamp extraction if True (optional - defaults to False)

### hypr-app-audit-trail()

The hypr-app-audit-trail() monitors the audit trail for one specific RP
Application ID. This driver requires the rp_app_id() parameter in order to
operate.

Options:
  url: custom URL for Hypr API access ('https://<custom domain>.hypr.com')
  bearer_token: base64 encoded authentication token from Hypr
  rp_app_id: the RP Application ID for the application to monitor
  page_size: number of results to return in a single page (optional - defaults to 100)
  initial_hours: number of hours to search backward on initial fetch (optional - defaults to 4)
  max_performance - Disables json parsing of message for timestamp extraction if True (optional - defaults to False)


### Implementation

This source driver is implemented in Python and is part of the
syslogng.modules.hypr Python package that is installed by syslog-ng in the
/usr/lib/syslog-ng/python/ directory.

The driver is packaged into the syslog-ng-mod-python (Debian/Ubuntu and
derivatives) or syslog-ng-python (rpm based distros) package and is ready to
use if you installed those packages. In the future this might become a
dedicated OS level package (e.g. syslog-ng-mod-hypr).
