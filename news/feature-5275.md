`cloud-auth`: Added azure_monitor_builtin and azure_monitor_custom destinations

Added oauth2 authentication for azure monitor destinations.

Example usage:
```
azure-monitor-custom(table-name("table")
                     dcr-id("dcr id")
                     dce-uri("dce uri")
                     auth(tenant-id("tenant id")
                          app-id("app id")
                          app-secret("app secret")))
```

Note: Table name should not contain the trailing "_CL" string for custom tables.