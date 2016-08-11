import sys
sys.path.append('/home/micek/syslog-ng/tests/pytest/')
from src.work_with_sockets.socket_listener import SocketListener

def run_tcp_listener():
    sl = SocketListener()
    sl.tcp_listener("127.0.0.1", 5555)
    print(sl.get_received_messages())

run_tcp_listener()

def run_udp_listener():
    sl = SocketListener()
    sl.udp_listener("127.0.0.1", 5555)
    print(sl.get_received_messages())

run_udp_listener()

def run_unix_stream_listener():
    sl = SocketListener()
    sl.unix_stream_listener("/tmp/aaa")
    print(sl.get_received_messages())

run_unix_stream_listener()

def run_unix_dgram_listener():
    sl = SocketListener()
    sl.unix_dgram_listener("/tmp/bbb")
    print(sl.get_received_messages())

run_unix_dgram_listener()

def run_tcp6_listener():
    sl = SocketListener()
    sl.tcp6_listener("::1", 5555)
    print(sl.get_received_messages())

run_tcp6_listener()

def run_udp6_listener():
    sl = SocketListener()
    sl.udp6_listener("::1", 5555)
    print(sl.get_received_messages())

run_udp6_listener()
