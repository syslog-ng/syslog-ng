#### Enable multiple systemd-journal() sources

Using multiple systemd-journal() sources are now possible as long as each source uses a unique systemd namespace. The namespace can be configured with the namespace() option, which has a default value of "*".