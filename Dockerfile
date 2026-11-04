
FROM gcc:12 AS builder

WORKDIR /app

COPY main.cpp .
COPY templates ./templates

RUN g++ -O2 -std=c++17 -Wall -Wextra \
    -o traffic_server main.cpp

FROM debian:bookworm-slim

WORKDIR /app

COPY --from=builder /app/traffic_server .
COPY --from=builder /app/templates ./templates

ENV PORT=8080

EXPOSE 8080

CMD ["./traffic_server"]
