#!/bin/bash
# Download and use a precompiled binary distribution

wget https://github.com/Kitware/CMake/releases/download/v3.31.9/cmake-3.31.9-linux-x86_64.tar.gz
tar xf cmake-3.31.9-linux-x86_64.tar.gz
rm -f cmake-3.31.9-linux-x86_64.tar.gz
export PATH=$(realpath cmake-3.31.9-linux-x86_64/bin):$PATH
