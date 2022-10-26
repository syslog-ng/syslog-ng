`kubernetes()` source and `kubernetes-metadata-parser()`: these two
components gained the ability to enrich log messages with Kubernetes
metadata.  When reading container logs syslog-ng would query the Kubernetes
API for the following fields and add them to the log-message.  The returned
meta-data is cached in memory, so not all log messages trigger a new query.

        .k8s.pod_uuid
        .k8s.labels.<label_name>
        .k8s.namespace_name
        .k8s.pod_name
        .k8s.container_name
        .k8s.container_image
        .k8s.container_hash
        .k8s.docker_id
