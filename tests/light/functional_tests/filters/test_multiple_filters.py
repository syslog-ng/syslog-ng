#!/usr/bin/env python
#############################################################################
# Copyright (c) 2024 Attila Szakacs
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
from src.syslog_ng_config.renderer import render_statement


def test_multiple_filters_implicit_and(config, syslog_ng):
    file_true = config.create_file_destination(file_name="dest-true.log", template="\"$MSG\\n\"")
    file_false = config.create_file_destination(file_name="dest-false.log", template="\"$MSG\\n\"")

    preamble = f"""
@version: {config.get_version()}

options {{ stats(level(1)); }};

source genmsg {{
    example-msg-generator(num(1) template("MESSAGE"));
    example-msg-generator(num(1) template("foobar"));
}};

filter f_filter {{
    not program("xyz");
    message("MESSAGE");
}};

destination dest_true {{
{render_statement(file_true)};
}};

destination dest_false {{
{render_statement(file_false)};
}};

log {{
    source(genmsg);
    if {{
        filter(f_filter);
        destination(dest_true);
    }} else {{
        destination(dest_false);
    }};
}};
"""
    config.set_raw_config(preamble)
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert file_false.get_stats()["processed"] == 1

    assert "MESSAGE" in file_true.read_log()
    assert "foobar" in file_false.read_log()
