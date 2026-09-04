FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# 1. Install base utilities, wget, GPG, and build tools
RUN apt-get update && apt-get install -y --no-install-recommends \
    curl \
    wget \
    gnupg \
    ca-certificates \
    build-essential \
    cmake \
    git \
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

# 2. Install LLVM 17 from official llvm.sh script
RUN wget https://apt.llvm.org/llvm.sh \
    && chmod +x llvm.sh \
    && ./llvm.sh 17 \
    && apt-get install -y --no-install-recommends libpolly-17-dev \
    && rm -f llvm.sh \
    && rm -rf /var/lib/apt/lists/*

# Point LLVM_DIR / CMake to LLVM 17
ENV LLVM_DIR=/usr/lib/llvm-17/cmake

# 3. Install Node.js 20.x
RUN curl -fsSL https://deb.nodesource.com/setup_20.x | bash - \
    && apt-get install -y nodejs \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# 4. Install backend dependencies
COPY package*.json ./
RUN npm install

# 5. Copy source files
COPY . .

# 6. Build the compiler with CMake pointing to LLVM 17
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release -DLLVM_DIR=/usr/lib/llvm-17/cmake \
    && cmake --build build --config Release

# 7. Ensure executable permissions
RUN chmod +x /app/build/mycompiler

ENV PORT=4000
ENV NOVA_COMPILER_PATH=/app/build/mycompiler

EXPOSE 4000
CMD ["npm", "start"]