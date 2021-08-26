`mqtt()`: username/password authentication

Example config:
```
mqtt(
  address("tcp://localhost:1883")
  topic("syslog/messages")
  username("user")
  password("passwd")
);
```

Note: The password is transmitted in cleartext without using `ssl://` or `wss://`.
