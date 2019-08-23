#!/usr/bin/env python
#############################################################################
# Copyright (c) 2019 Balabit
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
import pytest

test_parameters_raw = [
    (r"""<189>29: foo: *Apr 29 13:58:40.411: %SYS-5-CONFIG_I: Configured from console by console""", "${.app.name}", "cisco"),
    (r"""<190>30: foo: *Apr 29 13:58:46.411: %SYS-6-LOGGINGHOST_STARTSTOP: Logging to host 192.168.1.239 stopped - CLI initiated""", "${.app.name}", "cisco"),
    (r"""<190>31: foo: *Apr 29 13:58:46.411: %SYS-6-LOGGINGHOST_STARTSTOP: Logging to host 192.168.1.239 started - CLI initiated<189>32: 0.0.0.0: *Apr 29 13:59:12.491: %SYS-5-CONFIG_I: Configured from console by console<189>33: 0.0.0.0: *Apr 29 13:59:26.415: %SYS-5-CONFIG_I: Configured from console by console<189>34: 0.0.0.0: *Apr 29 13:59:56.603: %SYS-5-CONFIG_I: Configured from console by console^[[<189>35: *Apr 29 14:00:16.059: %SYS-5-CONFIG_I: Configured from console by console""", "${.app.name}", "cisco"),
    (r"""<190>32: foo: *Apr 29 13:58:46.411: %SYSMGR-STANDBY-3-SHUTDOWN_START: The System Manager has started the shutdown procedure.""", "${.app.name}", "cisco"),
    (r"""<134>{"count": 1, "supporting_data": {"data_values": ["x.x.x.x", "user@domain.com"], "data_type": "user"}, "organization_unit": "domain/domain/Domain Users/Enterprise Users", "severity_level": 2, "category": null, "timestamp": 1547421943, "_insertion_epoch_timestamp": 1547421943, "ccl": "unknown", "user": "user@domain.com", "audit_log_event": "Login Successful", "ur_normalized": "user@domain.com", "_id": "936289", "type": "admin_audit_logs", "appcategory": null}""", "${.app.name}", "netskope"),
    (r"""<159>Dec 19 10:48:57 EST 10.203.28.21 vendor=Websense product=Security product_version=7.7.0 action=permitted severity=1 category=153 user=- src_host=10.64.134.74 src_port=62189 dst_host=mail.google.com dst_ip=74.125.224.53 dst_port=443 bytes_out=197 bytes_in=76 http_response=200 http_method=CONNECT http_content_type=- http_user_agent=Mozilla/5.0_(Windows;_U;_Windows_NT_6.1;_enUS;_rv:1.9.2.23)_Gecko/20110920_Firefox/3.6.23 http_proxy_status_code=200 reason=- disposition=1034 policy=- role=8 duration=0 url=https://mail.google.com""", "${.app.name}", "websense"),
    (r"""<134>1 2018-03-21 17:25:25 MDS-72 CheckPoint 13752 - [action:"Update"; flags:"150784"; ifdir:"inbound"; logid:"160571424"; loguid:"{0x5ab27965,0x0,0x5b20a8c0,0x7d5707b6}"; origin:"192.168.32.91"; originsicname:"CN=GW91,O=Domain2_Server..cuggd3"; sequencenum:"1"; time:"1521645925"; version:"5"; auth_method:"Machine Authentication (Active Directory)"; auth_status:"Successful Login"; authentication_trial:"this is a reauthentication for session 9a026bba"; client_name:"Active Directory Query"; client_version:"R80.10"; domain_name:"spec.mgmt"; endpoint_ip:"192.168.32.69"; identity_src:"AD Query"; identity_type:"machine"; product:"Identity Awareness"; snid:"9a026bba"; src:"192.168.32.69"; src_machine_group:"All Machines"; src_machine_name:"yonatanad";]""", "${.app.name}", "checkpoint"),
    (r"""<134>1 2018-03-21T17:25:25 MDS-72 CheckPoint 13752 - [action:"Update"; flags:"150784"; ifdir:"inbound"; logid:"160571424"; loguid:"{0x5ab27965,0x0,0x5b20a8c0,0x7d5707b6}"; origin:"192.168.32.91"; originsicname:"CN=GW91,O=Domain2_Server..cuggd3"; sequencenum:"1"; time:"1521645925"; version:"5"; auth_method:"Machine Authentication (Active Directory)"; auth_status:"Successful Login"; authentication_trial:"this is a reauthentication for session 9a026bba"; client_name:"Active Directory Query"; client_version:"R80.10"; domain_name:"spec.mgmt"; endpoint_ip:"192.168.32.69"; identity_src:"AD Query"; identity_type:"machine"; product:"Identity Awareness"; snid:"9a026bba"; src:"192.168.32.69"; src_machine_group:"All Machines"; src_machine_name:"yonatanad";]""", "${.app.name}", "checkpoint"),
    (r"""time=1557767758|hostname=r80test|product=Firewall|layer_name=Network|layer_uuid=c0264a80-1832-4fce-8a90-d0849dc4ba33|match_id=1|parent_rule=0|rule_action=Accept|rule_uid=4420bdc0-19f3-4a3e-8954-03b742cd3aee|action=Accept|ifdir=inbound|ifname=eth0|logid=0|loguid={0x5cd9a64e,0x0,0x5060a8c0,0xc0000001}|origin=192.168.96.80|originsicname=cn\=cp_mgmt,o\=r80test..ymydp2|sequencenum=1|time=1557767758|version=5|dst=192.168.96.80|inzone=Internal|outzone=Local|proto=6|s_port=63945|service=443|service_id=https|src=192.168.96.27|""", "${.app.name}", "checkpoint"),
]


@pytest.mark.parametrize(
    "input_message, template, expected_value", test_parameters_raw,
    ids=list(map(str, range(len(test_parameters_raw)))),
)
def test_application_raw(config, syslog_ng, input_message, template, expected_value):
    config.add_include("scl.conf")

    generator_source = config.create_example_msg_generator_source(num=1, template=config.stringify(input_message))
    app_parser = config.create_app_parser(topic="syslog-raw")

    file_destination = config.create_file_destination(file_name="output.log", template=config.stringify(template + '\n'))
    config.create_logpath(statements=[generator_source, app_parser, file_destination])

    syslog_ng.start(config)
    assert file_destination.read_log().strip() == expected_value
