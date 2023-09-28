 [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

This is an ongoing series of articles describing how to use the Linux DRM API.

Complete code examples are provided, but you're encouraged to try write along
yourself, and experiment with the various parts. However, you may want to save
all of your work before doing so, as it's possible to get your computer into an
unusable state (where you cannot do anything and have to reboot), especially
when we're dealing with the TTY.

There are probably still a lot of minor spelling and grammatical mistakes. Any
feedback on the articles or fixes are appreciated.

**Sidenote:**
If you want a more featurful example, then there are 2 main sources of truth as,
in addition to implementing what is implemented here, they also implement:
- GPU accelaration
- Atomic updates (only updates one portion of a screen rathor then every screen
  which can save battery life)
- Real hotplugging support via libudev
- Support for unprivaleged usage
- Keyboard input via libinput
They are:
- [CuarzoSoftware/SRM](https://github.com/CuarzoSoftware/SRM) - A library with
  good docs and several examples as well as a few more features (like hardware
  cursors), it is also well maintained.
- [daniels/kms-quads](https://gitlab.freedesktop.org/daniels/kms-quads) - A full
  example repository, this is not updated, nowhere nearly as well documented
  (only a few comments) and you must understand the code as it is a full example
  that you must fork rathor then a library.

---

Dependencies:
- libdrm

How to run examples (from within subdirectories):
```sh
meson build
cd build
ninja
./prog_name
```

Most of the examples will only work when run in their own virtual terminal, and
will fail inside Xorg or a Wayland Compositor.
