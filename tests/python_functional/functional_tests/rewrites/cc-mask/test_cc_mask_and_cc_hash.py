#!/usr/bin/env python
#############################################################################
# Copyright (c) 2021 One Identity
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

_Input_String = "American Express  378282246310005" \
                "American Express  371449635398431" \
                "American Express Corporate  378734493671000" \
                "Diners Club  30569309025904" \
                "Diners Club  38520000023237" \
                "Discover  6011111111111117" \
                "Discover  6011000990139424" \
                "JCB  3530111333300000" \
                "JCB  3566002020360505" \
                "MasterCard  5555555555554444" \
                "MasterCard  5105105105105100" \
                "Visa  4111111111111111" \
                "Visa  4012888888881881" \
                "Visa  4222222222222"

_Expected_mask_output = "American Express  378282******0005" \
                        "American Express  371449******8431" \
                        "American Express Corporate  378734******1000" \
                        "Diners Club  305693******5904" \
                        "Diners Club  385200******3237" \
                        "Discover  601111******1117" \
                        "Discover  601100******9424" \
                        "JCB  353011******0000" \
                        "JCB  356600******0505" \
                        "MasterCard  555555******4444" \
                        "MasterCard  510510******5100" \
                        "Visa  411111******1111" \
                        "Visa  401288******1881" \
                        "Visa  422222******2222"

_Expected_hash_output = "American Express  ea4654336c140e70" \
                        "American Express  5e7d7549d9a51a21" \
                        "American Express Corporate  b83feb75b1ce505d" \
                        "Diners Club  58b3e8b7f99a5ab1" \
                        "Diners Club  002f83eefd0b7e53" \
                        "Discover  0ccaaf4da33d3e26" \
                        "Discover  ff659bd8ffefdb2b" \
                        "JCB  4c1d57bdab8338e7" \
                        "JCB  9d9cafd187ba5590" \
                        "MasterCard  6589b0d46b6f2f0d" \
                        "MasterCard  21b95eabb14f0726" \
                        "Visa  68bfb396f35af387" \
                        "Visa  62163a017b168ad4" \
                        "Visa  eb0f3622c9362fe9"


def test_cc_mask_and_cc_hash(config, syslog_ng):
    config.add_include("scl.conf")
    rewrite_cc_mask = config.create_rewrite_credit_card_mask()
    rewrite_cc_hash = config.create_rewrite_credit_card_hash()

    generator_source = config.create_example_msg_generator_source(num=1, template=config.stringify(_Input_String))
    file_destination_mask = config.create_file_destination(file_name="output_mask.log", template=config.stringify('${MSG}\n'))
    file_destination_hash = config.create_file_destination(file_name="output_hash.log", template=config.stringify('${MSG}\n'))
    config.create_logpath(statements=[generator_source, rewrite_cc_hash, file_destination_hash])
    config.create_logpath(statements=[generator_source, rewrite_cc_mask, file_destination_mask])

    syslog_ng.start(config)
    assert file_destination_mask.read_log().strip() == _Expected_mask_output
    assert file_destination_hash.read_log().strip() == _Expected_hash_output
