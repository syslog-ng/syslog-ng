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


def test_config_generator_generates_a_config_snippet_with_all_apps(mocker):
    m = mocker.patch("requests.get", return_value=mocker.Mock(**{
        'status_code': 200,
        'json': mocker.Mock(return_value=[{"appID": "rp_foo"}, {"appID": "rp_bar"}])
    }))

    bearer_token = b'bearer_token'
    base64_encoded_bearer_token = base64.b64encode(bearer_token).decode('utf8')
    hypr_args = {
        'url': 'https://dummy.hypr.com/',
        'bearer_token': base64_encoded_bearer_token,
    }
    snippet = hypr._hypr_config_generator(hypr_args)
    assert 'syslogng.modules.hypr.HyprAuditSource' in snippet
    assert '"url" => "https://dummy.hypr.com/"' in snippet
    assert '"bearer_token" => "{}"'.format(base64_encoded_bearer_token) in snippet
    assert '"rp_app_id" => "rp_foo"' in snippet
    assert '"rp_app_id" => "rp_bar"' in snippet
