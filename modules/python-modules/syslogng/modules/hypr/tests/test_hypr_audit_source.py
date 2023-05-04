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
                {
                    'rpAppId': 'rp_foo',
                    'eventTimeInUTC': 1666427394,
                    'message': 'whatever first',
                    'counter': 0,
                },
                {
                    'rpAppId': 'rp_foo',
                    'eventTimeInUTC': 1666427394,
                    'message': 'whatever second',
                    'counter': 1,
                    'single_nested': {
                        'double_nested': {
                            'none': None,
                        },
                    },
                    'float': 123.456,
                    'string_list': ['foo', 'bar'],
                    'non_string_list': [1, 2],
                    'bool': True,
                },
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
        if index == 0:
            assert msg['MESSAGE'] == b'whatever first'
            assert msg['PROGRAM'] == b'rp_foo'
            assert msg['.hypr.rpAppId'] == b'rp_foo'
            assert msg['.hypr.eventTimeInUTC'] == 1666427394
            assert msg['.hypr.message'] == b'whatever first'
            assert msg['.hypr.counter'] == 0
        elif index == 1:
            assert msg['MESSAGE'] == b'whatever second'
            assert msg['PROGRAM'] == b'rp_foo'
            assert msg['.hypr.rpAppId'] == b'rp_foo'
            assert msg['.hypr.eventTimeInUTC'] == 1666427394
            assert msg['.hypr.message'] == b'whatever second'
            assert msg['.hypr.counter'] == 1
            assert msg['.hypr.single_nested.double_nested.none'] is None
            assert msg['.hypr.float'] == 123.456
            assert msg['.hypr.string_list'] == ['foo', 'bar']
            assert msg['.hypr.non_string_list'] == b'[1, 2]'
            assert msg['.hypr.bool'] is True

        index += 1
        status, msg = sut.fetch()

    assert status == sut.NO_DATA
    assert index == 2

    assert sut.close() is None
    sut.deinit()


def test_hypr_audit_source_complete_lifecycle_no_parse(minimal_config, mocker):

    response_json = [
        {
            'metadata': {
                'totalRecords': 1,
                'totalPages': 1,
                'currentPage': 1,
            },
            'data': [
                {
                    'rpAppId': 'rp_foo',
                    'eventTimeInUTC': 1666427394,
                    'message': 'whatever first',
                    'counter': 1,
                    'single_nested': {
                        'double_nested': {
                            'none': None,
                        },
                    },
                    'float': 123.456,
                    'string_list': ['foo', 'bar'],
                    'non_string_list': [1, 2],
                    'bool': True,
                },
            ],
        }
    ]

    m = mocker.Mock(side_effect=response_json)

    mocker.patch('requests.get', return_value=mocker.Mock(**{
        'status_code': 200,
        'json': mocker.Mock(side_effect=response_json)
    }))
    sut = hypr.HyprAuditSource()
    sut.flags["parse"] = False

    assert sut.init(minimal_config) is True
    assert sut.open() is True

    status, msg = sut.fetch()

    assert status == sut.SUCCESS
    assert msg['MESSAGE'] == b'{"rpAppId": "rp_foo", "eventTimeInUTC": 1666427394, "message": "whatever first", "counter": 1, "single_nested": {"double_nested": {"none": null}}, "float": 123.456, "string_list": ["foo", "bar"], "non_string_list": [1, 2], "bool": true}'
    assert msg['PROGRAM'] == b'Hypr-rpFoo'

    assert sut.close() is None
    sut.deinit()
