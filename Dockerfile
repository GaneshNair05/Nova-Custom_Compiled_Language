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