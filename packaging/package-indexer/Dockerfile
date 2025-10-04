FROM ubuntu:jammy

RUN apt-get update && apt-get install -y \
  python3-pip \
  apt-utils \
  gnupg2 \
  createrepo-c \
  python3-pexpect \
  rpm \
  wget

RUN pip install \
  azure-storage-blob \
  azure-mgmt-cdn \
  azure-identity \
  pyyaml \
  types-PyYAML

# https://github.com/glowinthedark/index-html-generator
RUN wget -O /usr/local/bin/genindex.py https://raw.githubusercontent.com/glowinthedark/index-html-generator/refs/heads/master/genindex.py || true
# https://github.com/joshbrunty/Indexer
RUN wget -O /usr/loca/bin/indexer.py https://raw.githubusercontent.com/joshbrunty/Indexer/refs/heads/main/indexer.py || true
