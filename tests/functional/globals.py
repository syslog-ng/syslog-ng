import os

def is_premium():
    version = os.popen('../../syslog-ng/syslog-ng -V', 'r').read()
    if version.find('premium-edition') != -1:
        return True
    return False

def has_module(module):
    avail_mods = os.popen('../../syslog-ng/syslog-ng -V | grep ^Available-Modules: ', 'r').read()
    if avail_mods.find(module) != -1:
        return True
    return False


is_premium_edition = is_premium()
if is_premium_edition:
    logstore_store_supported = True
    wildcard_file_source_supported = True
else:
    logstore_store_supported = False
    wildcard_file_source_supported = False

port_number = os.getpid() % 30000 + 33000
ssl_port_number = port_number + 1
port_number_syslog = port_number + 2
port_number_network = port_number + 3

current_dir = os.getcwd()
try:
    src_dir = os.environ["srcdir"]
except KeyError:
    src_dir = current_dir

