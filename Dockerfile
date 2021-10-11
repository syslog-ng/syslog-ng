FROM ubuntu:focal

RUN apt update && apt install -y \
  python3-pip \
  apt-utils

RUN pip install \
  azure-storage-blob \
  azure-mgmt-cdn \
  azure-identity
