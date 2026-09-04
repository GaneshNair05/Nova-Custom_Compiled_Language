FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# 1. Install base utilities, LLVM, build tools, and Raylib dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    curl \
    ca-certificates \
    build-essential \
    cmake \
    git \
    llvm-dev \
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

# 5. Build the Nova compiler
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --config Release

# 6. Ensure binary execution permissions
RUN chmod +x /app/build/mycompiler

ENV PORT=4000
ENV NOVA_COMPILER_PATH=/app/build/mycompiler

EXPOSE 4000
CMD ["npm", "start"]