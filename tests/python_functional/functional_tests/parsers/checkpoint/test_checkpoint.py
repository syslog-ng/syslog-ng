#!/usr/bin/env python
#############################################################################
# Copyright (c) 2020 Balabit
# Copyright (c) 2020 Kokan
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
    (r"""<134>1 2018-03-21 17:25:25 MDS-72 CheckPoint 13752 - [action:"Update"; flags:"150784"; ifdir:"inbound"; logid:"160571424"; loguid:"{0x5ab27965,0x0,0x5b20a8c0,0x7d5707b6}"; origin:"192.168.32.91"; originsicname:"CN=GW91,O=Domain2_Server..cuggd3"; sequencenum:"1"; time:"1521645925"; version:"5"; auth_method:"Machine Authentication (Active Directory)"; auth_status:"Successful Login"; authentication_trial:"this is a reauthentication for session 9a026bba"; client_name:"Active Directory Query"; client_version:"R80.10"; domain_name:"spec.mgmt"; endpoint_ip:"192.168.32.69"; identity_src:"AD Query"; identity_type:"machine"; product:"Identity Awareness"; snid:"9a026bba"; src:"192.168.32.69"; src_machine_group:"All Machines"; src_machine_name:"yonatanad";]""", "<CheckPoint><MDS-72><Mar 21 17:25:25>"),
    (r"""<134>1 2018-03-21T17:25:25 MDS-72 CheckPoint 13752 - [action:"Update"; flags:"150784"; ifdir:"inbound"; logid:"160571424"; loguid:"{0x5ab27965,0x0,0x5b20a8c0,0x7d5707b6}"; origin:"192.168.32.91"; originsicname:"CN=GW91,O=Domain2_Server..cuggd3"; sequencenum:"1"; time:"1521645925"; version:"5"; auth_method:"Machine Authentication (Active Directory)"; auth_status:"Successful Login"; authentication_trial:"this is a reauthentication for session 9a026bba"; client_name:"Active Directory Query"; client_version:"R80.10"; domain_name:"spec.mgmt"; endpoint_ip:"192.168.32.69"; identity_src:"AD Query"; identity_type:"machine"; product:"Identity Awareness"; snid:"9a026bba"; src:"192.168.32.69"; src_machine_group:"All Machines"; src_machine_name:"yonatanad";]""", "<CheckPoint><MDS-72><Mar 21 17:25:25>"),
    (r"""<134>1 2019-11-27T02:58:25Z ABDP-CPLOG01 CheckPoint 22103 - [action:"Accept"; flags:"18692"; ifdir:"inbound"; ifname:"bond1.734"; loguid:"{0x5ddde651,0x48,0xca96040a,0xc000001b}"; origin:"10.4.150.2"; time:"1574823505"; version:"1"; __policy_id_tag:"product=VPN-1 & FireWall-1[db_tag={12D7A082-42F0-B240-8103-29F2F6FF139C};mgmt=ABDP-MGT01;date=1573204576;policy_name=Cutover_Final_V3_20161031]"; dst:"124.156.190.9"; nat_addtnl_rulenum:"1"; nat_rulenum:"8"; origin_sic_name:"CN=ABDP-CPFW01,O=ABDP-CPMGT01..94r78q"; product:"VPN-1 & FireWall-1"; proto:"6"; rule:"8"; rule_name:"3G IP Pool Outgoing"; rule_uid:"{DF64DC15-BEDF-4246-8B71-A75E0991C5D9}"; s_port:"64442"; service:"80"; service_id:"http"; src:"10.7.79.7"; xlatedport:"0"; xlatedst:"0.0.0.0"; xlatesport:"49254"; xlatesrc:"202.1.50.67"; ]""", "<CheckPoint><ABDP-CPLOG01><Nov 27 02:58:25>"),
    (r"""<134>1 2020-01-02T14:04:50Z fwmgmt CheckPoint 16559 - [action:"Accept"; flags:"411908"; ifdir:"inbound"; ifname:"bond0.401"; logid:"0"; loguid:"{0x5e0df882,0x0,0x20010ac,0xc0000002}"; origin:"172.16.0.2"; originsicname:"CN=fw01,O=fwmgmt.ny-net.local.nahkt6"; sequencenum:"6"; time:"1577973890"; version:"5"; __policy_id_tag:"product=VPN-1 & FireWall-1[db_tag={E97DE696-700C-7F42-BD71-537C8A0883CB};mgmt=fwmgmt;date=1556607233;policy_name=Standard\]"; dst:"192.203.230.10"; inzone:"Internal"; layer_name:"Network"; layer_uuid:"0eeae4c4-aa05-40a9-a319-623823330c95"; match_id:"7"; parent_rule:"0"; rule_action:"Accept"; rule_name:"ALLOW"; rule_uid:"52ff639d-c772-4797-8b45-e151e295702e"; nat_addtnl_rulenum:"1"; nat_rulenum:"8"; outzone:"External"; product:"VPN-1 & FireWall-1"; proto:"17"; s_port:"55263"; service:"53"; service_id:"domain-udp"; src:"10.126.0.154"; xlatedport:"0"; xlatedst:"0.0.0.0"; xlatesport:"52993"; xlatesrc:"172.16.16.10"; ]""", "<CheckPoint><fwmgmt><Jan  2 14:04:50>"),
]


@pytest.mark.parametrize(
    "input_message, expected_value", test_parameters_raw,
    ids=list(map(str, range(len(test_parameters_raw)))),
)
def test_checkpoint_parser(config, syslog_ng, input_message, expected_value):
    config.add_include("scl.conf")

    generator_source = config.create_example_msg_generator_source(num=1, template=config.stringify(input_message))
    checkpoint_parser = config.create_checkpoint_parser()

    file_destination = config.create_file_destination(file_name="output.log", template=config.stringify('<${PROGRAM}><${HOST}><${S_DATE}>\n'))
    config.create_logpath(statements=[generator_source, checkpoint_parser, file_destination])

    syslog_ng.start(config)
    assert file_destination.read_log().strip() == expected_value
