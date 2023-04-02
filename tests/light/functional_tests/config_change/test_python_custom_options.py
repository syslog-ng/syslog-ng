#!/usr/bin/env python
#############################################################################
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


def test_python_custom_options(config, syslog_ng):
    raw_config = r"""
python {
import syslogng

def assert_options(options):
    assert options["string"] == "example_string", "Actual: {}".format(options["string"])
    assert options["true"] == True, "Actual: {}".format(options["true"])
    assert options["false"] == False, "Actual: {}".format(options["false"])
    assert options["long"] == 123456789, "Actual: {}".format(options["long"])
    assert options["double"] == 123.456789, "Actual: {}".format(options["double"])
    assert options["string_list"] == ["string1", "string2"], "Actual: {}".format(options["string_list"])
    assert isinstance(options["template"], syslogng.LogTemplate), "Actual: {}".format(type(options["template"]))
    assert str(options["template"]) == "${template}", "Actual: {}".format(str(options["template"]))


class TestSource(syslogng.LogSource):
  def init(self, options):
    assert_options(options)
    return True


class TestFetcher(syslogng.LogFetcher):
  def init(self, options):
    assert_options(options)
    return True


class TestDestination(syslogng.LogDestination):
  def init(self, options):
    assert_options(options)
    return True


class TestParser(syslogng.LogParser):
  def init(self, options):
    assert_options(options)
    return True


class TestHttpHeader:
  def __init__(self, options):
    assert_options(options)

  def get_headers():
    pass

  def on_http_response_received():
    pass


def test_confgen(options):
  return '''
    python(
      class("TestDestination")
      options(
        "string" => {}
        "true" => {}
        "false" => {}
        "long" => {}
        "double" => {}
        "string-list" => [{}]
        "template" => LogTemplate({})
      )
      persist-name("custom-persist-name")
    );
  '''.format(options["string"], options["true"], options["false"],
             options["long"], options["double"], options["string_list"],
             options["template"])


syslogng.register_config_generator(context="destination", name="test_confgen",
                                   config_generator=test_confgen)
};

log {
  source {
    python(
      class("TestSource")
      options(
        "string" => "example_string"
        "true" => True
        "false" => False
        "long" => 123456789
        "double" => 123.456789
        "string-list" => ["string1", "string2"]
        "template" => LogTemplate("${template}")
      )
    );
    python-fetcher(
      class("TestFetcher")
      options(
        "string" => "example_string"
        "true" => True
        "false" => False
        "long" => 123456789
        "double" => 123.456789
        "string-list" => ["string1", "string2"]
        "template" => LogTemplate("${template}")
      )
    );
  };
  parser {
    python(
      class("TestParser")
      options(
        "string" => "example_string"
        "true" => True
        "false" => False
        "long" => 123456789
        "double" => 123.456789
        "string-list" => ["string1", "string2"]
        "template" => LogTemplate("${template}")
      )
    );
  };
  destination {
    python(
      class("TestDestination")
      options(
        "string" => "example_string"
        "true" => True
        "false" => False
        "long" => 123456789
        "double" => 123.456789
        "string-list" => ["string1", "string2"]
        "template" => LogTemplate("${template}")
      )
    );
    test-confgen(
      string("example_string")
      true(yes)
      false(no)
      long(123456789)
      double(123.456789)
      string-list("string1", "string2")
      template("${template}")
    );
  };
};
"""
    raw_config = "@version: {}\n".format(config.get_version()) + raw_config
    config.set_raw_config(raw_config)

    syslog_ng.start(config)
