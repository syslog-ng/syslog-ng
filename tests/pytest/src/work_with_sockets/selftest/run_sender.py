import sys
import time
sys.path.append('/home/micek/syslog-ng/tests/pytest/')
from src.work_with_sockets.socket_sender import SocketSender

def run_tcp_sender():
    ss = SocketSender()
    ss.tcp_send("127.0.0.1", 5555, "test message")

run_tcp_sender()

time.sleep(1)

def run_udp_sender():
    ss = SocketSender()
    ss.udp_send("127.0.0.1", 5555, "test message2")

run_udp_sender()

time.sleep(1)

def run_unix_stream_sender():
    ss = SocketSender()
    ss.unix_stream_send("/tmp/aaa", "test_message3")

run_unix_stream_sender()

time.sleep(1)

def run_unix_dgram_sender():
    ss = SocketSender()
    ss.unix_dgram_send("/tmp/bbb", "test_message4")

run_unix_dgram_sender()

time.sleep(1)

def run_tcp6_sender():
    ss = SocketSender()
    ss.tcp6_send("::1", 5555, "test_message5")

run_tcp6_sender()

time.sleep(1)

def run_udp6_sender():
    ss = SocketSender()
    ss.udp6_send("::1", 5555, "test_message6")

run_udp6_sender()

