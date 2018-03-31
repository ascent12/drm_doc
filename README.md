 [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

This is an ongoing series of articles describing how to use the Linux DRM API.

Complete code examples are provided, but you're encouraged to try write along
yourself, and experiment with the various parts. However, you may want to save
all of your work before doing so, as it's possible to get your computer into an
unusable state, especially when we're dealing with the TTY.

There are probably still a lot of minor spelling and grammatical mistakes. Any
feedback on the articles or fixes are appreciated.

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
