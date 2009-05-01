from globals import *
from log import *
from control import *
from messagegen import syslog_prefix

def check_expected(fname, messages, settle_time=1):

    flush_files(settle_time)
    print_user("Checking contents of output file: %s" % fname)
    if logstore_store_supported:
        iter = (False, True)
    else:
        iter = (False,)
    for logstore in iter:
            try:
                if logstore:
                        f = os.popen("../../src/logcat %s.lgs" % fname, "r")
                else:
                        f = open(fname + ".log", "r")
            except (IOError, OSError), e:
                print_user("Error opening file: %s, %s" % (fname, str(e)))
                return False

            lineno = 1
            # we handle message reordering by going through the complete
            # file and looking for the message
            # end the collected ranges must span the number of messages that
            # we expect
            matches = {}

            read_line = f.readline().strip()
            while read_line:
                if read_line[:len(syslog_prefix)] != syslog_prefix:
                    print_user("message does not start with syslog_prefix, file=%s:%d, line=%s" % (fname, lineno, read_line))
                    return False
                msg = read_line[len(syslog_prefix):]
                m = re.match("^ (\S+) (\d+)/(\d+)", msg)

                if not m:
                    print_user("message payload unexpected format, file=%s:%d, line=%s" % (fname, lineno, read_line))
                    return False
                msg = m.group(1)
                session = int(m.group(2))
                id = int(m.group(3))
                if not matches.has_key((msg, session)):
                    if id != 1:
                        print_user("the id of the first message in a session is not 1, session=%d, id=%d, file=%s:%d, line=%s"  % (session, id, fname, lineno, read_line))
                        return False
                else:
                    if matches[(msg, session)] != id - 1:
                        print_user("message reordering/drop detected in the same session, session=%d, id=%d, expected_id=%d, file=%s:%d, line=%s" % (session, id, matches[(msg, session)]+1, fname, lineno, read_line))
                        return False
                matches[(msg, session)] = id
                read_line = f.readline().strip()
                lineno = lineno + 1

            for (msg, session, count) in messages:

                if not matches.has_key((msg, session)):
                    print_user("output log files lack this kind of message: %s session: %d, count: %d" % (msg, session, count))
                    return False
                if matches[(msg, session)] != count - 1:
                    print_user("not enough messages found, message: %s, session: %d, last_id: %d, count: %d" % (msg, session,  matches[(msg, session)], count))
                    return False
    return True
