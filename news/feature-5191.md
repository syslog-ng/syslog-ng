affile: add support for filesize based logrotation

Config file example:
```
destination d_file_logrotate {
    file("/my-logfile.log", logrotate(enable(yes), size(30KB), rotations(5)));
};
```
