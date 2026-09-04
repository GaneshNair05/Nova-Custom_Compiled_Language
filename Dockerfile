FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install LLVM, build tools, Raylib, and Node.js
RUN apt-get update && apt-get install -y \
    curl \
    build-essential \
    cmake \
    llvm-dev \
    libraylib-dev \
    && curl -fsSL https://deb.nodesource.com/setup_20.x | bash - \
    && apt-get install -y nodejs \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Install Node dependencies
COPY package*.json ./
RUN npm install

# Copy source files
COPY . .

# Build your C++ compiler using your root CMakeLists.txt
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --config Release

# Ensure the compiled Linux binary is executable
# (assuming CMake produces 'mycompiler' inside /app/build/)
RUN chmod +x /app/build/mycompiler

ENV PORT=4000
ENV NOVA_COMPILER_PATH=/app/build/mycompiler

EXPOSE 4000
CMD ["npm", "start"]