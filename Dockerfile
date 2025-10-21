# Dockerfile.hybf
# Use Ubuntu as base image
FROM ubuntu:22.04

# Set environment variables to prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

# Install basic dependencies including Python scientific computing prerequisites
RUN apt-get update && apt-get install -y \
    git \
    bash \
    curl \
    wget \
    build-essential \
    python3 \
    python3-pip \
    python3-dev \
    python3-venv \
    pkg-config \
    libopenblas-dev \
    liblapack-dev \
    gfortran \
    time \
    && rm -rf /var/lib/apt/lists/*

# Upgrade pip to latest version
RUN python3 -m pip install --upgrade pip

# Install Python scientific libraries
RUN python3 -m pip install \
    numpy \
    scipy \
    matplotlib

# Set working directory
WORKDIR /app
# Create volume mount point for external code
VOLUME ["/app"]

# # Make scripts executable
# RUN chmod +x scripts/install-cmake.sh \
#     && chmod +x scripts/run-cmake.sh \
#     && chmod +x scripts/build-install.sh

# # Run the installation and build scripts
# # Need to source the cmake installation and run subsequent commands in the same RUN layer
# # to preserve the PATH environment variable
# RUN scripts/install-cmake.sh && \
#     export PATH=$(realpath cmake-3.21.4-linux-x86_64/bin):$PATH && \
#     bash scripts/run-cmake.sh && \
#     bash scripts/build-install.sh

# # Permanently add cmake to PATH for runtime
# ENV PATH="/app/cmake-3.21.4-linux-x86_64/bin:${PATH}"

# Set the default command (you may want to adjust this based on what the built application does)
CMD ["/bin/bash"]