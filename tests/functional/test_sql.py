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

    if not os.path.isfile("/usr/bin/sqlite3") and not os.path.isfile("/usr/local/bin/sqlite3"):
        print_user("no sqlite3 tool, skipping SQL test")
        return True

    soext='.so'
    if re.match('hp-ux', sys.platform):
        soext='.sl'
    if not os.path.isfile('/opt/syslog-ng/lib/dbd/libdbdsqlite3%s' % soext):
        print_user('No sqlite3 backend for libdbi. Skipping SQL test')

    messages = (
        'sql1',
        'sql2'
    )
    s = SocketSender(AF_INET, ('localhost', port_number), dgram=0)

    expected = []
    for msg in messages:
        expected.extend(s.sendMessages(msg, pri=7))
    time.sleep(15)
    stop_syslogng()
    time.sleep(5)
    return check_sql_expected("%s/test-sql.db" % current_dir, "logs", expected, settle_time=5, syslog_prefix="Sep  7 10:43:21 bzorp prog 12345")
