# Stage 1: Build the native C++ Engine
FROM node:22-slim as builder
RUN apt-get update && apt-get install -y cmake g++ make curl git

WORKDIR /app
# We need to copy the engine code to build it
COPY engine /app/engine

WORKDIR /app/engine
RUN mkdir build && cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make -j4

# Stage 2: Node.js server
FROM node:22-slim

WORKDIR /app

COPY server/package*.json ./
RUN npm install

# Copy the built engine
COPY --from=builder /app/engine/build/pawnGO /app/engine/pawnGO
COPY --from=builder /app/engine/build/nn-62ef826d1a6d.nnue /app/engine/nn-62ef826d1a6d.nnue

# Ensure pawnGO has executable permissions
RUN chmod +x /app/engine/pawnGO

COPY server /app/server
WORKDIR /app/server

EXPOSE 8080
CMD ["npm", "start"]
