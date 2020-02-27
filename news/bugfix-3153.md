`python`: message interface changed to unicode from bytes in Python 3

Prior this change, the type of an object like `msg["MSG"]` was Bytes in Python. However, typically `LogMessage` carries strings, which causes inconvenience, because users need to explicitely convert to string (unicode), using `str()` or `.decode()`. This change simplifies the api by converting to unicode automatically.
Only Python3 is affected. In Python2 Bytes and string is the same, so there was no need for explicit conversion.
