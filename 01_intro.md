# Writing a DRM application
## Part 1 - Introduction

DRM isn't just a way to lock down your users' freedoms when it comes to
software and media, and generally make the world a worse place, but is also a
fun way to draw things to your computer monitors without the need for a
windowing system on your Linux systems.

So just to be clear, I'm talking about the **Direct Rendering Manager** system
of the Linux Kernel, not **Digital Rights Management**.

The entire Linux graphics stack is an exciting and fast-moving area, with new
technologies and APIs constantly being added to make everything more efficient
and give a better experience for the user. With this rapid growth, its sad
to say that a lot of the documentation hasn't kept up, especially for people
who are new to the system. You'll find bits and pieces of documentation around,
but they're far and few between, and most of them only cover the old APIs or
are geared to people writing DRM kernel drivers.

So I thought I would write a guide to write a DRM userspace program, using all
of the modern APIs and features.

I'll expect you to have at least an intermediate understanding of C and
programming for Unix-like systems. I'm not going to explain how you should
build your code or how to find and use a library.

Obviously this doesn't have to be done in C, but C is the language of open
source, and we'll be using many C libraries.

## About Me

I'm one of the developers of [wlroots](https://github.com/swaywm/wlroots), a
popular library for writing Wayland compositors, used by
[sway](https://github.com/swaywm/sway) and many others. I'm responsible
for the DRM backend, and most of the code related to interacting with the
Linux graphics stack.

I'd been interested in Wayland, and possibly writing my own compositor for
quite a long time. I investigated the technologies involved, and I arrived
at the natural starting point of trying to find out to draw something to the
monitor. I'm not one to skimp on quality, so I wanted to do everything
*right*. I wanted my compositor to work at least as well as X11 does in
that department. Sure, X11 can screen-tear a lot, but the rest of its DRM
stuff is quite good.

Now, it turned out this is a monumental task. I was a university student with
no graphics experience,  and it took a good two years of casual research and
experimenting with DRM and the associated technologies to get to the point of
being remotely passable.

This whole thing is probably one of the largest roadblocks when it comes to
writing a Wayland compositor. Smaller projects and one-man teams just don't
have the resources to research and learn all of these complicated systems. I'd
spent all of this time learning **only** the graphics system; there are so many
other technologies involved in writing a Wayland compositor. After realising
the sheer scale of what I was trying to build, I ended up losing a lot of
motivation towards doing it.

Right around this time, SirCmpwn, the author of sway, expressed his intent to
write his own library for writing Wayland Compositors called wlroots, to
replace [wlc](https://github.com/Cloudef/wlc/), which sway was currently using.
I'd been interested in Sway (as I'm an i3 user), but it had always been a bit
too shoddy for my standards, especially around the display department.  I'm not
actually taking a stab at sway, but instead wlc; it didn't support display
rotation, and I'm not going to stop using my secondary monitor in the portrait
orientation. It also just did so much related to graphics wrong, and seemed
very fragile.

Now that there was an opportunity to contribute to something fresh, and I had
spent so much time researching Linux graphics, so I went ahead and contributed
what I knew to wlroots (and made sure to support display rotation), and here we
are today.

Now what was the point of me telling you all this? It's to say:  
**You probably don't want to write your own Wayland Compositor from scratch.**

I really would recommend using a library, especially if you're in a small team
or a lone developer. Only the big players like Gnome or KDE have the resources
to write this sort of stuff themselves, and using a library will seriously save
you years of development time.

I'd reccomend wlroots, but there may be some bias in that.

## Moving on

Now that I've told you why you shouldn't write your own DRM code, we'll move
on to doing it anyway.

Not everybody's writing a Wayland Compositor. DRM can be used to draw whatever
you want. For example, [mpv](https://mpv.io/) is capable of playing videos
directly to your monitor using DRM.

Maybe you want to write their own Wayland Compositor library to rival wlroots?

Also, other wlroots developers have said that most of my code touches a lot of
arcane stuff, and they don't truly understand what's going on, and why things
are designed the way they are. It'd be nice if people knew more about these
systems, even if they're not going to be writing the code themselves.

We'll start by writing a simple program which can detect all of the outputs
plugged in, and what resolutions they support; kind of like xrandr.

### DRM

DRM is interacted with by using a series of ioctls, usually found in
`/usr/include/libdrm`. Dealing with these ioctls is a pain in the ass, so Mesa
provides a library that wraps over them called libdrm, which we'll be using.

The path I gave before is actually the location of the kernel headers, which is
not libdrm. The actual libdrm headers are `<xf86drm.h>` and `<xf86drmMode.h>`.
It's like that just to be more annoying.

The pkg-config name is just libdrm.

### Opening your GPU

Just like most things in a Unix-like operating systems, graphics cards are
manipulated with files.  They're found in `/dev/dri/cardN`, where `N` is a
number, counting up from 0.  We'll keep things simple and assume there's a
single GPU, so we just need to do
```c
int drm_fd = open("/dev/dri/card0", O_RDRW | O_NONBLOCK);
```
`O_NONBLOCK` isn't required, but it'll be helpful later.

We won't be read\(2\)ing or write\(2\)ing to this file directly, but it'll be
used extensively over the DRM API.

### DRM Resources

There are many type of kernel-side objects that we need to know and configure
to get everything working. They are each referred to by a `uint32_t` handle.

We'll introduce and explore these objects as we need them.

In order to find out about the objects associated with our GPU, we need to call
```c
drmModeRes *resources = drmModeGetResources(drm_fd);
```
Looking at the definition of `drmModeRes`:
```c
typedef struct _drmModeRes {

	int count_fbs;
	uint32_t *fbs;

	int count_crtcs;
	uint32_t *crtcs;

	int count_connectors;
	uint32_t *connectors;

	int count_encoders;
	uint32_t *encoders;

	uint32_t min_width, max_width;
	uint32_t min_height, max_height;
} drmModeRes, *drmModeResPtr;
```
This simply contains several arrays of handles to DRM objects.

### Connectors

Our first, and probably most simple to understand DRM object is a connector.

A connector is represents the actual physical connectors on your GPUs.  So if
you have a DVI and HDMI connector on your GPU, they should each have their own
DRM connector object.

One surprising thing about connectors is that the number of them can actually
change between different calls to drmModeGetResources. You might wonder how
you're growing new connectors on your GPU, but with DisplayPort 1.2, there is a
feature called Multi-Stream Transport (referred to as MST), where you can have
multiple monitors going to a single physical connector, either with
daisy-chaining or a hub. When a new one of these monitors is plugged in, they
will appear as a separate DRM connector. So be careful to to make any
assumptions in your code about the number of connectors being fixed.

We'll start by looping through all of the connectors and printing some
information about them:

```c
for (int i = 0; i < resources->count_connectors; ++i) {
	drmModeConnector *conn = drmModeGetConnector(drm_fd, resources->connectors[i]);
	if (!conn)
		continue;

	drmModeFreeConnector(conn);
}
```

```c
typedef struct _drmModeConnector {
	uint32_t connector_id;
	uint32_t encoder_id; /**< Encoder currently connected to */
	uint32_t connector_type;
	uint32_t connector_type_id;
	drmModeConnection connection;
	uint32_t mmWidth, mmHeight; /**< HxW in millimeters */
	drmModeSubPixel subpixel;

	int count_modes;
	drmModeModeInfoPtr modes;

	int count_props;
	uint32_t *props; /**< List of property ids */
	uint64_t *prop_values; /**< List of property values */

	int count_encoders;
	uint32_t *encoders; /**< List of encoder ids */
} drmModeConnector, *drmModeConnectorPtr;
```

- `connector_type` tells us whether our connector is HDMI-A, DVI, DisplayPort,
etc.
- `connector_type_id` distinguishes between connectors of the same type, so if
you have two HDMI connectors, one will be 1 and the other 2. We can use this
information to give our connectors a meaningful name, such as "HDMI-A-0".
- `connector_type` is only an integer, so we'll write a convenience function to
get the corresponding string.  
`connection` tells us if there is a monitor plugged in.

### Modes

```c
typedef struct _drmModeModeInfo {
	uint32_t clock;
	uint16_t hdisplay, hsync_start, hsync_end, htotal, hskew;
	uint16_t vdisplay, vsync_start, vsync_end, vtotal, vscan;

	uint32_t vrefresh;

	uint32_t flags;
	uint32_t type;
	char name[DRM_DISPLAY_MODE_LEN];
} drmModeModeInfo, *drmModeModeInfoPtr;
```

A mode is a description of the resolution and refresh rate that a monitor
should run at. The modes in `drmModeConnector` are the modes that a monitor
reports that it can run natively. They're ordered from best to worst.

- `hdisplay` is the horizontal resolution.
- `vdisplay` is the vertical resolution.
- `vrefresh` is the refresh rate in Hz, but it's quite inaccurate. We'll write
another function to compute a more accurate refresh rate from the rest of the
mode.  
- `flags` tells us various extra things about the mode, such as if it uses
interlacing.

---

From here, we have enough information to print all of the information about the
modes.

That concludes the basic introduction and usage of the DRM API. Next time,
we'll introduce more DRM objects, and configure the display pipeline to do
what we want.
