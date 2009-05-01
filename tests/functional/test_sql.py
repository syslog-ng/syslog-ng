from globals import *
from log import *
from messagegen import *
from messagecheck import *
from control import flush_files

config = """@version: 3.0

options { use_dns(no); };

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
    s = SocketSender(AF_INET, ('localhost', port_number), dgram=0)

    expected = s.sendMessages("sql1", pri=7)
    flush_files(settle_time=3)
    expected = s.sendMessages("sql2", pri=7)
    return True
