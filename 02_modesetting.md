# Writing a DRM application
## Part 2 - Modesetting

Last time, we enumerated our all of the connectors our GPU has, gave it a nice
little name, and printed its modes. It's all pretty easy to grasp.

However, this is where things really step up in difficulty, and we get into the
real meat and potatoes of DRM. We're going to perform a modeset, which is the
process of configuring the DRM pipeline and displaying a frame to the screen.

We have to learn about 3 new types of DRM objects: CRTCs, Encoders, and
Framebuffers. We can't do anything useful without all of them, so we'll
just have to introduce them at the same time.

## Legacy DRM

For this modeset, we're going to perform it using the legacy DRM interface.

> Didn't you say in the first article that we were using the modern APIs?

Yes, I did. However, the modern API, Atomic Modesetting, is still not supported by
every DRM driver, and in the interest of hardware capability, we're going to need
to do it this way. Atomic Modesetting also requires us to use more parts of the DRM
interface we haven't introduced yet.

We're going to get around to Atomic Modesetting in its own article.

## Framebuffers

This DRM object is pretty easy to understand; it's a buffer to store a frame.

```c
typedef struct _drmModeFB {
	uint32_t fb_id;
	uint32_t width, height;
	uint32_t pitch;
	uint32_t bpp;
	uint32_t depth;
	/* driver specific handle */
	uint32_t handle;
} drmModeFB, *drmModeFBPtr;
```

The fields are pretty straight forward:
- `width` and `height` are obvious.
- `pitch` is the number of bytes between different rows in a frame.
This is often called 'stride', as well.
- `bpp` bits per pixel.
- `depth` is the colour depth.
- `handle`, as the comment says: it's a driver specific handle to the framebuffer.
This is different than the handle used for a DRM object.

I brought up the `drmModeFB` struct, but there actually very little reason to
use it directly. We'll just be using its DRM handle and the driver specific
handle for all we need.

I also glossed over `bpp` and `depth`, and whatever the subtle difference there
is between the two.  As it turns out, it's not important: we'll be using DRM
formats instead, which handle all of these kinds of details for us.

## DRM Formats

DRM uses these to describe how the pixels are laid out in your framebuffer.
They're described using [FourCC](https://en.wikipedia.org/wiki/FourCC) codes,
but you can just find a list of them in `/usr/include/libdrm/drm_fourcc.h`.

There are certainly a lot of them there, but that doesn't mean that your
graphics driver supports them.

In fact, the only ones you can use with any degree of confidence are
`DRM_FORMAT_XRGB8888` and `DRM_FORMAT_ARGB8888`.

`XRGB8888` says:
- There are 4 colour components
- The first is ignored and is 8 bits
- The second is red and is 8 bits
- The third is green and is 8 bits
- The fourth is blue and is 8 bits

Similarly, `ARGB8888` says:
- There are 4 colour components
- The first is alpha and is 8 bits
- The second is red and is 8 bits
- The third is green and is 8 bits
- The fourth is blue and is 8 bits

It's helpful to be able to read these descriptions, so we'll do one more
just to drive the point home:

`BGRA1010102` says:
- There are 4 colour components
- The first is blue and is 10 bits
- The second is green and is 10 bits
- The third is red and is 10 bits
- The fourth is alpha and is 2 bits

You'll also notice some oddball ones like `DRM_FORMAT_NV12` and
`DRM_FORMAT_YUYV`. These are multi-planar formats, and don't use the RGB
colour space, and will be covered separately later

It's also important to note that these formats are little endian.  So if we're
using `ARGB8888` and wanted write the colour
[#112233](http://www.color-hex.com/color/112233)[100], in memory, it would look
like
```c
uint8_t pixel[4] = { 0xff, 0x33, 0x22, 0x11 };
```

## CRTCs

After connectors, these are the next most important part of the DRM system.

It stands for **Cathode Ray Tube Controller**.

For those of you who don't remember, CRTs were the display technology used for
old, bulky TVs and Computer monitors, before LCDs completely took over.
However, CRTCs have absolutely nothing to do with Cathode Ray Tubes. The name
is completely meaningless, and is only called that for hysterical raisins.

CRTCs serve as the point for setting framebuffers and controlling our
connectors. They are quite nebulous objects, but most of our interactions with
DRM will end up happening through them.

```c
typedef struct _drmModeCrtc {
	uint32_t crtc_id;
	uint32_t buffer_id; /**< FB id to connect to 0 = disconnect */

	uint32_t x, y; /**< Position on the framebuffer */
	uint32_t width, height;
	int mode_valid;
	drmModeModeInfo mode;

	int gamma_size; /**< Number of gamma stops */

} drmModeCrtc, *drmModeCrtcPtr;
```

- `buffer_id` is the DRM object handle of the framebuffer being used.
- `mode` is the mode currently used on the connector this CRTC is controlling.
- `x`, `y`, `width`, and `height` all show what part of the framebuffer is
actually being shown, meaning we can use a framebuffer larger than mode we're
using.

Despite the fact that the CRTC is controlling a connector, you'll notice that
there is no reference to one here. In fact, it's possible for a CRTC to control
multiple connectors, giving a cloned display, but they must all be running at
the same mode.

## Encoders

Encoders are responsible for taking the pixels from a CRTC, and encoding it into
a format that a particular connector can use.

```c
typedef struct _drmModeEncoder {
	uint32_t encoder_id;
	uint32_t encoder_type;
	uint32_t crtc_id;
	uint32_t possible_crtcs;
	uint32_t possible_clones;
} drmModeEncoder, *drmModeEncoderPtr;
```

You can get the encoders that a connector can use from the `encoders` member of
`drmModeConnector`.

We can ignore most of the members, but the one important one for our purpose if
`possible_crtcs`.  This value is a bitmask, and represent which CRTCs found in
`drmModeRes` it is compatible with.

So if bit 0 of `possible_crtcs` is set, it means that this encoder is
compatible with `resources->crtcs[0]`; if bit 1 is set, it's compatible with
`resources->crtcs[1]`; and so on.

## The Pipeline

![DRM Pipeline](./02_pipeline.svg)
