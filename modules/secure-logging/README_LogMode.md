# logmode

The classic secure logging has been expanded and does now provide a logmode:

- LOGMODE_PLAIN_DIRECT
- LOGMODE_PLAIN_BASE64
- LOGMODE_ENCRYPTED

The logmode is just an enumeration value provided in utils_slog.h.

```
enum LogMode
 {
   LOGMODE_INVALID,      //-- uninitialized, ERROR
   LOGMODE_PLAIN_DIRECT, //-- direct, do NOT encrypt and provide log message in plain text
   LOGMODE_PLAIN_BASE64, //-- base64, do NOT encrypt and provide log message Base64 encoded
   LOGMODE_ENCRYPTED,    //-- enc, do encrypt and provide encrypted log message Base64 encoded
};
```
In the syslog-ng.conf file the logmode is set by --logmode and one of the key words
(direct|base64|enc) inside the template slog.
In case it is missing, --logmode enc is automatically used silently which behaves like old
secure-logging without plain mode.


## LOGMODE_PLAIN_DIRECT
LOGMODE_PLAIN_DIRECT (--logmode direct in syslog-ng.conf) ensures that
log messages are not encrypted and provided as plain text as they are.
Additional line and checksum information is added for each message Base64 encoded to detected manipulation.

## LOGMODE_PLAIN_BASE64
LOGMODE_PLAIN_BASE64 (--logmode base64 in syslog-ng.conf) ensures that
log messages are not encrypted but provided encoded in Base64.
Additional line and checksum information is added for each message Base64 encoded to detected manipulation.

## LOGMODE_ENCRYPTED
LOGMODE_ENCRYPTED (--logmode enc in syslog-ng.conf) is the classic way of secure logging before 
the logmode was introduced.
Log message are encrypted and the encrypted messages are provided Base64 encoded.
Additional line and checksum information is added for each message Base64 encoded to detected manipulation.



# Example Configuration syslog-ng

## syslog-ng.conf for LOGMODE_PLAIN_DIRECT (direct)

```
@version: 4.8
@include "scl.conf"
@define mypath "/tmp/test_slog/data"

options {
    flush_lines (0);
    time_reopen (10);
    log_fifo_size (1000);
    chain_hostnames (off);
    use_dns (no);
    use_fqdn (no);
    create_dirs (no);
    keep_hostname (yes);
    use-uniqid(yes);
};

source s_local {
        system();
        internal();
};

source s_network {
        network(
                transport("udp")
                port(7777)
                flags(store-raw-message)
        );
};

template slog {
        template("$(slog --key-file `mypath`/host.key --mac-file `mypath`/mac.dat --logmode direct $RAWMSG)\n");
};

destination d_local {
        file(`mypath`/messages.slog template(slog));
};

log {
        source(s_network);
        destination(d_local);
};
```


## syslog-ng.conf for LOGMODE_PLAIN_BASE64 (base64)

For base64, the logmode is set to **base64**
```
template slog {
        template("$(slog --key-file `mypath`/host.key --mac-file `mypath`/mac.dat --logmode base64 $RAWMSG)\n");
   };
```


## syslog-ng.conf for LOGMODE_ENCRYPTED (enc)

To provide only encrypted log messages, the logmode is to be set to **enc**:
```
   template slog {
        template("$(slog --key-file `mypath`/host.key --mac-file `mypath`/mac.dat --logmode enc $RAWMSG)\n");
   };
```


# Configuration Console Tools

The log mode can be provided to console tools slogencrypt and slogverify.
In case the argument --logmode is missing, --logmode enc is used silently
which provides the same behaviour as in old secure-logging without having
the plain mode.

## slogencrypt

```
./slogencrypt --help
Usage:
  slogencrypt [OPTION…] NEWKEY NEWMAC INPUTLOG OUTPUTLOG [COUNTER] - Encrypt log files using secure logging

Example:
  ./slogencrypt
  --key-file ./current_host.key
  --mac-file ./current_mac.dat
  --logmode direct
  ./new_host.key
  ./new_mac.dat
  ./input_log.txt
  ./output_log.out


Help Options:
  -h, --help                Show help options

Application Options:
  -k, --key-file=FILE       Current host key file
  -m, --mac-file=FILE       Current MAC file
  -l, --logmode=LOGMODE     Log mode (direct|base64|enc) whether log messages shall be provided as they are in plain text or only encoded in Base64 or encrypted
```

## slogverify

```
./slogverify --help
Usage:
  slogverify [OPTION…] INPUTLOG OUTPUTLOG [COUNTER] - Log archive verification

Examples:
  normal mode:
    ./slogverify
    --key-file ./host.key
    --mac-file ./mac.dat
    --logmode enc
    ./messages.slog
    ./messages_verified.txt

  iterative mode:
    ./slogverify -i
    --prev-key-file ./host0.key
    --prev-mac-file ./mac0.dat
    --mac-file ./mac1.dat
    --logmode enc
    ./plainlog_1.out
    ./plainlog_1.chk


Help Options:
  -h, --help                   Show help options

Application Options:
  -i, --iterative              Iterative verification
  -k, --key-file=FILE          Initial host key file
  -m, --mac-file=FILE          Current MAC file
  -l, --logmode=LOGMODE        Log mode (direct|base64|enc) whether log is expected as is (direct) or plain but Base64 encoded or encrypted
  -p, --prev-key-file=FILE     Previous host key file in iterative mode
  -r, --prev-mac-file=FILE     Previous MAC file in iterative mode
```

Note: The logmode for verification must fit the one used for slogencrypt.


# man pages

```
doc/man/local/secure-logging.7
doc/man/local/slogencrypt.1
doc/man/local/slogverify.1
```

## Generate manuals by build
Ensure -DENABLE_MANPAGES=on in the build configuration

## Read manual

```
m@air:~/Software/install/share/man/man1$ ls
dqtool.1  loggen.1  pdbtool.1  persist-tool.1  slogencrypt.1  slogkey.1  slogverify.1  syslog-ng-ctl.1  syslog-ng-debun.1

m@air:~/Software/install/share/man/man1$ man ./slogencrypt.1

m@air:~/Software/install/share/man/man1$ man ./slogverify.1
```

```
m@air:~/Software/install/share/man$ cd man7
m@air:~/Software/install/share/man/man7$ ls
secure-logging.7
m@air:~/Software/install/share/man/man7$ man ./secure-logging.7
```

