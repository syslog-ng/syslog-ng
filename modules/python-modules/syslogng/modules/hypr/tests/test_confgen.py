#############################################################################
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
from syslogng.modules import hypr
import base64


def test_config_generator_all_options(mocker):
    mocker.patch("requests.get", return_value=mocker.Mock(**{
        'status_code': 200,
        'json': mocker.Mock(return_value=[
            {"appID": "HYPRDefaultApplication"},
            {"appID": "HYPRDefaultWorkstationApplication"},
            {"appID": "rp_foo"},
        ])
    }))

    bearer_token = b'bearer_token'
    base64_encoded_bearer_token = base64.b64encode(bearer_token).decode('utf8')
    hypr_args = {
        'url': '"https://dummy.hypr.com/"',
        'bearer_token': '"' + base64_encoded_bearer_token + '"',
    }

    snippet = hypr._hypr_config_generator(hypr_args)
    expected_snippet = """
        python-fetcher(
            class("syslogng.modules.hypr.HyprAuditSource")
            options(
                "url" => "https://dummy.hypr.com/"
                "rp_app_id" => "rp_foo"
                "bearer_token" => "%s"
                "page_size" => 100
                "initial_hours" => 4
                "ignore_persistence" => False
                "log_level" => "INFO"
            )
            flags()
            persist-name("hypr-https://dummy.hypr.com/-rp_foo")
            fetch-no-data-delay(60)
        );""" % base64_encoded_bearer_token

    assert expected_snippet in snippet


def test_config_generator_all_options(mocker):
    mocker.patch("requests.get", return_value=mocker.Mock(**{
        'status_code': 200,
        'json': mocker.Mock(return_value=[{"appID": "rp_foo"}, {"appID": "rp_bar"}, {"appID": "rp_baz"}])
    }))

    bearer_token = b'bearer_token'
    base64_encoded_bearer_token = base64.b64encode(bearer_token).decode('utf8')
    hypr_args = {
        'url': '"https://dummy.hypr.com/"',
        'bearer_token': '"' + base64_encoded_bearer_token + '"',
        'page_size': "123",
        'initial_hours': "9",
        'log_level': 'ERROR',
        'application_skiplist': '"rp_baz", "rp_app"',
        'ignore_persistence': "yes",
        'persist_name': '"custom-persist-name"',
        'fetch_no_data_delay': "42",
        'flags': 'no-parse',
    }

    snippet = hypr._hypr_config_generator(hypr_args)
    expected_snippets = [
        """
        python-fetcher(
            class("syslogng.modules.hypr.HyprAuditSource")
            options(
                "url" => "https://dummy.hypr.com/"
                "rp_app_id" => "rp_foo"
                "bearer_token" => "%s"
                "page_size" => 123
                "initial_hours" => 9
                "ignore_persistence" => yes
                "log_level" => "ERROR"
            )
            flags(no-parse)
            persist-name("custom-persist-name-rp_foo")
            fetch-no-data-delay(42)
        );""" % base64_encoded_bearer_token,
        """
        python-fetcher(
            class("syslogng.modules.hypr.HyprAuditSource")
            options(
                "url" => "https://dummy.hypr.com/"
                "rp_app_id" => "rp_bar"
                "bearer_token" => "%s"
                "page_size" => 123
                "initial_hours" => 9
                "ignore_persistence" => yes
                "log_level" => "ERROR"
            )
            flags(no-parse)
            persist-name("custom-persist-name-rp_bar")
            fetch-no-data-delay(42)
        );""" % base64_encoded_bearer_token,
    ]

    for expected_snippet in expected_snippets:
        assert expected_snippet in snippet
