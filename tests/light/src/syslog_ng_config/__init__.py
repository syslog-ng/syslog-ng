
def stringify(s):
    return '"' + s.replace('\\', "\\\\").replace('"', '\\"').replace('\n', '\\n') + '"'
