Fix signal handling when an external library/plugin sets SIG_IGN

Previously, setting SIG_IGN in a plugin/library (for example, in a Python module) resulted in a crash.
