import os

def is_running_in_build_tree():
    return 'SYSLOG_NG_BINARY' not in os.environ

def get_module_path_from_binary():
    syslog_ng_version_info = os.popen("%s --version" % get_syslog_ng_binary(), 'r').read()
    for line in syslog_ng_version_info.split("\n"):
        if 'Module-Path' in line:
            return line.split(" ")[-1]
    raise Exception("Failed to determine module paths from syslog-ng version")

def format_module_path_for_intree_modules():
    module_path = ''
    for (root, dirs, files) in os.walk(os.path.abspath(os.path.join(os.environ['top_builddir'], 'modules'))):
        module_path += ':'.join(map(lambda x: root + '/' + x + '/.libs', dirs))
        break
    return module_path

def get_module_path():
    if is_running_in_build_tree():
        return format_module_path_for_intree_modules()
    return get_module_path_from_binary()

def get_syslog_ng_binary():
    return os.getenv('SYSLOG_NG_BINARY', '../../syslog-ng/syslog-ng')

def is_premium():
    version = os.popen('%s -V' % get_syslog_ng_binary(), 'r').read()
    if version.find('premium-edition') != -1:
        return True
    return False

def has_module(module):
    avail_mods = os.popen('%s -V | grep ^Available-Modules: ' % get_syslog_ng_binary(), 'r').read()
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

