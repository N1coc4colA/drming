# drming

drming streams a Linux computer's display to another device, such as an
Android tablet. It is intended for using a tablet as a small portable display
when the computer's physical screen is unavailable.

## Features

- Creates a virtual display with the Linux kernel's **VKMS** driver.
- Captures the display through the Linux DRM infrastructure.
- Streams video and audio over the network.
- Uses DTLS and client certificates to authenticate and encrypt connections.
- Advertises the server with mDNS through Avahi.
- Discovers servers automatically on Android; Linux clients use Avahi for
  discovery.
- Supports H.265, JPEG, PNG, and WebP stream formats (the available formats
  depend on the selected build and decoder support).
- Provides a Qt/QML client for Android and Linux.
- Lets the client manage server certificates and client certificate/key pairs.

## Architecture

The repository contains two applications:

- `server`: the headless Linux daemon. It creates and configures the VKMS
  display, captures frames, encodes the stream, and publishes the service.
- `client`: the Qt/QML application for Android or Linux. It discovers services,
  authenticates to a server, receives the streams, and displays the video.

The `submodules` directory contains `ctrack`, `ffmpeg-kit-next`, and `qt-aes`.
The CMake build uses `ctrack` and `qt-aes` directly. The FFmpeg-related
submodule is included for the project's media integration and should remain
initialized when setting up a complete checkout.

## Prerequisites

### All builds

- Git
- CMake 3.16 or newer
- A C++23 compiler
- Qt 6.10 or newer with Core, Gui, Network, Quick, and Quick Controls 2; add
  the DBus module when building the server

### Linux server

Install the development packages for:

- `libdrm`
- Avahi client (`libavahi-client`)
- FFmpeg libraries: `libavformat`, `libavcodec`, `libavutil`, and `libswscale`
- `pkg-config`

On Debian/Ubuntu, the package names include:

```sh
sudo apt install \
  libavahi-client-dev libdrm-dev \
  libavcodec-dev libavformat-dev libavutil-dev libswscale-dev \
  pkg-config
```

The server also needs a kernel with `CONFIG_DRM_VKMS` enabled, a mounted
`configfs`, and a running Avahi daemon.

### Linux client

The Linux client additionally needs the FFmpeg development libraries listed
above and Avahi client development files. A desktop Qt installation is
required.

### Android client

Use a Qt installation with Android support, the Android SDK/NDK, and the
standard Qt Android build dependencies. The Android build also expects the
Qt Android OpenSSL 3 libraries. By default CMake looks for them under:

```text
$HOME/Android/Sdk/android_openssl/ssl_3/
```

Set `-DANDROID_OPENSSL_DIR=/path/to/android_openssl/ssl_3` when they are
installed elsewhere. Android API level 28 is the minimum configured by the
project; the target SDK is 36.

## Getting the source

Clone the repository and initialize all submodules before configuring CMake:

```sh
git clone https://github.com/N1coc4colA/drming.git
cd drming
git submodule update --init --recursive
```

If the repository was cloned with `--recurse-submodules`, the update command
is still safe and verifies that the checked-out submodule revisions are
available.

## Building

### Build the Linux server and client

Building each target separately is recommended because the server is Linux
only, while the client may also be configured for Android:

```sh
cmake -S server -B build/server -DCMAKE_BUILD_TYPE=Release
cmake --build build/server --parallel

cmake -S client -B build/client \
  -DCMAKE_BUILD_TYPE=Release \
  -DANDROID_MULTI_MODE=OFF
cmake --build build/client --parallel
```

The executables are produced in the corresponding build directory. Use
`cmake --build <build-dir> --target help` to list available targets.

To configure both targets from the repository root for one compatible desktop
platform:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### Build the Android client

Configure the `client` directory with the Android toolchain supplied by Qt
Creator or by CMake. A typical command is:

```sh
cmake -S client -B build/android \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-28 \
  -DANDROID_OPENSSL_DIR="$HOME/Android/Sdk/android_openssl/ssl_3"
cmake --build build/android --parallel
```

For a multi-ABI package, leave `ANDROID_MULTI_MODE` enabled (the default) and
use the Qt Android deployment flow. Do not add the Linux-only server to an
Android build.

### Tests

Tests are enabled with `BUILD_TESTING=ON`. For example:

```sh
cmake -S server -B build/server \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build/server --parallel
ctest --test-dir build/server --output-on-failure

cmake -S client -B build/client \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DANDROID_MULTI_MODE=OFF
cmake --build build/client --parallel
ctest --test-dir build/client --output-on-failure
```

## Certificates and trust

The server requires a private key, a server certificate, and a directory of
trusted client certificates. The defaults are relative to the working
directory:

```text
./certs/server.key
./certs/server.crt
./certs/valids/*
```

Create or install certificates using your organization's PKI, then start the
server with the matching paths. The client must have a client certificate/key
pair and the server certificate loaded. The client UI has separate
**Certificates** and **Keys** pages for importing and managing these files.

Do not copy private keys into the repository or make them world-readable.
Certificate identity and trust decisions are part of the security boundary:
only place client certificates in the server's trusted directory when those
clients should be allowed to connect.

## Running

Start the server first, then open the client and select the discovered service.
The server command supports detailed help:

```sh
./build/server/drming_server --help
```

A typical server invocation is:

```sh
sudo ./build/server/drming_server \
  --key /etc/pki/drming/server.key \
  --cert /etc/pki/drming/server.crt \
  --trusted '/etc/pki/drming/valids/*' \
  --port 1234 \
  --audio-port 1235 \
  --video-port 1236
```

The audio and video addresses must be multicast addresses (the defaults are
`239.255.1.1` and `239.255.1.2`). Ensure that the network and firewall permit
the control, audio, and video traffic. If multicast is unavailable on the
network, the current server validation will reject alternate unicast audio or
video addresses.

By default the server advertises itself through mDNS. Pass
`--no-advertise` to disable advertising. Use `--iface <interface>` when the
machine has multiple network interfaces.

### VKMS setup

The server normally attempts to load VKMS itself. It must run with sufficient
privileges to load the module and create the configfs display instance. You
can load the module manually:

```sh
sudo modprobe vkms enable_cursor=1
```

`enable_cursor=1` is important when the mouse cursor should be visible in the
captured display. If startup reports that `configfs` is not mounted, mount it
before starting the server:

```sh
sudo mount -t configfs none /sys/kernel/config
```

An example systemd unit is provided in `drming_desktop.service`. Adapt its
paths, user/group, executable location, and port values before installing it.

## Warnings and limitations

- The server is Linux-only and requires VKMS support in the running kernel.
- Running the server as root is usually necessary for VKMS setup. Treat the
  service and its certificate files as privileged system components.
- Network discovery and streaming are intended for a trusted network. Use
  certificates, firewall rules, and an appropriate trusted-client directory;
  do not expose the service to an untrusted network without reviewing the
  threat model.
- The default ports are `80` for control, audio, and video. Override them with
  distinct, permitted ports in real deployments to avoid bind conflicts.
- Android and Linux decoding capabilities differ. H.265 playback depends on
  the platform decoder; select another format if the target device cannot
  decode it.
- Depending on the stream format used, frame rate, size, and compression level,
  it is possible that the server sends too much data to the clients. In such
  cases, it will only saturate the client's network stack, and the stream will
  not be properly processed. Tweaking the parameters can be required to
  properly fit the clients, especially when you are not using H265.
- iOS and macOS are not supported.
- The project has been tested on Fedora 43/44 and Android 14. Other
  distributions, kernels, Qt versions, and devices may require adjustments.

## License and third-party code

This repository includes third-party projects as Git submodules. See each
submodule's own license and documentation for its terms and build details.
