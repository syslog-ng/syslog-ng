FROM ubuntu:focal

RUN apt update && apt install -y \
  python3-pip

RUN pip install \
  azure-storage-blob
