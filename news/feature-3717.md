`discord()` destination

syslog-ng now has a webhook-based Discord destination.
Example usage:
```
destination {
  discord(url("https://discord.com/api/webhooks/x/y"));
};
```

The following options can be used to customize the destination further:

`avatar-url()`, `username("$HOST-bot")`, `tts(true)`, `template("${MSG:-[empty message]}")`.
