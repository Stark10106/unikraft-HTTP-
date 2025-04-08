# HTTP Server Unikernel

This is a lightweight HTTP server implemented as a unikernel using Unikraft. It demonstrates:

- Network functionality (listening on a port, handling connections)
- Dynamic content generation
- Performance metrics collection and display
- Request handling

## Setup

To run this example, [install Unikraft's companion command-line toolchain `kraft`](https://unikraft.org/docs/cli), clone this repository and `cd` into this directory.

## Build and Run

First, build the unikernel:

```bash
kraft build --plat qemu --arch x86_64 .
```

Then run it with port forwarding for the HTTP server:

```bash
kraft run --rm -p 8080:8080 --plat qemu --arch x86_64 --initrd .unikraft/build/initramfs-x86_64.cpio .
```

## Testing the HTTP Server

Once the unikernel is running, you can test it using your web browser or curl:

```bash
# Using curl
curl http://localhost:8080/

# Or open in your browser:
# http://localhost:8080/
```

The server displays:
- A welcome message
- Server uptime
- Request counter (refresh to see it increase)

## Performance Testing

./benchmark.sh

## Memory Configuration

If you need more memory for your unikernel, use the `-M` flag:

```bash
kraft run --rm -p 8080:8080 --plat qemu --arch x86_64 -M 256M --initrd .unikraft/build/initramfs-x86_64.cpio .
```

## Extending the Server

This simple HTTP server can be extended in several ways:
- Add support for different URL paths
- Implement file serving capabilities
- Add authentication mechanisms
- Improve request parsing and HTTP protocol compliance

Feel free to modify the code to experiment with these enhancements!