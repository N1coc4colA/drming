# drming
Small project to be able to stream my screen even at startup onto my tablet, because my laptop's screen's broken.
Being able to have a small portable screen (my tablet) feels like a dream to me.

## Architecture
The project uses VKMS to create a virtual display. It then uses the DRM infrastructure of the kernel to get the image into userland, then stream it over the network.
The project has two main directories:
 - desktop: the daemon to use on your computer that does not have a screen anymore.
 - android: the app to use on your android device, but it can also be used on other platforms too (except iOS & MacOS).

## Dependencies
Dependencies of desktop:
```
QtCore
QtGui
QtNetwork
QtDBus
QtQuick
libdrm
Avahi Client
VKMS
```
Dependencies of android:
```
QtCore
QtGui
QtNetwork
QtQuick
```
Notice that these do not include the classic dependencies required to compile a Qt C++ app for the Android platform.

## mDNS
Avahi is used to perform the mDNS advertising over the network. When using android on other platforms, Avahi is also used to perform the mDNS discovery of services.
If the platform used is Android, then the Android APIs are used for the discovery.

## Tested platforms
Obviously, only GNU/Linux is supported for desktop, and has been tested on Fedora 43 & 44.
Concerning android, it has been tested on Android 14, and Fedora 43 & 44.

## Building
You can build using:
```
cmake
make
```

You can use this from the top dir, in which case it will build both android and desktop for the same platform.
However, you can also go in each dir, and build android and desktop separately if you aim for different platforms & architectures.

## Usage
To use this project, you first need to run drming_desktop, as root to have the rights, then you can connect to it using drming_android.
You can load yourself the vkms driver, or let drming_desktop do it. The usual way to load vkms is the following, with the parameter used
to enable the use of the cursor plane (or you may won't see your mouse cursor):

```
modprobe vkms enable_cursor=1
```
