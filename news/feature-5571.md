`cloud-auth`: Add generic OAuth2 authentication module

A generic OAuth2 authentication module supporting client credentials flow with configurable authentication methods (HTTP Basic Auth or POST body credentials) has been added. The module is extensible via virtual methods, allowing future authenticators to inherit common OAuth2 token management logic.
