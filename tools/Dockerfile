# The machine PC5 is built on: Ubuntu 24.04 with tools/packages.txt.
# tools/build.sh uses it when the computer itself isn't Ubuntu 24.04.
FROM ubuntu:24.04
COPY packages.txt /tmp/packages.txt
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends $(cat /tmp/packages.txt) && \
    rm -rf /var/lib/apt/lists/* /tmp/packages.txt && \
    git config --system --add safe.directory '*'
ENV LC_ALL=C TZ=UTC
WORKDIR /PC5
