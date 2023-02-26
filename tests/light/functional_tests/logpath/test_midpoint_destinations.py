#!/usr/bin/env python
#############################################################################
# Copyright (c) 2023 Balazs Scheidler <bazsi77@gmail.com>
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


def create_config(config, test_config, msg="foobar"):
    file_true = config.create_file_destination(file_name="dest-true.log", template="'$MSG\n'")
    file_false = config.create_file_destination(file_name="dest-false.log", template="'$MSG\n'")
    file_after = config.create_file_destination(file_name="dest-after.log", template="'$MSG\n'")
    file_fallback = config.create_file_destination(file_name="dest-fallback.log", template="'$MSG\n'")
    file_dropall = config.create_file_destination(file_name="dest-dropall.log", template="'$MSG\n'")

    preamble = f"""
@version: {config.get_version()}

options {{ stats-level(1); }};

block filter true() {{
    "1" eq "1"
}};

block filter false() {{
    "0" eq "1"
}};

source genmsg {{
    example-msg-generator(num(1) template("{msg}"));
}};

destination dest_after {{
    {render_statement(file_after)};
}};

destination dest_true {{
    {render_statement(file_true)};
}};

destination dest_false {{
    {render_statement(file_false)};
}};

destination dest_fallback {{
    {render_statement(file_fallback)};
}};

destination dest_dropall {{
    channel {{
        filter {{ false(); }};
        destination {{ {render_statement(file_dropall)}; }};
    }};
}};

log {{
    source(genmsg);
    destination(dest_fallback);
    flags(fallback);
}};

"""
    config.set_raw_config(preamble + test_config)
    return (file_true, file_false, file_after, file_fallback, file_dropall)


def test_midpoint_destination_that_drops_all_messages_does_not_cause_message_to_be_unmatched(config, syslog_ng):
    # Rationale:
    #
    #   midpoint destinations which drop messages completely do it in their
    #   own context which should not affect the log path as a whole.  As far
    #   as the log path is concerned, the message was successfully
    #   dispatched to the destination.
    test_config = """

log {
    source(genmsg);
    destination(dest_dropall);
    destination(dest_after);
};
"""
    (file_true, file_false, file_after, file_fallback, file_dropall) = create_config(config, test_config)
    syslog_ng.start(config)

    assert "processed" not in file_dropall.get_stats()
    assert file_after.get_stats()["processed"] == 1
    assert "processed" not in file_fallback.get_stats()

    assert file_after.read_log() == "foobar\n"


def test_midpoint_inline_destination_that_drops_all_messages_does_not_cause_message_to_be_unmatched(config, syslog_ng):
    # Rationale:
    #
    #   midpoint destinations which drop messages completely do it in their
    #   own context which should not affect the log path as a whole.  As far
    #   as the log path is concerned, the message was successfully
    #   dispatched to the destination.
    test_config = """

log {
    source(genmsg);
    destination {
        channel { filter { false(); }; };
    };
    destination(dest_after);
};
"""
    (file_true, file_false, file_after, file_fallback, file_dropall) = create_config(config, test_config)
    syslog_ng.start(config)

    assert "processed" not in file_dropall.get_stats()
    assert file_after.get_stats()["processed"] == 1
    assert "processed" not in file_fallback.get_stats()

    assert file_after.read_log() == "foobar\n"


def test_filter_between_destinations_that_drops_all_messages_causes_the_message_to_be_unmatched(config, syslog_ng):
    # Rationale:
    #
    #   In this case the filter() is part of the log path and the path is
    #   not processed completely because the filter dropped the message.
    #   The anticipated use-case here is that the first destination serves
    #   debugging purposes only, and the delivery of the message to that
    #   destination does not mean that the log path was successfully
    #   finished.

    test_config = """

log {
    source(genmsg);
    destination(dest_true);
    filter { false(); };
    destination(dest_after);
};
"""
    (file_true, file_false, file_after, file_fallback, _) = create_config(config, test_config)
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_after.get_stats()
    assert file_fallback.get_stats()["processed"] == 1

    assert file_true.read_log() == "foobar\n"
    assert file_fallback.read_log() == "foobar\n"


def test_junction_that_drops_messages_in_all_branches_causes_the_message_to_be_unmatched(config, syslog_ng):

    test_config = """

log {
    source(genmsg);
    destination(dest_true);
    junction {
        channel { filter { false(); }; };
        channel { filter { false(); }; };
    };
    destination(dest_after);
};
"""
    (file_true, file_false, file_after, file_fallback, _) = create_config(config, test_config)
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_after.get_stats()
    assert file_fallback.get_stats()["processed"] == 1

    assert file_true.read_log() == "foobar\n"
    assert file_fallback.read_log() == "foobar\n"


def test_junction_that_drops_messages_in_all_branches_causes_the_message_to_be_unmatched_even_if_they_reference_destinations(config, syslog_ng):

    test_config = """

log {
    source(genmsg);
    junction {
        channel {
            destination(dest_true);
            filter { false(); };
            destination(dest_false);
        };
        channel { filter { false(); }; };
    };
    destination(dest_after);
};
"""
    (file_true, file_false, file_after, file_fallback, _) = create_config(config, test_config)
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert "processed" not in file_after.get_stats()
    assert file_fallback.get_stats()["processed"] == 1

    assert file_true.read_log() == "foobar\n"
    assert file_fallback.read_log() == "foobar\n"


def test_junction_that_contains_message_dropping_destination_continues_to_deliver_messages_on_the_same_path(config, syslog_ng):

    test_config = """

log {
    source(genmsg);
    junction {
        channel {
            destination(dest_dropall);
            destination(dest_true);
        };
        channel { filter { false(); }; destination(dest_false); };
    };
    destination(dest_after);
};
"""
    (file_true, file_false, file_after, file_fallback, file_dropall) = create_config(config, test_config)
    syslog_ng.start(config)

    assert "processed" not in file_dropall.get_stats()
    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()

    assert file_after.get_stats()["processed"] == 1
    assert "processed" not in file_fallback.get_stats()

    assert file_true.read_log() == "foobar\n"
    assert file_after.read_log() == "foobar\n"
