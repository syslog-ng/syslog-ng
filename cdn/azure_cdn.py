from pathlib import Path

from azure.core.polling import LROPoller
from azure.identity import ClientSecretCredential
from azure.mgmt.cdn import CdnManagementClient
from azure.mgmt.cdn.models import PurgeParameters

from .cdn import CDN


class AzureCDN(CDN):
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
