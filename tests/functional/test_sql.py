from globals import *
from log import *
from messagegen import *
from messagecheck import *
from control import flush_files, stop_syslogng

config = """@version: 3.0

options { ts_format(iso); chain_hostnames(no); keep_hostname(yes); };

source s_int { internal(); };
source s_tcp { tcp(port(%(port_number)d)); };

destination d_sql {
    sql(type(sqlite3) database("%(current_dir)s/test-sql.db") host(dummy) port(1234) username(dummy) password(dummy)
        table("logs")
        null("@NULL@")
        columns("date datetime", "host", "program", "pid", "msg")
        values("$DATE", "$HOST", "$PROGRAM", "${PID:-@NULL@}", "$MSG")
        indexes("date", "host", "program"));
};

log { source(s_tcp); destination(d_sql); };

""" % locals()

def test_sql():
    messages = (
        'sql1',
        'sql2'
    )
    s = SocketSender(AF_INET, ('localhost', port_number), dgram=0)

    expected = []
    for msg in messages:
        expected.extend(s.sendMessages(msg, pri=7))
    time.sleep(5)
    stop_syslogng()
    return check_sql_expected("%s/test-sql.db" % current_dir, "logs", expected, settle_time=5, syslog_prefix="Sep  7 10:43:21 bzorp prog 12345")
