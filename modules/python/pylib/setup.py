#!/usr/bin/env python

from distutils.core import setup

setup(name='syslogng',
      version='1.0',
      description='syslog-ng Python extensions',
      author='Balazs Scheidler',
      author_email='balazs.scheidler@balabit.com',
      url='https://www.syslog-ng.org',
      packages=['syslogng', 'syslogng.debuggercli'],
     )