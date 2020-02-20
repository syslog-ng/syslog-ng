`python`: Added `--with-python3-home` configure option to use a hard-coded PYTHONHOME for Python-based plugins

This can be useful when a Python interpreter is bundled with syslog-ng.
Relocation is supported, for example: --with-python3-home='${exec_prefix}'
