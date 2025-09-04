packaging: New package formats, platforms, and architectures!

- the long-awaited RPM repository is here, we have RHEL-8, RHEL-9, and REHL-10 packages available, both for amd64 and arm64 architectures,\
  just download and install the repository definition

    ``` shell
    sudo curl -o /etc/yum.repos.d/syslog-ng-ose-stable.repo https://ose-repo.syslog-ng.com/yum/syslog-ng-ose-stable.repo
    ```

- we fixed the publishing of our arm64 DEB packages
- added new DEB packages for Debian Trixie, both for amd64 and arm64.
- new DBLD docker images for Rocky-9, OpenSuse Tumbleweed, Ubuntu Plucky, and Debian Trixie
