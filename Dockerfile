FROM --platform=linux/amd64 ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    wget \
    xz-utils \
    git \
    && rm -rf /var/lib/apt/lists/*

# Download and install steward-fu's mini toolchain
RUN mkdir -p /opt && \
    cd /tmp && \
    wget --no-check-certificate https://github.com/steward-fu/website/releases/download/miyoo-mini/mini_toolchain-v1.0.tar.gz && \
    tar xf mini_toolchain-v1.0.tar.gz -C /opt && \
    rm mini_toolchain-v1.0.tar.gz

# Set up environment
ENV PATH="/opt/mini/bin:${PATH}"
ENV CROSS=/opt/mini/bin/arm-linux-gnueabihf-

WORKDIR /build
