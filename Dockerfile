FROM ubuntu:focal

RUN apt-get update && apt-get install -y \
  python3-pip \
  apt-utils \
  gnupg2

RUN pip install \
  azure-storage-blob \
  azure-mgmt-cdn \
  azure-identity \
  pyyaml \
  types-PyYAML
