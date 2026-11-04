# Stage 1: Build the C++ binary
FROM gcc:12 AS builder

WORKDIR /app
COPY . .

# Compile using C++17 with optimization
RUN g++ -O2 -std=c++17 -o traffic_server main.cpp

# Stage 2: Minimal runtime image
FROM debian:bookworm-slim

WORKDIR /app
COPY --from=builder /app/traffic_server .
COPY --from=builder /app/templates ./templates

ENV PORT=8080
EXPOSE 8080

CMD ["./traffic_server"]