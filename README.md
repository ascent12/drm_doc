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
If you want a more featurful example, then please see
[daniels/kms-quads](https://gitlab.freedesktop.org/daniels/kms-quads). As in
addition to implementing what is implemented here, it also implements:
- GPU accelaration
- Atomic updates (only updates one portion of a screen rathor then every screen
  which can save battery life)
- Real hotplugging support via libudev
- Support for unprivaleged usage
- Keyboard input via libinput

The only disadvantage from  using this repository to learn DRM is that it is
nowhere nearly as well documented, with only a few comments.

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
