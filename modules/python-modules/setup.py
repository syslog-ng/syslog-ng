#!/usr/bin/env python3
#############################################################################
# Copyright (c) 2022 Balazs Scheidler <bazsi77@gmail.com>
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

from setuptools import setup

setup(name='syslogng',
      version='1.0',
      description='syslog-ng Python Core & Modules',
      author='Balazs Scheidler',
      author_email='bazsi77@gmail.com',
      url='https://www.syslog-ng.org',
      package_data={"": ["scl/*"]},
      exclude_package_data={"": ["*~"]},
      packages=[
        "syslogng",
        "syslogng.debuggercli",
        "syslogng.modules.example",
        "syslogng.modules.kubernetes",
      ],
      install_requires=[
          "kubernetes"
      ])
