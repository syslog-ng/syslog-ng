from pathlib import Path

from azure.core.polling import LROPoller
from azure.identity import ClientSecretCredential
from azure.mgmt.cdn import CdnManagementClient
from azure.mgmt.cdn.models import PurgeParameters

from .cdn import CDN


class AzureCDN(CDN):
    """
    A `CDN` implementation that can connect to an Azure CDN instance.

    Example config:

    ```yaml
    vendor: "azure"
    all:
      resource-group-name: "secret1"
      profile-name: "secret2"
      endpoint-name: "secret3"
      endpoint-subscription-id: "secret4"
      tenant-id: "secret5"
      client-id: "secret6"
      client-secret: "secret7"
    ```
    """

    def __init__(
        self,
        resource_group_name: str,
        profile_name: str,
        endpoint_name: str,
        endpoint_subscription_id: str,
        tenant_id: str,
        client_id: str,
        client_secret: str,
    ):
        credential = ClientSecretCredential(
            tenant_id=tenant_id,
            client_id=client_id,
            client_secret=client_secret,
        )
        self.__cdn = CdnManagementClient(
            credential=credential,
            subscription_id=endpoint_subscription_id,
        )

        self.__resource_group_name = resource_group_name
        self.__profile_name = profile_name
        self.__endpoint_name = endpoint_name

        super().__init__()

    @staticmethod
    def get_config_keyword() -> str:
        return "azure"

    @staticmethod
    def from_config(cfg: dict) -> CDN:
        return AzureCDN(
            resource_group_name=cfg["resource-group-name"],
            profile_name=cfg["profile-name"],
            endpoint_name=cfg["endpoint-name"],
            endpoint_subscription_id=cfg["endpoint-subscription-id"],
            tenant_id=cfg["tenant-id"],
            client_id=cfg["client-id"],
            client_secret=cfg["client-secret"],
        )

    def refresh_cache(self, path: Path) -> None:
        path_str = str(Path("/", path))

        self._log_info("Refreshing CDN cache.", path=path_str)

        poller: LROPoller = self.__cdn.endpoints.begin_purge_content(
            resource_group_name=self.__resource_group_name,
            profile_name=self.__profile_name,
            endpoint_name=self.__endpoint_name,
            content_file_paths=PurgeParameters(content_paths=[path_str]),
        )
        poller.wait()

        status = poller.status()
        if not status == "Succeeded":
            raise Exception("Failed to refresh CDN cache. status: {}".format(status))

        self._log_info("Successfully refreshed CDN cache.", path=path_str)
