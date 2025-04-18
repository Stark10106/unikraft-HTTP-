# Build stage
FROM gcc:13.2.0-bookworm AS build

WORKDIR /src

# Copy source code to container
COPY ./http_server.c /src/http_server.c

# Compile the code with optimizations, and strip the binary for size reduction
RUN set -xe; \
    gcc -O3 -Wall -Wextra -fPIC -pie -o /http_server http_server.c && \
    strip /http_server

# Final stage - minimal runtime image
FROM scratch

# Copy the compiled executable and its dependencies
COPY --from=build /http_server /http_server

# Copy only the necessary runtime libraries (via ldd)
COPY --from=build /lib/x86_64-linux-gnu/libc.so.6 /lib/x86_64-linux-gnu/
COPY --from=build /lib64/ld-linux-x86-64.so.2 /lib64/

# Set the entry point for the container
ENTRYPOINT ["/http_server"]
