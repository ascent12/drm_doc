# Writing a DRM application
## Part 1 - Introduction

The graphics stack is an interesting and fast-moving part of the Linux
ecosystem.  There are a constant stream of new features and interfaces being
added to improve the performance and experience for Linux users.  It's all
quite a challenge to keep up with, and is especially hard for newcomers to get
their feet wet.  Documentation is spread out over many places and projects, and
there are few tutorials for how to use many of these interfaces; especially the
newer ones.

I was the person who added the support for DRM and related APIs into
[wlroots](https://github.com/swaywm/wlroots), a popular Wayland compositor
library used by [Sway](https://github.com/swaywm/sway), and many other Wayland
compositors.  I had to spend a lot of time researching and experimenting with
the Linux graphics APIs in order to get anything useful working.

There were a few tutorials and blog posts floating around
\([this](https://github.com/dvdhrm/docs/tree/master/drm-howto) was particularly
 helpful\), but they were never complete. They would only cover a very specific
interface, and it was up to you to figure out how to fit them all together.

In order to make it easier for people to get into Linux graphics \(and
understand what on earth I'm doing in wlroots' DRM code\), I thought I would
write a tutorial explaining how you'd write a DRM application, from scratch,
with all of the bells and whistles.

### DRM

DRM has nothing to do with preventing you from watching movies, but instead
stands for 'Direct Rendering Manager'. It's the kernel subsystem for
dealing with graphics cards and computer monitors connected to them.
It's used by Xorg and Wayland compositors for their outputs, but is
certainly not limited to implementing a windowing system. You could use
it to draw whatever you want. For example, [mpv](https://mpv.io) is capable
of displaying videos directly to your monitors using DRM.
