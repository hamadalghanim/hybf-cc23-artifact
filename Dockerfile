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
    libsdl2-dev \
    time \
    gdb \
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

RUN echo "set auto-load safe-path /" > /root/.gdbinit

# # Make scripts executable
# RUN chmod +x scripts/install-cmake.sh \
#     && chmod +x scripts/run-cmake.sh \
#     && chmod +x scripts/build-install.sh

# # Run the installation and build scripts
# # Need to source the cmake installation and run subsequent commands in the same RUN layer
# # to preserve the PATH environment variable

RUN wget https://github.com/Kitware/CMake/releases/download/v3.31.9/cmake-3.31.9-linux-x86_64.tar.gz && \
    tar xf cmake-3.31.9-linux-x86_64.tar.gz && \
    rm -f cmake-3.31.9-linux-x86_64.tar.gz && \
    mv cmake-3.31.9-linux-x86_64 /opt/cmake

# Add CMake to PATH
ENV PATH="/opt/cmake/bin:${PATH}"

# Set the default command (you may want to adjust this based on what the built application does)
CMD ["/bin/bash"]