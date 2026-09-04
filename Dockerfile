FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# 1. Install LLVM 18, build tools, zlib, and graphics libraries directly from standard Ubuntu 24.04 repos
RUN apt-get update && apt-get install -y --no-install-recommends \
    curl \
    ca-certificates \
    build-essential \
    cmake \
    git \
    llvm-dev \
    clang \
    zlib1g-dev \
    libzstd-dev \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    libx11-dev \
    libxcursor-dev \
    libxinerama-dev \
    libxrandr-dev \
    libxi-dev \
    libasound2-dev \
    && rm -rf /var/lib/apt/lists/*

# 2. Install Node.js 20.x
RUN curl -fsSL https://deb.nodesource.com/setup_20.x | bash - \
    && apt-get install -y nodejs \
    && rm -rf /var/lib/apt/lists/*

# 2b. Build and install raylib from source — it isn't in Ubuntu's apt repos
# (only an unrelated "python3-xraylib" X-ray physics package matches that
# name), so codegen.cpp's `#include <raylib.h>` has nothing to resolve
# against without this. Installs raylib.h/rlgl.h/raymath.h to
# /usr/local/include and libraylib.a + a raylib-config.cmake to
# /usr/local/lib, so both a manual `-lraylib` and CMake's
# `find_package(raylib)` will find it. Static build (raylib's CMake
# default) keeps the final image from needing libraylib.so at runtime.
# The X11/GL/ALSA -dev packages installed in step 1 are raylib's actual
# build dependencies on Linux — that's why they were already there.
RUN git clone --depth 1 --branch 5.5 https://github.com/raysan5/raylib.git /tmp/raylib \
    && cmake -S /tmp/raylib -B /tmp/raylib/build -DBUILD_EXAMPLES=OFF -DCMAKE_BUILD_TYPE=Release \
    && cmake --build /tmp/raylib/build -j$(nproc) \
    && cmake --install /tmp/raylib/build \
    && rm -rf /tmp/raylib

WORKDIR /app

# 3. Install backend dependencies
COPY package*.json ./
RUN npm install

# 4. Copy source files
COPY . .

# 5. Build compiler (CMake will automatically discover LLVM 18)
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --config Release

# 6. Set binary execution permissions
RUN chmod +x /app/build/mycompiler

ENV PORT=4000
ENV NOVA_COMPILER_PATH=/app/build/mycompiler

EXPOSE 4000
CMD ["npm", "start"]