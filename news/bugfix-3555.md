`date-parser()`: fix hour-only timezone parsing

If the timestamp contains a short timezone offset (e.g. hours only), the
recent change in strptime() introduces an error, it interprets those
numbers as minutes instead of hours. For example: Jan 16 2019 18:23:12 +05
