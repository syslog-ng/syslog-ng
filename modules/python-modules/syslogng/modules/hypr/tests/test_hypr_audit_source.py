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
from syslogng.modules import hypr
import base64
import pytest
import json


@pytest.fixture
def empty_config():
    return {}


@pytest.fixture
def minimal_config():
    bearer_token = b'bearer_token'
    base64_encoded_bearer_token = base64.b64encode(bearer_token).decode('utf8')
    return {
        'url': 'https://dummy.hypr.com',
        'bearer_token': base64_encoded_bearer_token,
        'rp_app_id': 'rpFoo'
    }


def test_hypr_audit_source_can_be_instantiated():

    sut = hypr.HyprAuditSource()
    assert sut is not None


def test_hypr_audit_source_with_invalid_configuration(caplog, empty_config):
    sut = hypr.HyprAuditSource()

    assert sut.init(empty_config) is False
    assert "Missing rp_app_id" in caplog.text


def test_hypr_audit_source_lifecycle_without_actually_fetching_messages(minimal_config):

    sut = hypr.HyprAuditSource()

    assert sut.init(minimal_config) is True
    assert sut.open() is True
    assert sut.close() is None
    sut.deinit()


def test_hypr_audit_source_complete_lifecycle(minimal_config, mocker):

    response_json = [
        {
            'metadata': {
                'totalRecords': 2,
                'totalPages': 1,
                'currentPage': 1,
            },
            'data': [
                {'rpAppId': 'rp_foo', 'eventTimeInUTC': 1666427394, 'message': 'whatever first', 'counter': 0},
                {'rpAppId': 'rp_foo', 'eventTimeInUTC': 1666427394, 'message': 'whatever second', 'counter': 1}
            ]
        },
        {
            'metadata': {
                'totalRecords': 0,
                'totalPages': 1,
                'currentPage': 1,
            },
            'data': [],
        }
    ]

    m = mocker.Mock(side_effect=response_json)

    mocker.patch('requests.get', return_value=mocker.Mock(**{
        'status_code': 200,
        'json': mocker.Mock(side_effect=response_json)
    }))
    sut = hypr.HyprAuditSource()

    assert sut.init(minimal_config) is True
    assert sut.open() is True

    index = 0
    status, msg = sut.fetch()
    while status == sut.SUCCESS:
        js = json.loads(msg['MESSAGE'])
        assert js['rpAppId'] == 'rp_foo'
        assert js['counter'] == index

        index += 1
        status, msg = sut.fetch()

    assert status == sut.NO_DATA
    assert index == 2

    assert sut.close() is None
    sut.deinit()
