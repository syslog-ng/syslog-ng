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
from src.common.blocking import wait_until_true
from src.syslog_ng_config.renderer import render_statement


def create_config(config, test_config, msg="foobar"):
    file_true = config.create_file_destination(file_name="dest-true.log", template="'$MSG\n'")
    file_false = config.create_file_destination(file_name="dest-false.log", template="'$MSG\n'")
    file_after = config.create_file_destination(file_name="dest-after.log", template="'$MSG\n'")
    file_fallback = config.create_file_destination(file_name="dest-fallback.log", template="'$MSG\n'")

    preamble = f"""
@version: {config.get_version()}

options {{ stats(level(1)); }};

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

log {{
    source(genmsg);
    destination(dest_fallback);
    flags(fallback);
}};

"""
    config.set_raw_config(preamble + test_config)
    return (file_true, file_false, file_after, file_fallback)


def test_simple_if(config, syslog_ng):
    test_config = """

log {
    source(genmsg);
    if (true()) {
        destination(dest_true);
    } else {
        destination(dest_false);
    };

    destination(dest_after);
};
"""
    (file_true, file_false, file_after, file_fallback) = create_config(config, test_config)
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_after.get_stats()["processed"] == 1
    assert "processed" not in file_fallback.get_stats()

    assert file_true.read_log() == "foobar\n"
    assert file_after.read_log() == "foobar\n"


def test_simple_if_negated(config, syslog_ng):
    test_config = """

log {
    source(genmsg);
    if (false()) {
        destination(dest_true);
    } else {
        destination(dest_false);
    };

    destination(dest_after);
};
"""
    (file_true, file_false, file_after, file_fallback) = create_config(config, test_config)
    syslog_ng.start(config)

    assert "processed" not in file_true.get_stats()
    assert file_false.get_stats()["processed"] == 1
    assert file_after.get_stats()["processed"] == 1
    assert "processed" not in file_fallback.get_stats()
    assert file_false.read_log() == "foobar\n"
    assert file_after.read_log() == "foobar\n"


def test_simple_if_that_drops_in_all_branches(config, syslog_ng):
    test_config = """

log {
    source(genmsg);
    if (true()) {
        filter { false(); };
        destination(dest_true);
    } else {
        destination(dest_false);
    };

    destination(dest_after);
};
"""
    (file_true, file_false, file_after, file_fallback) = create_config(config, test_config)
    syslog_ng.start(config)

    assert "processed" not in file_true.get_stats()
    assert "processed" not in file_false.get_stats()
    assert "processed" not in file_after.get_stats()
    assert file_fallback.get_stats()["processed"] == 1

    assert file_fallback.read_log() == "foobar\n"


def test_compound_if(config, syslog_ng):
    test_config = """

log {
    source(genmsg);
    if {
        filter { true(); };
        destination(dest_true);
    } else {
        destination(dest_false);
    };

    destination(dest_after);
};
"""
    (file_true, file_false, file_after, file_fallback) = create_config(config, test_config)
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_after.get_stats()["processed"] == 1
    assert "processed" not in file_fallback.get_stats()

    assert file_true.read_log() == "foobar\n"
    assert file_after.read_log() == "foobar\n"


def test_compound_if_negated_continues_in_the_false_expr(config, syslog_ng):
    test_config = """

log {
    source(genmsg);
    if {
        filter { false(); };
        destination(dest_true);
    } else {
        destination(dest_false);
    };

    destination(dest_after);
};
"""
    (file_true, file_false, file_after, file_fallback) = create_config(config, test_config)
    syslog_ng.start(config)

    assert "processed" not in file_true.get_stats()
    assert file_false.get_stats()["processed"] == 1
    assert file_after.get_stats()["processed"] == 1
    assert "processed" not in file_fallback.get_stats()
    assert file_false.read_log() == "foobar\n"
    assert file_after.read_log() == "foobar\n"


def test_compound_if_that_drops_in_all_branches(config, syslog_ng):
    test_config = """

log {
    source(genmsg);
    if {
        filter { false(); };
        destination(dest_true);
    } else {
        filter { false(); };
        destination(dest_false);
    };

    destination(dest_after);
};
"""
    (file_true, file_false, file_after, file_fallback) = create_config(config, test_config)
    syslog_ng.start(config)

    assert "processed" not in file_true.get_stats()
    assert "processed" not in file_false.get_stats()
    assert "processed" not in file_after.get_stats()
    assert file_fallback.get_stats()["processed"] == 1

    assert file_fallback.read_log() == "foobar\n"


def test_compound_if_with_messages_dropped_after_if(config, syslog_ng):
    test_config = """

log {
    source(genmsg);
    if {
        filter { true(); };
        destination(dest_true);
    } else {
        destination(dest_false);
    };

    filter { false(); };
    destination(dest_after);
};
"""
    (file_true, file_false, file_after, file_fallback) = create_config(config, test_config)
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert "processed" not in file_after.get_stats()
    assert file_fallback.get_stats()["processed"] == 1

    assert file_true.read_log() == "foobar\n"
    assert file_fallback.read_log() == "foobar\n"


def test_three_levels_of_conditionals_drop_after_the_innermost_if(config, syslog_ng):
    test_config = """

log {
    source(genmsg);
    if {
        if {
            if {
                destination(dest_true);
            } else {
                destination(dest_false);
            };
            filter { false(); };
            destination(dest_false);
        } else {
            destination(dest_true);
        };
    } else {
        destination(dest_false);
    };
    destination(dest_after);
};
"""
    (file_true, file_false, file_after, file_fallback) = create_config(config, test_config)
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 2
    assert "processed" not in file_false.get_stats()
    assert file_after.get_stats()["processed"] == 1

    assert file_true.read_log() == "foobar\n"
    assert file_after.read_log() == "foobar\n"


def test_three_levels_of_conditionals_drop_after_the_middle_if(config, syslog_ng):
    test_config = """

log {
    source(genmsg);
    if {
        if {
            if {
                destination(dest_true);
            } else {
                destination(dest_false);
            };
        } else {
            destination(dest_false);
        };
        filter { false(); };
        destination(dest_false);
    } else {
        destination(dest_true);
    };
    destination(dest_after);
};
"""
    (file_true, file_false, file_after, file_fallback) = create_config(config, test_config)
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 2
    assert "processed" not in file_false.get_stats()
    assert file_after.get_stats()["processed"] == 1

    assert file_true.read_log() == "foobar\n"
    assert file_after.read_log() == "foobar\n"


def test_three_levels_of_simple_conditionals_drop_after_the_middle_if(config, syslog_ng):
    test_config = """

log {
    source(genmsg);
    if {
        if (true()) {
            if (true()) {
                if (true()) {
                    destination(dest_true);
                } else {
                    destination(dest_false);
                };
            } else {
                destination(dest_false);
            };
            filter { false(); };
            destination(dest_false);
        } else {
            destination(dest_false);
        };
    } else {
        destination(dest_true);
    };
    destination(dest_after);
};
"""
    (file_true, file_false, file_after, file_fallback) = create_config(config, test_config)
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 2
    assert "processed" not in file_false.get_stats()
    assert file_after.get_stats()["processed"] == 1

    assert file_true.read_log() == "foobar\n"
    assert file_after.read_log() == "foobar\n"


def test_three_levels_of_conditionals_drop_after_the_innermost_junction(config, syslog_ng):
    test_config = """

log {
    source(genmsg);
    junction {
        channel {
            junction {
                channel {
                    junction {
                        channel {
                            destination(dest_true);
                            flags(final);
                        };
                        channel {
                            destination(dest_false);
                            flags(final);
                        };
                    };
                    filter { false(); };
                    destination(dest_false);
                    flags(final);
                };
                channel {
                    destination(dest_after);
                    flags(final);
                };
            };
            flags(final);
        };
        channel {
            destination(dest_false);
            flags(final);
        };
    };
};
"""
    (file_true, file_false, file_after, file_fallback) = create_config(config, test_config)
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_after.get_stats()["processed"] == 1
    assert "processed" not in file_fallback.get_stats()

    assert file_true.read_log() == "foobar\n"
    assert file_after.read_log() == "foobar\n"


def test_three_levels_of_conditionals_drop_after_the_middle_junction(config, syslog_ng):
    test_config = """

log {
    source(genmsg);
    junction {
        channel {
            junction {
                channel {
                    junction {
                        channel {
                            destination(dest_true);
                            flags(final);
                        };
                        channel {
                            destination(dest_false);
                            flags(final);
                        };
                    };
                    flags(final);
                };
                channel {
                    destination(dest_false);
                    flags(final);
                };
            };
            filter { false(); };
            destination(dest_false);
            flags(final);
        };
        channel {
            destination(dest_true);
            flags(final);
        };
    };
    destination(dest_after);
};
"""
    (file_true, file_false, file_after, file_fallback) = create_config(config, test_config)
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 2
    assert "processed" not in file_false.get_stats()
    assert file_after.get_stats()["processed"] == 1
    assert "processed" not in file_fallback.get_stats()

    assert file_true.read_log() == "foobar\n"
    assert file_after.read_log() == "foobar\n"


def test_consuming_messages_into_correlation_state_does_not_cause_else_branches_to_be_processed(config, syslog_ng):
    test_config = """

log {
    # generate two messages
    source(genmsg);
    source(genmsg);

    rewrite {
        set("PROG" value("PROGRAM"));
    };

    if {
        parser {
            grouping-by(key("$PROGRAM")
                timeout(1)
                trigger("$(context-length)" == 2)
                aggregate(value("MESSAGE", "$MSG correlated\n"))
                inject-mode(aggregate-only));
        };
        destination(dest_true);
    } else {
        destination(dest_false);
    };
    destination(dest_after);
};
"""
    (file_true, file_false, file_after, file_fallback) = create_config(config, test_config)
    syslog_ng.start(config)

    assert wait_until_true(lambda: "processed" in file_true.get_stats() and file_true.get_stats()["processed"] == 1)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_after.get_stats()["processed"] == 1
    assert "processed" not in file_fallback.get_stats()

    assert file_true.read_log() == "foobar correlated\n"
    assert file_after.read_log() == "foobar correlated\n"
