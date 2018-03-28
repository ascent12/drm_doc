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

DRM is interacted with by using a series of ioctls, usually found in
`/usr/include/libdrm`, but you'd usually interact with it using the libdrm
userspace library, which exists under the Mesa umbrella of projects.

The header files you need to include are `<xf86drm.h>` and `<xf86drmMode.h>`.

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
to get everything working. They are referred to by a `uint32_t` handle.

These objects are
- Connectors
- CRTCs
- Encoders
- Framebuffers

We'll introduce and explore these more as we need them.

In order to find out about all of these resources, we need to call
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

A connector is represents the actual physical connectors on your GPUs.  So if
you have a DVI and HDMI connector on your GPU, they should each have their own
DRM connector object.

One surprising thing about connectors is that the number of them can actually
change between different calls to drmModeGetResources. You might wonder how
you're growing new connectors on your GPU, but with DisplayPort 1.2, there is a
feature called Multi-Stream Transport (referred to as MST), where you can have
multiple monitors going to a single physical connector, either with
daisy-chaining or a hub. When a new one of these monitors is plugged in, they
will appear as a separate DRM connector.

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

`connector_type` tells us whether our connector is HDMI, DVI, DisplayPort, etc.
`connector_type_id` distinguishes between connectors of the same type, so if
you have two HDMI connectors, one will be 1 and the other 2. We can use this
information to give our connectors a meaningful name, such as "HDMI-A-0".
`connector_type` is only an integer, so we'll write a convenience function to
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
should run at. The modes in drmModeConnector are the modes that a monitor
reports that it can run natively. They're ordered from best to worst.

`hdisplay` is the horizontal resolution.  
`vdisplay` is the vertical resolution.  
`vrefresh` is the refresh rate in Hz, but it's quite inaccurate. We'll write
another function to compute a more accurate refresh rate from the rest of the
mode.  
`flags` tells us various extra things about the mode, such as if it uses
interlacing.

---

From here, we have enough information to write a program similar to xrandr
which print all of the monitors and resolutions supported.

That concludes the basic introduction and usage of the DRM API. Next time,
we'll introduce more DRM objects, and configure the display pipeline to do
what we want.
