`set-facility()`: add new rewrite operation to change the syslog facility
associated with the message.

<details>
  <summary>Example config, click to expand!</summary>

```

log {
    source { system(); };
    if (program("postfix")) {
      rewrite { set-facility("mail"); };
    };
    destination { file("/var/log/mail.log"); };
    flags(flow-control);
};
```
</details>
