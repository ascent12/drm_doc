#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

/*
 * Get the human-readable string from a DRM connector type.
 * This is compatible with Weston's connector naming.
 */
static const char *conn_str(uint32_t conn_type)
{
	switch (conn_type) {
	case DRM_MODE_CONNECTOR_Unknown:     return "Unknown";
	case DRM_MODE_CONNECTOR_VGA:         return "VGA";
	case DRM_MODE_CONNECTOR_DVII:        return "DVI-I";
	case DRM_MODE_CONNECTOR_DVID:        return "DVI-D";
	case DRM_MODE_CONNECTOR_DVIA:        return "DVI-A";
	case DRM_MODE_CONNECTOR_Composite:   return "Composite";
	case DRM_MODE_CONNECTOR_SVIDEO:      return "SVIDEO";
	case DRM_MODE_CONNECTOR_LVDS:        return "LVDS";
	case DRM_MODE_CONNECTOR_Component:   return "Component";
	case DRM_MODE_CONNECTOR_9PinDIN:     return "DIN";
	case DRM_MODE_CONNECTOR_DisplayPort: return "DP";
	case DRM_MODE_CONNECTOR_HDMIA:       return "HDMI-A";
	case DRM_MODE_CONNECTOR_HDMIB:       return "HDMI-B";
	case DRM_MODE_CONNECTOR_TV:          return "TV";
	case DRM_MODE_CONNECTOR_eDP:         return "eDP";
	case DRM_MODE_CONNECTOR_VIRTUAL:     return "Virtual";
	case DRM_MODE_CONNECTOR_DSI:         return "DSI";
	default:                             return "Unknown";
	}
}

/*
 * Calculate an accurate refresh rate from 'mode'.
 * The result is in mHz.
 */
static int refresh_rate(drmModeModeInfo *mode)
{
	int res = (mode->clock * 1000000LL / mode->htotal + mode->vtotal / 2) / mode->vtotal;

	if (mode->flags & DRM_MODE_FLAG_INTERLACE)
		res *= 2;

	if (mode->flags & DRM_MODE_FLAG_DBLSCAN)
		res /= 2;

	if (mode->vscan > 1)
		res /= mode->vscan;

	return res;
}

/*
 * Prints all of the connectors and the modes they support,
 * somewhat similar to xrandr.
 */
int main(void)
{
	/* We just take the first GPU that exists. */
	int drm_fd = open("/dev/dri/card0", O_RDWR | O_NONBLOCK);
	if (drm_fd < 0) {
		perror("/dev/dri/card0");
		return 1;
	}

	drmModeRes *res = drmModeGetResources(drm_fd);
	if (!res) {
		perror("drmModeGetResources");
		return 1;
	}

	for (int i = 0; i < res->count_connectors; ++i) {
		drmModeConnector *conn = drmModeGetConnector(drm_fd, res->connectors[i]);
		if (!conn) {
			perror("drmModeGetConnector");
			continue;
		}

		printf("%s-%"PRIu32" %s\n",
			conn_str(conn->connector_type),
			conn->connector_type_id,
			conn->connection == DRM_MODE_CONNECTED ? "connected" : "disonnected");

		for (int j = 0; j < conn->count_modes; ++j) {
			drmModeModeInfo *mode = &conn->modes[j];

			printf("  %"PRIi32"x%"PRIi32"%s_%.02f\n",
				mode->hdisplay, mode->vdisplay,
				mode->flags & DRM_MODE_FLAG_INTERLACE ? "i" : "",
				refresh_rate(mode) / 1000.0);
		}

		drmModeFreeConnector(conn);
	}

	drmModeFreeResources(res);
	close(drm_fd);
}
