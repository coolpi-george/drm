/*
 * Copyright (c) 2006-2007 Intel Corporation
 * Copyright (c) 2007 Dave Airlie <airlied@linux.ie>
 *
 * DRM core CRTC related functions
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 *
 * Authors:
 *      Keith Packard
 *	Eric Anholt <eric@anholt.net>
 *      Dave Airlie <airlied@linux.ie>
 *      Jesse Barnes <jesse.barnes@intel.com>
 */
#include <linux/list.h>
#include "drm.h"
#include "drmP.h"
#include "drm_crtc.h"

struct drm_prop_enum_list {
	int type;
	char *name;
};

/*
 * Global properties
 */
static struct drm_prop_enum_list drm_dpms_enum_list[] =
{ { DPMSModeOn, "On" },
  { DPMSModeStandby, "Standby" },
  { DPMSModeSuspend, "Suspend" },
  { DPMSModeOff, "Off" }
};

char *drm_get_dpms_name(int val)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(drm_dpms_enum_list); i++)
		if (drm_dpms_enum_list[i].type == val)
			return drm_dpms_enum_list[i].name;

	return "unknown";
}

static struct drm_prop_enum_list drm_conn_enum_list[] = 
{ { ConnectorUnknown, "Unknown" },
  { ConnectorVGA, "VGA" },
  { ConnectorDVII, "DVI-I" },
  { ConnectorDVID, "DVI-D" },
  { ConnectorDVIA, "DVI-A" },
  { ConnectorComposite, "Composite" },
  { ConnectorSVIDEO, "SVIDEO" },
  { ConnectorLVDS, "LVDS" },
  { ConnectorComponent, "Component" },
  { Connector9PinDIN, "9-pin DIN" },
  { ConnectorDisplayPort, "DisplayPort" },
  { ConnectorHDMIA, "HDMI Type A" },
  { ConnectorHDMIB, "HDMI Type B" },
};
static struct drm_prop_enum_list drm_output_enum_list[] =
{ { DRM_MODE_OUTPUT_NONE, "None" },
  { DRM_MODE_OUTPUT_DAC, "DAC" },
  { DRM_MODE_OUTPUT_TMDS, "TMDS" },
  { DRM_MODE_OUTPUT_LVDS, "LVDS" },
  { DRM_MODE_OUTPUT_TVDAC, "TV" },
};

char *drm_get_output_name(struct drm_output *output)
{
	static char buf[32];

	snprintf(buf, 32, "%s-%d", drm_output_enum_list[output->output_type].name,
		 output->output_type_id);
	return buf;
}

char *drm_get_output_status_name(enum drm_output_status status)
{
	if (status == output_status_connected)
		return "connected";
	else if (status == output_status_disconnected)
		return "disconnected";
	else
		return "unknown";
}

/**
 * drm_idr_get - allocate a new identifier
 * @dev: DRM device
 * @ptr: object pointer, used to generate unique ID
 *
 * LOCKING:
 * Caller must hold DRM mode_config lock.
 *
 * Create a unique identifier based on @ptr in @dev's identifier space.  Used
 * for tracking modes, CRTCs and outputs.
 *
 * RETURNS:
 * New unique (relative to other objects in @dev) integer identifier for the
 * object.
 */
int drm_idr_get(struct drm_device *dev, void *ptr)
{
	int new_id = 0;
	int ret;
again:
	if (idr_pre_get(&dev->mode_config.crtc_idr, GFP_KERNEL) == 0) {
		DRM_ERROR("Ran out memory getting a mode number\n");
		return 0;
	}

	ret = idr_get_new_above(&dev->mode_config.crtc_idr, ptr, 1, &new_id);
	if (ret == -EAGAIN)
		goto again;	

	return new_id;
}

/**
 * drm_idr_put - free an identifer
 * @dev: DRM device
 * @id: ID to free
 *
 * LOCKING:
 * Caller must hold DRM mode_config lock.
 *
 * Free @id from @dev's unique identifier pool.
 */
void drm_idr_put(struct drm_device *dev, int id)
{
	idr_remove(&dev->mode_config.crtc_idr, id);
}

/**
 * drm_crtc_from_fb - find the CRTC structure associated with an fb
 * @dev: DRM device
 * @fb: framebuffer in question
 *
 * LOCKING:
 * Caller must hold mode_config lock.
 *
 * Find CRTC in the mode_config structure that matches @fb.
 *
 * RETURNS:
 * Pointer to the CRTC or NULL if it wasn't found.
 */
struct drm_crtc *drm_crtc_from_fb(struct drm_device *dev,
				  struct drm_framebuffer *fb)
{
	struct drm_crtc *crtc;

	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		if (crtc->fb == fb)
			return crtc;
	}
	return NULL;
}

/**
 * drm_framebuffer_create - create a new framebuffer object
 * @dev: DRM device
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * Creates a new framebuffer objects and adds it to @dev's DRM mode_config.
 *
 * RETURNS:
 * Pointer to new framebuffer or NULL on error.
 */
struct drm_framebuffer *drm_framebuffer_create(struct drm_device *dev)
{
	struct drm_framebuffer *fb;

	fb = kzalloc(sizeof(struct drm_framebuffer), GFP_KERNEL);
	if (!fb)
		return NULL;
	
	fb->id = drm_idr_get(dev, fb);
	fb->dev = dev;
	dev->mode_config.num_fb++;
	list_add(&fb->head, &dev->mode_config.fb_list);

	return fb;
}
EXPORT_SYMBOL(drm_framebuffer_create);

/**
 * drm_framebuffer_destroy - remove a framebuffer object
 * @fb: framebuffer to remove
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * Scans all the CRTCs in @dev's mode_config.  If they're using @fb, removes
 * it, setting it to NULL.
 */
void drm_framebuffer_destroy(struct drm_framebuffer *fb)
{
	struct drm_device *dev = fb->dev;
	struct drm_crtc *crtc;

	/* remove from any CRTC */
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		if (crtc->fb == fb)
			crtc->fb = NULL;
	}

	drm_idr_put(dev, fb->id);
	list_del(&fb->head);
	dev->mode_config.num_fb--;

	kfree(fb);
}
EXPORT_SYMBOL(drm_framebuffer_destroy);

/**
 * drm_crtc_create - create a new CRTC object
 * @dev: DRM device
 * @funcs: callbacks for the new CRTC
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * Creates a new CRTC object and adds it to @dev's mode_config structure.
 *
 * RETURNS:
 * Pointer to new CRTC object or NULL on error.
 */
struct drm_crtc *drm_crtc_create(struct drm_device *dev,
				 const struct drm_crtc_funcs *funcs)
{
	struct drm_crtc *crtc;

	crtc = kzalloc(sizeof(struct drm_crtc), GFP_KERNEL);
	if (!crtc)
		return NULL;

	crtc->dev = dev;
	crtc->funcs = funcs;

	crtc->id = drm_idr_get(dev, crtc);

	list_add_tail(&crtc->head, &dev->mode_config.crtc_list);
	dev->mode_config.num_crtc++;

	return crtc;
}
EXPORT_SYMBOL(drm_crtc_create);

/**
 * drm_crtc_destroy - remove a CRTC object
 * @crtc: CRTC to remove
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * Cleanup @crtc.  Calls @crtc's cleanup function, then removes @crtc from
 * its associated DRM device's mode_config.  Frees it afterwards.
 */
void drm_crtc_destroy(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;

	if (crtc->funcs->cleanup)
		(*crtc->funcs->cleanup)(crtc);

	drm_idr_put(dev, crtc->id);
	list_del(&crtc->head);
	dev->mode_config.num_crtc--;
	kfree(crtc);
}
EXPORT_SYMBOL(drm_crtc_destroy);

/**
 * drm_crtc_in_use - check if a given CRTC is in a mode_config
 * @crtc: CRTC to check
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * Walk @crtc's DRM device's mode_config and see if it's in use.
 *
 * RETURNS:
 * True if @crtc is part of the mode_config, false otherwise.
 */
bool drm_crtc_in_use(struct drm_crtc *crtc)
{
	struct drm_output *output;
	struct drm_device *dev = crtc->dev;
	/* FIXME: Locking around list access? */
	list_for_each_entry(output, &dev->mode_config.output_list, head)
		if (output->crtc == crtc)
			return true;
	return false;
}
EXPORT_SYMBOL(drm_crtc_in_use);

/*
 * Detailed mode info for a standard 640x480@60Hz monitor
 */
static struct drm_display_mode std_mode[] = {
	{ DRM_MODE("640x480", DRM_MODE_TYPE_DEFAULT, 25200, 640, 656,
		   752, 800, 0, 480, 490, 492, 525, 0,
		   V_NHSYNC | V_NVSYNC) }, /* 640x480@60Hz */
};

/**
 * drm_crtc_probe_output_modes - get complete set of display modes
 * @dev: DRM device
 * @maxX: max width for modes
 * @maxY: max height for modes
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * Based on @dev's mode_config layout, scan all the outputs and try to detect
 * modes on them.  Modes will first be added to the output's probed_modes
 * list, then culled (based on validity and the @maxX, @maxY parameters) and
 * put into the normal modes list.
 *
 * Intended to be used either at bootup time or when major configuration
 * changes have occurred.
 *
 * FIXME: take into account monitor limits
 */
void drm_crtc_probe_single_output_modes(struct drm_output *output, int maxX, int maxY)
{
	struct drm_device *dev = output->dev;
	struct drm_display_mode *mode, *t;
	int ret;
	//if (maxX == 0 || maxY == 0) 
	// TODO

	/* set all modes to the unverified state */
	list_for_each_entry_safe(mode, t, &output->modes, head)
		mode->status = MODE_UNVERIFIED;
		
	output->status = (*output->funcs->detect)(output);
	
	if (output->status == output_status_disconnected) {
		DRM_DEBUG("%s is disconnected\n", drm_get_output_name(output));
		/* TODO set EDID to NULL */
		return;
	}
	
	ret = (*output->funcs->get_modes)(output);
	
	if (ret) {
		drm_mode_output_list_update(output);
	}
	
	if (maxX && maxY)
		drm_mode_validate_size(dev, &output->modes, maxX,
				       maxY, 0);
	list_for_each_entry_safe(mode, t, &output->modes, head) {
		if (mode->status == MODE_OK)
			mode->status = (*output->funcs->mode_valid)(output,mode);
	}
	
	
	drm_mode_prune_invalid(dev, &output->modes, TRUE);
	
	if (list_empty(&output->modes)) {
		struct drm_display_mode *stdmode;
		
		DRM_DEBUG("No valid modes on %s\n", drm_get_output_name(output));
		
		/* Should we do this here ???
		 * When no valid EDID modes are available we end up
		 * here and bailed in the past, now we add a standard
		 * 640x480@60Hz mode and carry on.
		 */
		stdmode = drm_mode_duplicate(dev, &std_mode[0]);
		drm_mode_probed_add(output, stdmode);
		drm_mode_list_concat(&output->probed_modes,
				     &output->modes);
		
		DRM_DEBUG("Adding standard 640x480 @ 60Hz to %s\n",
			  drm_get_output_name(output));
	}
	
	drm_mode_sort(&output->modes);
	
	DRM_DEBUG("Probed modes for %s\n", drm_get_output_name(output));
	list_for_each_entry_safe(mode, t, &output->modes, head) {
		mode->vrefresh = drm_mode_vrefresh(mode);
		
		drm_mode_set_crtcinfo(mode, CRTC_INTERLACE_HALVE_V);
		drm_mode_debug_printmodeline(dev, mode);
	}
}

void drm_crtc_probe_output_modes(struct drm_device *dev, int maxX, int maxY)
{
	struct drm_output *output;

	list_for_each_entry(output, &dev->mode_config.output_list, head) {
		drm_crtc_probe_single_output_modes(output, maxX, maxY);
	}
}
EXPORT_SYMBOL(drm_crtc_probe_output_modes);

/**
 * drm_crtc_set_mode - set a mode
 * @crtc: CRTC to program
 * @mode: mode to use
 * @x: width of mode
 * @y: height of mode
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * Try to set @mode on @crtc.  Give @crtc and its associated outputs a chance
 * to fixup or reject the mode prior to trying to set it.
 *
 * RETURNS:
 * True if the mode was set successfully, or false otherwise.
 */
bool drm_crtc_set_mode(struct drm_crtc *crtc, struct drm_display_mode *mode,
		       int x, int y)
{
	struct drm_device *dev = crtc->dev;
	struct drm_display_mode *adjusted_mode, saved_mode;
	int saved_x, saved_y;
	struct drm_output *output;
	bool ret = true;

	adjusted_mode = drm_mode_duplicate(dev, mode);

	crtc->enabled = drm_crtc_in_use(crtc);

	if (!crtc->enabled)
		return true;

	saved_mode = crtc->mode;
	saved_x = crtc->x;
	saved_y = crtc->y;
	
	/* Update crtc values up front so the driver can rely on them for mode
	 * setting.
	 */
	crtc->mode = *mode;
	crtc->x = x;
	crtc->y = y;

	if (drm_mode_equal(&saved_mode, &crtc->mode)) {
		if (saved_x != crtc->x || saved_y != crtc->y) {
			crtc->funcs->mode_set_base(crtc, crtc->x, crtc->y);
			goto done;
		}
	}

	/* Pass our mode to the outputs and the CRTC to give them a chance to
	 * adjust it according to limitations or output properties, and also
	 * a chance to reject the mode entirely.
	 */
	list_for_each_entry(output, &dev->mode_config.output_list, head) {
		
		if (output->crtc != crtc)
			continue;
		
		if (!(ret = output->funcs->mode_fixup(output, mode, adjusted_mode))) {
			goto done;
		}
	}
	
	if (!(ret = crtc->funcs->mode_fixup(crtc, mode, adjusted_mode))) {
		goto done;
	}

	/* Prepare the outputs and CRTCs before setting the mode. */
	list_for_each_entry(output, &dev->mode_config.output_list, head) {

		if (output->crtc != crtc)
			continue;
		
		/* Disable the output as the first thing we do. */
		output->funcs->prepare(output);
	}
	
	crtc->funcs->prepare(crtc);
	
	/* Set up the DPLL and any output state that needs to adjust or depend
	 * on the DPLL.
	 */
	crtc->funcs->mode_set(crtc, mode, adjusted_mode, x, y);

	list_for_each_entry(output, &dev->mode_config.output_list, head) {

		if (output->crtc != crtc)
			continue;
		
		DRM_INFO("%s: set mode %s %x\n", drm_get_output_name(output), mode->name, mode->mode_id);

		output->funcs->mode_set(output, mode, adjusted_mode);
	}
	
	/* Now, enable the clocks, plane, pipe, and outputs that we set up. */
	crtc->funcs->commit(crtc);

	list_for_each_entry(output, &dev->mode_config.output_list, head) {

		if (output->crtc != crtc)
			continue;
		
		output->funcs->commit(output);

#if 0 // TODO def RANDR_12_INTERFACE
		if (output->randr_output)
			RRPostPendingProperties (output->randr_output);
#endif
	}
	
	/* XXX free adjustedmode */
	drm_mode_destroy(dev, adjusted_mode);
	/* TODO */
//	if (scrn->pScreen)
//		drm_crtc_set_screen_sub_pixel_order(dev);

done:
	if (!ret) { 
		crtc->mode = saved_mode;
		crtc->x = saved_x;
		crtc->y = saved_y;
	}

	return ret;
}
EXPORT_SYMBOL(drm_crtc_set_mode);

/**
 * drm_disable_unused_functions - disable unused objects
 * @dev: DRM device
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * If an output or CRTC isn't part of @dev's mode_config, it can be disabled
 * by calling its dpms function, which should power it off.
 */
void drm_disable_unused_functions(struct drm_device *dev)
{
	struct drm_output *output;
	struct drm_crtc *crtc;

	list_for_each_entry(output, &dev->mode_config.output_list, head) {
		if (!output->crtc)
			(*output->funcs->dpms)(output, DPMSModeOff);
	}

	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		if (!crtc->enabled)
			crtc->funcs->dpms(crtc, DPMSModeOff);
	}
}
EXPORT_SYMBOL(drm_disable_unused_functions);

/**
 * drm_mode_probed_add - add a mode to the specified output's probed mode list
 * @output: output the new mode
 * @mode: mode data
 *
 * LOCKING:
 * Caller must hold mode config lock.
 * 
 * Add @mode to @output's mode list for later use.
 */
void drm_mode_probed_add(struct drm_output *output,
			 struct drm_display_mode *mode)
{
	list_add(&mode->head, &output->probed_modes);
}
EXPORT_SYMBOL(drm_mode_probed_add);

/**
 * drm_mode_remove - remove and free a mode
 * @output: output list to modify
 * @mode: mode to remove
 *
 * LOCKING:
 * Caller must hold mode config lock.
 * 
 * Remove @mode from @output's mode list, then free it.
 */
void drm_mode_remove(struct drm_output *output, struct drm_display_mode *mode)
{
	list_del(&mode->head);
	kfree(mode);
}
EXPORT_SYMBOL(drm_mode_remove);

/**
 * drm_output_create - create a new output
 * @dev: DRM device
 * @funcs: callbacks for this output
 * @name: user visible name of the output
 *
 * LOCKING:
 * Caller must hold @dev's mode_config lock.
 *
 * Creates a new drm_output structure and adds it to @dev's mode_config
 * structure.
 *
 * RETURNS:
 * Pointer to the new output or NULL on error.
 */
struct drm_output *drm_output_create(struct drm_device *dev,
				     const struct drm_output_funcs *funcs,
				     int output_type)
{
	struct drm_output *output = NULL;

	output = kzalloc(sizeof(struct drm_output), GFP_KERNEL);
	if (!output)
		return NULL;
		
	output->dev = dev;
	output->funcs = funcs;
	output->id = drm_idr_get(dev, output);
	output->output_type = output_type;
	output->output_type_id = 1; /* TODO */
	INIT_LIST_HEAD(&output->user_modes);
	INIT_LIST_HEAD(&output->probed_modes);
	INIT_LIST_HEAD(&output->modes);
	/* randr_output? */
	/* output_set_monitor(output)? */
	/* check for output_ignored(output)? */

	mutex_lock(&dev->mode_config.mutex);
	list_add_tail(&output->head, &dev->mode_config.output_list);
	dev->mode_config.num_output++;

	drm_output_attach_property(output, dev->mode_config.edid_property, 0);

	drm_output_attach_property(output, dev->mode_config.dpms_property, 0);

	mutex_unlock(&dev->mode_config.mutex);

	return output;

}
EXPORT_SYMBOL(drm_output_create);

/**
 * drm_output_destroy - remove an output
 * @output: output to remove
 *
 * LOCKING:
 * Caller must hold @dev's mode_config lock.
 *
 * Call @output's cleanup function, then remove the output from the DRM
 * mode_config after freeing @output's modes.
 */
void drm_output_destroy(struct drm_output *output)
{
	struct drm_device *dev = output->dev;
	struct drm_display_mode *mode, *t;

	if (*output->funcs->cleanup)
		(*output->funcs->cleanup)(output);

	list_for_each_entry_safe(mode, t, &output->probed_modes, head)
		drm_mode_remove(output, mode);

	list_for_each_entry_safe(mode, t, &output->modes, head)
		drm_mode_remove(output, mode);

	list_for_each_entry_safe(mode, t, &output->user_modes, head)
		drm_mode_remove(output, mode);

	mutex_lock(&dev->mode_config.mutex);
	drm_idr_put(dev, output->id);
	list_del(&output->head);
	mutex_unlock(&dev->mode_config.mutex);
	kfree(output);
}
EXPORT_SYMBOL(drm_output_destroy);


/**
 * drm_mode_create - create a new display mode
 * @dev: DRM device
 *
 * LOCKING:
 * None.
 *
 * Create a new drm_display_mode, give it an ID, and return it.
 *
 * RETURNS:
 * Pointer to new mode on success, NULL on error.
 */
struct drm_display_mode *drm_mode_create(struct drm_device *dev)
{
	struct drm_display_mode *nmode;

	nmode = kzalloc(sizeof(struct drm_display_mode), GFP_KERNEL);
	if (!nmode)
		return NULL;

	nmode->mode_id = drm_idr_get(dev, nmode);
	return nmode;
}
EXPORT_SYMBOL(drm_mode_create);

/**
 * drm_mode_destroy - remove a mode
 * @dev: DRM device
 * @mode: mode to remove
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * Free @mode's unique identifier, then free it.
 */
void drm_mode_destroy(struct drm_device *dev, struct drm_display_mode *mode)
{
	drm_idr_put(dev, mode->mode_id);

	kfree(mode);
}
EXPORT_SYMBOL(drm_mode_destroy);

static int drm_mode_create_standard_output_properties(struct drm_device *dev)
{
	int i;

	/*
	 * Standard properties (apply to all outputs)
	 */
	dev->mode_config.edid_property =
		drm_property_create(dev, DRM_MODE_PROP_BLOB | DRM_MODE_PROP_IMMUTABLE,
				    "EDID", 0);

	dev->mode_config.dpms_property =
		drm_property_create(dev, DRM_MODE_PROP_ENUM, 
			"DPMS", ARRAY_SIZE(drm_dpms_enum_list));
	for (i = 0; i < ARRAY_SIZE(drm_dpms_enum_list); i++)
		drm_property_add_enum(dev->mode_config.dpms_property, i, drm_dpms_enum_list[i].type, drm_dpms_enum_list[i].name);

	dev->mode_config.connector_type_property =
		drm_property_create(dev, DRM_MODE_PROP_ENUM | DRM_MODE_PROP_IMMUTABLE,
			"Connector Type", ARRAY_SIZE(drm_conn_enum_list));
	for (i = 0; i < ARRAY_SIZE(drm_conn_enum_list); i++)
		drm_property_add_enum(dev->mode_config.connector_type_property, i, drm_conn_enum_list[i].type, drm_conn_enum_list[i].name);

	dev->mode_config.connector_num_property =
		drm_property_create(dev, DRM_MODE_PROP_RANGE | DRM_MODE_PROP_IMMUTABLE,
			"Connector ID", 2);
	dev->mode_config.connector_num_property->values[0] = 0;
	dev->mode_config.connector_num_property->values[1] = 20;

	return 0;
}

/**
 * drm_create_tv_properties - create TV specific output properties
 * @dev: DRM device
 * @num_modes: number of different TV formats (modes) supported
 * @modes: array of pointers to strings containing name of each format
 *
 * Called by a driver's TV initialization routine, this function creates
 * the TV specific output properties for a given device.  Caller is
 * responsible for allocating a list of format names and passing them to
 * this routine.
 */
bool drm_create_tv_properties(struct drm_device *dev, int num_modes,
			      char *modes[])
{
	int i;

	dev->mode_config.tv_left_margin_property =
		drm_property_create(dev, DRM_MODE_PROP_RANGE |
				    DRM_MODE_PROP_IMMUTABLE,
				    "left margin", 2);
	dev->mode_config.tv_left_margin_property->values[0] = 0;
	dev->mode_config.tv_left_margin_property->values[1] = 100;

	dev->mode_config.tv_right_margin_property =
		drm_property_create(dev, DRM_MODE_PROP_RANGE,
				    "right margin", 2);
	dev->mode_config.tv_right_margin_property->values[0] = 0;
	dev->mode_config.tv_right_margin_property->values[1] = 100;

	dev->mode_config.tv_top_margin_property =
		drm_property_create(dev, DRM_MODE_PROP_RANGE,
				    "top margin", 2);
	dev->mode_config.tv_top_margin_property->values[0] = 0;
	dev->mode_config.tv_top_margin_property->values[1] = 100;

	dev->mode_config.tv_bottom_margin_property =
		drm_property_create(dev, DRM_MODE_PROP_RANGE,
				    "bottom margin", 2);
	dev->mode_config.tv_bottom_margin_property->values[0] = 0;
	dev->mode_config.tv_bottom_margin_property->values[1] = 100;

	dev->mode_config.tv_mode_property =
		drm_property_create(dev, DRM_MODE_PROP_ENUM,
				    "mode", num_modes);
	for (i = 0; i < num_modes; i++)
		drm_property_add_enum(dev->mode_config.tv_mode_property, i,
				      i, modes[i]);

	return 0;
}
EXPORT_SYMBOL(drm_create_tv_properties);

/**
 * drm_mode_config_init - initialize DRM mode_configuration structure
 * @dev: DRM device
 *
 * LOCKING:
 * None, should happen single threaded at init time.
 *
 * Initialize @dev's mode_config structure, used for tracking the graphics
 * configuration of @dev.
 */
void drm_mode_config_init(struct drm_device *dev)
{
	mutex_init(&dev->mode_config.mutex);
	INIT_LIST_HEAD(&dev->mode_config.fb_list);
	INIT_LIST_HEAD(&dev->mode_config.crtc_list);
	INIT_LIST_HEAD(&dev->mode_config.output_list);
	INIT_LIST_HEAD(&dev->mode_config.property_list);
	INIT_LIST_HEAD(&dev->mode_config.property_blob_list);
	idr_init(&dev->mode_config.crtc_idr);

	drm_mode_create_standard_output_properties(dev);

	/* Just to be sure */
	dev->mode_config.num_fb = 0;
	dev->mode_config.num_output = 0;
	dev->mode_config.num_crtc = 0;
	dev->mode_config.hotplug_counter = 0;
}
EXPORT_SYMBOL(drm_mode_config_init);

/**
 * drm_get_buffer_object - find the buffer object for a given handle
 * @dev: DRM device
 * @bo: pointer to caller's buffer_object pointer
 * @handle: handle to lookup
 *
 * LOCKING:
 * Must take @dev's struct_mutex to protect buffer object lookup.
 *
 * Given @handle, lookup the buffer object in @dev and put it in the caller's
 * @bo pointer.
 *
 * RETURNS:
 * Zero on success, -EINVAL if the handle couldn't be found.
 */
static int drm_get_buffer_object(struct drm_device *dev, struct drm_buffer_object **bo, unsigned long handle)
{
	struct drm_user_object *uo;
	struct drm_hash_item *hash;
	int ret;

	*bo = NULL;

	mutex_lock(&dev->struct_mutex);
	ret = drm_ht_find_item(&dev->object_hash, handle, &hash);
	if (ret) {
		DRM_ERROR("Couldn't find handle.\n");
		ret = -EINVAL;
		goto out_err;
	}

	uo = drm_hash_entry(hash, struct drm_user_object, hash);
	if (uo->type != drm_buffer_type) {
		ret = -EINVAL;
		goto out_err;
	}
	
	*bo = drm_user_object_entry(uo, struct drm_buffer_object, base);
	ret = 0;
out_err:
	mutex_unlock(&dev->struct_mutex);
	return ret;
}

/**
 * drm_pick_crtcs - pick crtcs for output devices
 * @dev: DRM device
 *
 * LOCKING:
 * Caller must hold mode config lock.
 */
static void drm_pick_crtcs (struct drm_device *dev)
{
	int c, o, assigned;
	struct drm_output *output, *output_equal;
	struct drm_crtc   *crtc;
	struct drm_display_mode *des_mode = NULL, *modes, *modes_equal;
	int found;

	list_for_each_entry(output, &dev->mode_config.output_list, head) {
       		output->crtc = NULL;
    
    		/* Don't hook up outputs that are disconnected ??
		 *
		 * This is debateable. Do we want fixed /dev/fbX or
		 * dynamic on hotplug (need mode code for that though) ?
		 *
		 * If we don't hook up outputs now, then we only create
		 * /dev/fbX for the output that's enabled, that's good as
		 * the users console will be on that output.
		 *
		 * If we do hook up outputs that are disconnected now, then
		 * the user may end up having to muck about with the fbcon
		 * map flags to assign his console to the enabled output. Ugh.
		 */
    		if (output->status != output_status_connected)
			continue;

		if (list_empty(&output->modes))
			continue;

		des_mode = NULL;
		found = 0;
		list_for_each_entry(des_mode, &output->modes, head) {
			if (des_mode->type & DRM_MODE_TYPE_PREFERRED) {
				found = 1;
				break;
			}
		}

		/* No preferred mode, let's just select the first available */
		if (!found) {
			des_mode = NULL;
			list_for_each_entry(des_mode, &output->modes, head) {
				break;
			}
		}

		c = -1;
		list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
			assigned = 0;

			c++;
			if ((output->possible_crtcs & (1 << c)) == 0)
		    		continue;
	
			list_for_each_entry(output_equal, &dev->mode_config.output_list, head) {
				if (output->id == output_equal->id)
					continue;

				/* Find out if crtc has been assigned before */
				if (output_equal->crtc == crtc)
					assigned = 1;
			}

#if 1 /* continue for now */
			if (assigned)
				continue;
#endif

			o = -1;
			list_for_each_entry(output_equal, &dev->mode_config.output_list, head) {
				o++;
				if (output->id == output_equal->id)
					continue;

				list_for_each_entry(modes, &output->modes, head) {
					list_for_each_entry(modes_equal, &output_equal->modes, head) {
						if (drm_mode_equal (modes, modes_equal)) {
							if ((output->possible_clones & output_equal->possible_clones) && (output_equal->crtc == crtc)) {
								printk("Cloning %s (0x%lx) to %s (0x%lx)\n",drm_get_output_name(output),output->possible_clones,drm_get_output_name(output_equal),output_equal->possible_clones);
								des_mode = modes;
								assigned = 0;
								goto clone;
							}
						}
					}
				}
			}

clone:
			/* crtc has been assigned skip it */
			if (assigned)
				continue;

			/* Found a CRTC to attach to, do it ! */
			output->crtc = crtc;
			output->crtc->desired_mode = des_mode;
			output->initial_x = 0;
			output->initial_y = 0;
			DRM_DEBUG("Desired mode for CRTC %d is 0x%x:%s\n",c,des_mode->mode_id, des_mode->name);
			break;
    		}
	}
}
EXPORT_SYMBOL(drm_pick_crtcs);

/**
 * drm_initial_config - setup a sane initial output configuration
 * @dev: DRM device
 * @can_grow: this configuration is growable
 *
 * LOCKING:
 * Called at init time, must take mode config lock.
 *
 * Scan the CRTCs and outputs and try to put together an initial setup.
 * At the moment, this is a cloned configuration across all heads with
 * a new framebuffer object as the backing store.
 *
 * RETURNS:
 * Zero if everything went ok, nonzero otherwise.
 */
bool drm_initial_config(struct drm_device *dev, bool can_grow)
{
	struct drm_output *output;
	struct drm_crtc *crtc;
	int ret = false;

	mutex_lock(&dev->mode_config.mutex);

	drm_crtc_probe_output_modes(dev, 2048, 2048);

	drm_pick_crtcs(dev);

	/* This is a little screwy, as we've already walked the outputs 
	 * above, but it's a little bit of magic too. There's the potential
	 * for things not to get setup above if an existing device gets
	 * re-assigned thus confusing the hardware. By walking the outputs
	 * this fixes up their crtc's.
	 */
	list_for_each_entry(output, &dev->mode_config.output_list, head) {

		/* can't setup the output if there's no assigned mode */
		if (!output->crtc || !output->crtc->desired_mode)
			continue;

		dev->driver->fb_probe(dev, output->crtc, output);

		/* and needs an attached fb */
		if (output->crtc->fb)
			drm_crtc_set_mode(output->crtc, output->crtc->desired_mode, 0, 0);
	}

	drm_disable_unused_functions(dev);

	mutex_unlock(&dev->mode_config.mutex);
	return ret;
}
EXPORT_SYMBOL(drm_initial_config);

/**
 * drm_mode_config_cleanup - free up DRM mode_config info
 * @dev: DRM device
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * Free up all the outputs and CRTCs associated with this DRM device, then
 * free up the framebuffers and associated buffer objects.
 *
 * FIXME: cleanup any dangling user buffer objects too
 */
void drm_mode_config_cleanup(struct drm_device *dev)
{
	struct drm_output *output, *ot;
	struct drm_crtc *crtc, *ct;
	struct drm_framebuffer *fb, *fbt;
	struct drm_property *property, *pt;

	list_for_each_entry_safe(output, ot, &dev->mode_config.output_list, head) {
		drm_sysfs_output_remove(output);
		drm_output_destroy(output);
	}

	list_for_each_entry_safe(property, pt, &dev->mode_config.property_list, head) {
		drm_property_destroy(dev, property);
	}

	list_for_each_entry_safe(fb, fbt, &dev->mode_config.fb_list, head) {
		/* there should only be bo of kernel type left */
		if (fb->bo->type != drm_bo_type_kernel)
			drm_framebuffer_destroy(fb);
		else
			dev->driver->fb_remove(dev, fb);
	}

	list_for_each_entry_safe(crtc, ct, &dev->mode_config.crtc_list, head) {
		drm_crtc_destroy(crtc);
	}

}
EXPORT_SYMBOL(drm_mode_config_cleanup);

/**
 * drm_crtc_set_config - set a new config from userspace
 * @crtc: CRTC to setup
 * @crtc_info: user provided configuration
 * @new_mode: new mode to set
 * @output_set: set of outputs for the new config
 * @fb: new framebuffer
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * Setup a new configuration, provided by the user in @crtc_info, and enable
 * it.
 *
 * RETURNS:
 * Zero. (FIXME)
 */
int drm_crtc_set_config(struct drm_mode_set *set)
{
	struct drm_device *dev;
	struct drm_crtc **save_crtcs, *new_crtc;
	bool save_enabled;
	bool changed = false;
	bool flip_or_move = false;
	struct drm_output *output;
	int count = 0, ro;

	DRM_DEBUG("\n");

	if (!set)
		return -EINVAL;

	if (!set->crtc)
		return -EINVAL;

	DRM_DEBUG("crtc: %p fb: %p outputs: %p num_outputs: %i (x, y) (%i, %i)\n", set->crtc, set->fb, set->outputs, set->num_outputs, set->x, set->y);
	dev = set->crtc->dev;

	/* save previous config */
	save_enabled = set->crtc->enabled;

	/* this is meant to be num_output not num_crtc */
	save_crtcs = kzalloc(dev->mode_config.num_output * sizeof(struct drm_crtc *), GFP_KERNEL);
	if (!save_crtcs)
		return -ENOMEM;

	/* We should be able to check here if the fb has the same properties
	 * and then just flip_or_move it */
	if (set->crtc->fb != set->fb)
		flip_or_move = true;

	if (set->x != set->crtc->x || set->y != set->crtc->y)
		flip_or_move = true;

	if (set->mode && !drm_mode_equal(set->mode, &set->crtc->mode)) {
		DRM_DEBUG("modes are different\n");
		drm_mode_debug_printmodeline(dev, &set->crtc->mode);
		drm_mode_debug_printmodeline(dev, set->mode);
		changed = true;
	}

	list_for_each_entry(output, &dev->mode_config.output_list, head) {
		save_crtcs[count++] = output->crtc;

		if (output->crtc == set->crtc)
			new_crtc = NULL;
		else
			new_crtc = output->crtc;

		for (ro = 0; ro < set->num_outputs; ro++) {
			if (set->outputs[ro] == output)
				new_crtc = set->crtc;
		}
		if (new_crtc != output->crtc) {
			changed = true;
			output->crtc = new_crtc;
		}
	}

	/* mode_set_base is not a required function */
	if (flip_or_move && !set->crtc->funcs->mode_set_base)
		changed = true;

	if (changed) {
		set->crtc->fb = set->fb;
		set->crtc->enabled = (set->mode != NULL);
		if (set->mode != NULL) {
			DRM_DEBUG("attempting to set mode from userspace\n");
			drm_mode_debug_printmodeline(dev, set->mode);
			if (!drm_crtc_set_mode(set->crtc, set->mode, set->x,
					       set->y)) {
				set->crtc->enabled = save_enabled;
				count = 0;
				list_for_each_entry(output, &dev->mode_config.output_list, head)
					output->crtc = save_crtcs[count++];
				kfree(save_crtcs);
				return -EINVAL;
			}
			/* TODO are these needed? */
			set->crtc->desired_x = set->x;
			set->crtc->desired_y = set->y;
			set->crtc->desired_mode = set->mode;
		}
		drm_disable_unused_functions(dev);
	} else if (flip_or_move) {
		if (set->crtc->fb != set->fb)
			set->crtc->fb = set->fb;
		set->crtc->funcs->mode_set_base(set->crtc, set->x, set->y);
	}

	kfree(save_crtcs);
	return 0;
}
EXPORT_SYMBOL(drm_crtc_set_config);

/**
 * drm_hotplug_stage_two
 * @dev DRM device
 * @output hotpluged output
 *
 * LOCKING.
 * Caller must hold mode config lock, function might grab struct lock.
 *
 * Stage two of a hotplug.
 *
 * RETURNS:
 * Zero on success, errno on failure.
 */
int drm_hotplug_stage_two(struct drm_device *dev, struct drm_output *output,
			  bool connected)
{
	int has_config = 0;

	dev->mode_config.hotplug_counter++;

	/* We might want to do something more here */
	if (!connected) {
		DRM_DEBUG("not connected\n");
		return 0;
	}

	if (output->crtc && output->crtc->desired_mode) {
		DRM_DEBUG("drm thinks that the output already has a config\n");
		has_config = 1;
	}

	drm_crtc_probe_output_modes(dev, 2048, 2048);

	if (!has_config)
		drm_pick_crtcs(dev);

	if (!output->crtc || !output->crtc->desired_mode) {
		DRM_DEBUG("could not find a desired mode or crtc for output\n");
		return 1;
	}

	/* We should really check if there is a fb using this crtc */
	if (!has_config)
		dev->driver->fb_probe(dev, output->crtc, output);
	else {
		dev->driver->fb_resize(dev, output->crtc);

		if (!drm_crtc_set_mode(output->crtc, output->crtc->desired_mode, 0, 0))
			DRM_ERROR("failed to set mode after hotplug\n");
	}

	drm_sysfs_hotplug_event(dev);

	drm_disable_unused_functions(dev);

	return 0;
}
EXPORT_SYMBOL(drm_hotplug_stage_two);

int drm_mode_hotplug_ioctl(struct drm_device *dev,
			   void *data, struct drm_file *file_priv)
{
	struct drm_mode_hotplug *arg = data;

	arg->counter = dev->mode_config.hotplug_counter;

	return 0;
}

/**
 * drm_crtc_convert_to_umode - convert a drm_display_mode into a modeinfo
 * @out: drm_mode_modeinfo struct to return to the user
 * @in: drm_display_mode to use
 *
 * LOCKING:
 * None.
 *
 * Convert a drm_display_mode into a drm_mode_modeinfo structure to return to
 * the user.
 */
void drm_crtc_convert_to_umode(struct drm_mode_modeinfo *out, struct drm_display_mode *in)
{
	out->clock = in->clock;
	out->hdisplay = in->hdisplay;
	out->hsync_start = in->hsync_start;
	out->hsync_end = in->hsync_end;
	out->htotal = in->htotal;
	out->hskew = in->hskew;
	out->vdisplay = in->vdisplay;
	out->vsync_start = in->vsync_start;
	out->vsync_end = in->vsync_end;
	out->vtotal = in->vtotal;
	out->vscan = in->vscan;
	out->vrefresh = in->vrefresh;
	out->flags = in->flags;
	out->type = in->type;
	strncpy(out->name, in->name, DRM_DISPLAY_MODE_LEN);
	out->name[DRM_DISPLAY_MODE_LEN-1] = 0;
}

/**
 * drm_crtc_convert_to_umode - convert a modeinfo into a drm_display_mode
 * @out: drm_display_mode to return to the user
 * @in: drm_mode_modeinfo to use
 *
 * LOCKING:
 * None.
 *
 * Convert a drmo_mode_modeinfo into a drm_display_mode structure to return to
 * the caller.
 */
void drm_crtc_convert_umode(struct drm_display_mode *out, struct drm_mode_modeinfo *in)
{
	out->clock = in->clock;
	out->hdisplay = in->hdisplay;
	out->hsync_start = in->hsync_start;
	out->hsync_end = in->hsync_end;
	out->htotal = in->htotal;
	out->hskew = in->hskew;
	out->vdisplay = in->vdisplay;
	out->vsync_start = in->vsync_start;
	out->vsync_end = in->vsync_end;
	out->vtotal = in->vtotal;
	out->vscan = in->vscan;
	out->vrefresh = in->vrefresh;
	out->flags = in->flags;
	out->type = in->type;
	strncpy(out->name, in->name, DRM_DISPLAY_MODE_LEN);
	out->name[DRM_DISPLAY_MODE_LEN-1] = 0;
}
	
/**
 * drm_mode_getresources - get graphics configuration
 * @inode: inode from the ioctl
 * @filp: file * from the ioctl
 * @cmd: cmd from ioctl
 * @arg: arg from ioctl
 *
 * LOCKING:
 * Takes mode config lock.
 *
 * Construct a set of configuration description structures and return
 * them to the user, including CRTC, output and framebuffer configuration.
 *
 * Called by the user via ioctl.
 *
 * RETURNS:
 * Zero on success, errno on failure.
 */
int drm_mode_getresources(struct drm_device *dev,
			  void *data, struct drm_file *file_priv)
{
	struct drm_mode_card_res *card_res = data;
	struct list_head *lh;
	struct drm_framebuffer *fb;
	struct drm_output *output;
	struct drm_crtc *crtc;
	int ret = 0;
	int output_count = 0;
	int crtc_count = 0;
	int fb_count = 0;
	int copied = 0;
	uint32_t __user *fb_id;
	uint32_t __user *crtc_id;
	uint32_t __user *output_id;

	mutex_lock(&dev->mode_config.mutex);

	list_for_each(lh, &dev->mode_config.fb_list)
		fb_count++;

	list_for_each(lh, &dev->mode_config.crtc_list)
		crtc_count++;

	list_for_each(lh, &dev->mode_config.output_list)
		output_count++;

	card_res->max_height = dev->mode_config.max_height;
	card_res->min_height = dev->mode_config.min_height;
	card_res->max_width = dev->mode_config.max_width;
	card_res->min_width = dev->mode_config.min_width;

	/* handle this in 4 parts */
	/* FBs */
	if (card_res->count_fbs >= fb_count) {
		copied = 0;
		fb_id = (uint32_t *)(unsigned long)card_res->fb_id_ptr;
		list_for_each_entry(fb, &dev->mode_config.fb_list, head) {
			if (put_user(fb->id, fb_id + copied)) {
				ret = -EFAULT;
				goto out;
			}
			copied++;
		}
	}
	card_res->count_fbs = fb_count;

	/* CRTCs */
	if (card_res->count_crtcs >= crtc_count) {
		copied = 0;
		crtc_id = (uint32_t *)(unsigned long)card_res->crtc_id_ptr;
		list_for_each_entry(crtc, &dev->mode_config.crtc_list, head){
			DRM_DEBUG("CRTC ID is %d\n", crtc->id);
			if (put_user(crtc->id, crtc_id + copied)) {
				ret = -EFAULT;
				goto out;
			}
			copied++;
		}
	}
	card_res->count_crtcs = crtc_count;


	/* Outputs */
	if (card_res->count_outputs >= output_count) {
		copied = 0;
		output_id = (uint32_t *)(unsigned long)card_res->output_id_ptr;
		list_for_each_entry(output, &dev->mode_config.output_list,
				    head) {
 			DRM_DEBUG("OUTPUT ID is %d\n", output->id);
			if (put_user(output->id, output_id + copied)) {
				ret = -EFAULT;
				goto out;
			}
			copied++;
		}
	}
	card_res->count_outputs = output_count;
	
	DRM_DEBUG("Counted %d %d\n", card_res->count_crtcs,
		  card_res->count_outputs);

out:	
	mutex_unlock(&dev->mode_config.mutex);
	return ret;
}

/**
 * drm_mode_getcrtc - get CRTC configuration
 * @inode: inode from the ioctl
 * @filp: file * from the ioctl
 * @cmd: cmd from ioctl
 * @arg: arg from ioctl
 *
 * LOCKING:
 * Caller? (FIXME)
 *
 * Construct a CRTC configuration structure to return to the user.
 *
 * Called by the user via ioctl.
 *
 * RETURNS:
 * Zero on success, errno on failure.
 */
int drm_mode_getcrtc(struct drm_device *dev,
		     void *data, struct drm_file *file_priv)
{
	struct drm_mode_crtc *crtc_resp = data;
	struct drm_crtc *crtc;
	struct drm_output *output;
	int ocount;
	int ret = 0;

	mutex_lock(&dev->mode_config.mutex);
	crtc = idr_find(&dev->mode_config.crtc_idr, crtc_resp->crtc_id);
	if (!crtc || (crtc->id != crtc_resp->crtc_id)) {
		ret = -EINVAL;
		goto out;
	}

	crtc_resp->x = crtc->x;
	crtc_resp->y = crtc->y;

	if (crtc->fb)
		crtc_resp->fb_id = crtc->fb->id;
	else
		crtc_resp->fb_id = 0;

	crtc_resp->outputs = 0;
	if (crtc->enabled) {

		drm_crtc_convert_to_umode(&crtc_resp->mode, &crtc->mode);
		crtc_resp->mode_valid = 1;
		ocount = 0;
		list_for_each_entry(output, &dev->mode_config.output_list, head) {
			if (output->crtc == crtc)
				crtc_resp->outputs |= 1 << (ocount++);
		}
		
	} else {
		crtc_resp->mode_valid = 0;
	}

out:
	mutex_unlock(&dev->mode_config.mutex);
	return ret;
}

/**
 * drm_mode_getoutput - get output configuration
 * @inode: inode from the ioctl
 * @filp: file * from the ioctl
 * @cmd: cmd from ioctl
 * @arg: arg from ioctl
 *
 * LOCKING:
 * Caller? (FIXME)
 *
 * Construct a output configuration structure to return to the user.
 *
 * Called by the user via ioctl.
 *
 * RETURNS:
 * Zero on success, errno on failure.
 */
int drm_mode_getoutput(struct drm_device *dev,
		       void *data, struct drm_file *file_priv)
{
	struct drm_mode_get_output *out_resp = data;
	struct drm_output *output;
	struct drm_display_mode *mode;
	int mode_count = 0;
	int props_count = 0;
	int ret = 0;
	int copied = 0;
	int i;
	struct drm_mode_modeinfo u_mode;
	struct drm_mode_modeinfo __user *mode_ptr;
	uint32_t __user *prop_ptr;
	uint64_t __user *prop_values;

	memset(&u_mode, 0, sizeof(struct drm_mode_modeinfo));

	DRM_DEBUG("output id %d:\n", out_resp->output);

	mutex_lock(&dev->mode_config.mutex);
	output= idr_find(&dev->mode_config.crtc_idr, out_resp->output);
	if (!output || (output->id != out_resp->output)) {
		ret = -EINVAL;
		goto out;
	}

	list_for_each_entry(mode, &output->modes, head)
		mode_count++;
	
	for (i = 0; i < DRM_OUTPUT_MAX_PROPERTY; i++) {
		if (output->property_ids[i] != 0) {
			props_count++;
		}
	}

	if (out_resp->count_modes == 0) {
		drm_crtc_probe_single_output_modes(output, dev->mode_config.max_width, dev->mode_config.max_height);
	}

	out_resp->output_type = output->output_type;
	out_resp->output_type_id = output->output_type_id;
	out_resp->mm_width = output->display_info.width_mm;
	out_resp->mm_height = output->display_info.height_mm;
	out_resp->subpixel = output->display_info.subpixel_order;
	out_resp->connection = output->status;
	if (output->crtc)
		out_resp->crtc = output->crtc->id;
	else
		out_resp->crtc = 0;

	out_resp->crtcs = output->possible_crtcs;
	out_resp->clones = output->possible_clones;

	if ((out_resp->count_modes >= mode_count) && mode_count) {
		copied = 0;
		mode_ptr = (struct drm_mode_modeinfo *)(unsigned long)out_resp->modes_ptr;
		list_for_each_entry(mode, &output->modes, head) {
			drm_crtc_convert_to_umode(&u_mode, mode);
			if (copy_to_user(mode_ptr + copied,
					 &u_mode, sizeof(u_mode))) {
				ret = -EFAULT;
				goto out;
			}
			copied++;
			
		}
	}
	out_resp->count_modes = mode_count;

	if ((out_resp->count_props >= props_count) && props_count) {
		copied = 0;
		prop_ptr = (uint32_t *)(unsigned long)(out_resp->props_ptr);
		prop_values = (uint64_t *)(unsigned long)(out_resp->prop_values_ptr);
		for (i = 0; i < DRM_OUTPUT_MAX_PROPERTY; i++) {
			if (output->property_ids[i] != 0) {
				if (put_user(output->property_ids[i], prop_ptr + copied)) {
					ret = -EFAULT;
					goto out;
				}

				if (put_user(output->property_values[i], prop_values + copied)) {
					ret = -EFAULT;
					goto out;
				}
				copied++;
			}
		}
	}
	out_resp->count_props = props_count;

out:
	mutex_unlock(&dev->mode_config.mutex);
	return ret;
}

/**
 * drm_mode_setcrtc - set CRTC configuration
 * @inode: inode from the ioctl
 * @filp: file * from the ioctl
 * @cmd: cmd from ioctl
 * @arg: arg from ioctl
 *
 * LOCKING:
 * Caller? (FIXME)
 *
 * Build a new CRTC configuration based on user request.
 *
 * Called by the user via ioctl.
 *
 * RETURNS:
 * Zero on success, errno on failure.
 */
int drm_mode_setcrtc(struct drm_device *dev,
		     void *data, struct drm_file *file_priv)
{
	struct drm_mode_crtc *crtc_req = data;
	struct drm_crtc *crtc, *crtcfb;
	struct drm_output **output_set = NULL, *output;
	struct drm_framebuffer *fb = NULL;
	struct drm_display_mode *mode = NULL;
	struct drm_mode_set set;
	uint32_t __user *set_outputs_ptr;
	int ret = 0;
	int i;

	mutex_lock(&dev->mode_config.mutex);
	crtc = idr_find(&dev->mode_config.crtc_idr, crtc_req->crtc_id);
	if (!crtc || (crtc->id != crtc_req->crtc_id)) {
		DRM_DEBUG("Unknown CRTC ID %d\n", crtc_req->crtc_id);
		ret = -EINVAL;
		goto out;
	}

	if (crtc_req->mode_valid) {
		/* If we have a mode we need a framebuffer. */
		/* If we pass -1, set the mode with the currently bound fb */
		if (crtc_req->fb_id == -1) {
			list_for_each_entry(crtcfb, &dev->mode_config.crtc_list, head) {
				if (crtcfb == crtc) {
					DRM_DEBUG("Using current fb for setmode\n");
					fb = crtc->fb;		
				}
			}
		} else {
			fb = idr_find(&dev->mode_config.crtc_idr, crtc_req->fb_id);
			if (!fb || (fb->id != crtc_req->fb_id)) {
				DRM_DEBUG("Unknown FB ID%d\n", crtc_req->fb_id);
				ret = -EINVAL;
				goto out;
			}
		}

		mode = drm_mode_create(dev);
		drm_crtc_convert_umode(mode, &crtc_req->mode);
		drm_mode_set_crtcinfo(mode, CRTC_INTERLACE_HALVE_V);
	}

	if (crtc_req->count_outputs == 0 && mode) {
		DRM_DEBUG("Count outputs is 0 but mode set\n");
		ret = -EINVAL;
		goto out;
	}

	if (crtc_req->count_outputs > 0 && !mode && !fb) {
		DRM_DEBUG("Count outputs is %d but no mode or fb set\n", crtc_req->count_outputs);
		ret = -EINVAL;
		goto out;
	}

	if (crtc_req->count_outputs > 0) {
		u32 out_id;
		/* Maybe we should check that count_outputs is a sensible value. */
		output_set = kmalloc(crtc_req->count_outputs *
				     sizeof(struct drm_output *), GFP_KERNEL);
		if (!output_set) {
			ret = -ENOMEM;
			goto out;
		}

		for (i = 0; i < crtc_req->count_outputs; i++) {
			set_outputs_ptr = (uint32_t *)(unsigned long)crtc_req->set_outputs_ptr;
			if (get_user(out_id, &set_outputs_ptr[i])) {
				ret = -EFAULT;
				goto out;
			}

			output = idr_find(&dev->mode_config.crtc_idr, out_id);
			if (!output || (out_id != output->id)) {
				DRM_DEBUG("Output id %d unknown\n", out_id);
				ret = -EINVAL;
				goto out;
			}

			output_set[i] = output;
		}
	}

	set.crtc = crtc;
	set.x = crtc_req->x;
	set.y = crtc_req->y;
	set.mode = mode;
	set.outputs = output_set;
	set.num_outputs = crtc_req->count_outputs;
	set.fb =fb;
	ret = drm_crtc_set_config(&set);

out:
	kfree(output_set);
	mutex_unlock(&dev->mode_config.mutex);
	return ret;
}

int drm_mode_cursor_ioctl(struct drm_device *dev,
			void *data, struct drm_file *file_priv)
{
	struct drm_mode_cursor *req = data;
	struct drm_crtc *crtc;
	struct drm_buffer_object *bo = NULL; /* must be set */
	int ret = 0;

	DRM_DEBUG("\n");

	if (!req->flags) {
		DRM_ERROR("no operation set\n");
		return -EINVAL;
	}

	mutex_lock(&dev->mode_config.mutex);
	crtc = idr_find(&dev->mode_config.crtc_idr, req->crtc);
	if (!crtc || (crtc->id != req->crtc)) {
		DRM_DEBUG("Unknown CRTC ID %d\n", req->crtc);
		ret = -EINVAL;
		goto out;
	}

	if (req->flags & DRM_MODE_CURSOR_BO) {
		/* Turn of the cursor if handle is 0 */
		if (req->handle)
			ret = drm_get_buffer_object(dev, &bo, req->handle);

		if (ret) {
			DRM_ERROR("invalid buffer id\n");
			ret = -EINVAL;
			goto out;
		}

		if (crtc->funcs->cursor_set) {
			ret = crtc->funcs->cursor_set(crtc, bo, req->width, req->height);
		} else {
			DRM_ERROR("crtc does not support cursor\n");
			ret = -EFAULT;
			goto out;
		}
	}

	if (req->flags & DRM_MODE_CURSOR_MOVE) {
		if (crtc->funcs->cursor_move) {
			ret = crtc->funcs->cursor_move(crtc, req->x, req->y);
		} else {
			DRM_ERROR("crtc does not support cursor\n");
			ret = -EFAULT;
			goto out;
		}
	}
out:
	mutex_unlock(&dev->mode_config.mutex);
	return ret;
}

/**
 * drm_mode_addfb - add an FB to the graphics configuration
 * @inode: inode from the ioctl
 * @filp: file * from the ioctl
 * @cmd: cmd from ioctl
 * @arg: arg from ioctl
 *
 * LOCKING:
 * Takes mode config lock.
 *
 * Add a new FB to the specified CRTC, given a user request.
 *
 * Called by the user via ioctl.
 *
 * RETURNS:
 * Zero on success, errno on failure.
 */
int drm_mode_addfb(struct drm_device *dev,
		   void *data, struct drm_file *file_priv)
{
	struct drm_mode_fb_cmd *r = data;
	struct drm_mode_config *config = &dev->mode_config;
	struct drm_framebuffer *fb;
	struct drm_buffer_object *bo;
	int ret = 0;

	if ((config->min_width > r->width) || (r->width > config->max_width)) {
		DRM_ERROR("mode new framebuffer width not within limits\n");
		return -EINVAL;
	}
	if ((config->min_height > r->height) || (r->height > config->max_height)) {
		DRM_ERROR("mode new framebuffer height not within limits\n");
		return -EINVAL;
	}

	mutex_lock(&dev->mode_config.mutex);
	/* TODO check limits are okay */
	ret = drm_get_buffer_object(dev, &bo, r->handle);
	if (ret || !bo) {
		DRM_ERROR("BO handle not valid\n");
		ret = -EINVAL;
		goto out;
	}

	/* TODO check buffer is sufficently large */
	/* TODO setup destructor callback */

	fb = drm_framebuffer_create(dev);
	if (!fb) {
		DRM_ERROR("could not create framebuffer\n");
		ret = -EINVAL;
		goto out;
	}

	fb->width = r->width;
	fb->height = r->height;
	fb->pitch = r->pitch;
	fb->bits_per_pixel = r->bpp;
	fb->depth = r->depth;
	fb->bo = bo;

	r->buffer_id = fb->id;

	list_add(&fb->filp_head, &file_priv->fbs);

out:
	mutex_unlock(&dev->mode_config.mutex);
	return ret;
}

/**
 * drm_mode_rmfb - remove an FB from the configuration
 * @inode: inode from the ioctl
 * @filp: file * from the ioctl
 * @cmd: cmd from ioctl
 * @arg: arg from ioctl
 *
 * LOCKING:
 * Takes mode config lock.
 *
 * Remove the FB specified by the user.
 *
 * Called by the user via ioctl.
 *
 * RETURNS:
 * Zero on success, errno on failure.
 */
int drm_mode_rmfb(struct drm_device *dev,
		   void *data, struct drm_file *file_priv)
{
	struct drm_framebuffer *fb = NULL;
	struct drm_framebuffer *fbl = NULL;
	uint32_t *id = data;
	int ret = 0;
	int found = 0;

	mutex_lock(&dev->mode_config.mutex);
	fb = idr_find(&dev->mode_config.crtc_idr, *id);
	/* TODO check that we realy get a framebuffer back. */
	if (!fb || (*id != fb->id)) {
		DRM_ERROR("mode invalid framebuffer id\n");
		ret = -EINVAL;
		goto out;
	}

	list_for_each_entry(fbl, &file_priv->fbs, filp_head)
		if (fb == fbl)
			found = 1;

	if (!found) {
		DRM_ERROR("tried to remove a fb that we didn't own\n");
		ret = -EINVAL;
		goto out;
	}

	/* TODO release all crtc connected to the framebuffer */
	/* TODO unhock the destructor from the buffer object */

	if (fb->bo->type == drm_bo_type_kernel)
		DRM_ERROR("the bo type should not be of kernel type\n");

	list_del(&fb->filp_head);
	drm_framebuffer_destroy(fb);

out:
	mutex_unlock(&dev->mode_config.mutex);
	return ret;
}

/**
 * drm_mode_getfb - get FB info
 * @inode: inode from the ioctl
 * @filp: file * from the ioctl
 * @cmd: cmd from ioctl
 * @arg: arg from ioctl
 *
 * LOCKING:
 * Caller? (FIXME)
 *
 * Lookup the FB given its ID and return info about it.
 *
 * Called by the user via ioctl.
 *
 * RETURNS:
 * Zero on success, errno on failure.
 */
int drm_mode_getfb(struct drm_device *dev,
		   void *data, struct drm_file *file_priv)
{
	struct drm_mode_fb_cmd *r = data;
	struct drm_framebuffer *fb;
	int ret = 0;

	mutex_lock(&dev->mode_config.mutex);
	fb = idr_find(&dev->mode_config.crtc_idr, r->buffer_id);
	if (!fb || (r->buffer_id != fb->id)) {
		DRM_ERROR("invalid framebuffer id\n");
		ret = -EINVAL;
		goto out;
	}

	r->height = fb->height;
	r->width = fb->width;
	r->depth = fb->depth;
	r->bpp = fb->bits_per_pixel;
	r->handle = fb->bo->base.hash.key;
	r->pitch = fb->pitch;

out:
	mutex_unlock(&dev->mode_config.mutex);
	return ret;
}

/**
 * drm_fb_release - remove and free the FBs on this file
 * @filp: file * from the ioctl
 *
 * LOCKING:
 * Takes mode config lock.
 *
 * Destroy all the FBs associated with @filp.
 *
 * Called by the user via ioctl.
 *
 * RETURNS:
 * Zero on success, errno on failure.
 */
void drm_fb_release(struct file *filp)
{
	struct drm_file *priv = filp->private_data;
	struct drm_device *dev = priv->minor->dev;
	struct drm_framebuffer *fb, *tfb;

	mutex_lock(&dev->mode_config.mutex);
	list_for_each_entry_safe(fb, tfb, &priv->fbs, filp_head) {
		list_del(&fb->filp_head);
		if (fb->bo->type == drm_bo_type_kernel)
			DRM_ERROR("the bo type should not be of kernel_type, the kernel will probably explode, why Dave\n");

		drm_framebuffer_destroy(fb);
	}
	mutex_unlock(&dev->mode_config.mutex);
}

/*
 *
 */

static int drm_mode_attachmode(struct drm_device *dev,
			       struct drm_output *output,
			       struct drm_display_mode *mode)
{
	int ret = 0;

	list_add_tail(&mode->head, &output->user_modes);
	return ret;
}

int drm_mode_attachmode_crtc(struct drm_device *dev, struct drm_crtc *crtc,
			     struct drm_display_mode *mode)
{
	struct drm_output *output;
	int ret = 0;
	struct drm_display_mode *dup_mode;
	int need_dup = 0;
	list_for_each_entry(output, &dev->mode_config.output_list, head) {
		if (output->crtc == crtc) {
			if (need_dup)
				dup_mode = drm_mode_duplicate(dev, mode);
			else
				dup_mode = mode;
			ret = drm_mode_attachmode(dev, output, dup_mode); 
			if (ret)
				return ret;
			need_dup = 1;
		}
	}
	return 0;
}
EXPORT_SYMBOL(drm_mode_attachmode_crtc);

static int drm_mode_detachmode(struct drm_device *dev,
			       struct drm_output *output,
			       struct drm_display_mode *mode)
{
	int found = 0;
	int ret = 0;
	struct drm_display_mode *match_mode, *t;

	list_for_each_entry_safe(match_mode, t, &output->user_modes, head) {
		if (drm_mode_equal(match_mode, mode)) {
			list_del(&match_mode->head);
			drm_mode_destroy(dev, match_mode);
			found = 1;
			break;
		}
	}

	if (!found)
		ret = -EINVAL;

	return ret;
}

int drm_mode_detachmode_crtc(struct drm_device *dev, struct drm_display_mode *mode)
{
	struct drm_output *output;

	list_for_each_entry(output, &dev->mode_config.output_list, head) {
		drm_mode_detachmode(dev, output, mode);
	}
	return 0;
}
EXPORT_SYMBOL(drm_mode_detachmode_crtc);

/**
 * drm_fb_attachmode - Attach a user mode to an output
 * @inode: inode from the ioctl
 * @filp: file * from the ioctl
 * @cmd: cmd from ioctl
 * @arg: arg from ioctl
 *
 * This attaches a user specified mode to an output.
 * Called by the user via ioctl.
 *
 * RETURNS:
 * Zero on success, errno on failure.
 */
int drm_mode_attachmode_ioctl(struct drm_device *dev,
			      void *data, struct drm_file *file_priv)
{
	struct drm_mode_mode_cmd *mode_cmd = data;
	struct drm_output *output;
	struct drm_display_mode *mode;
	struct drm_mode_modeinfo *umode = &mode_cmd->mode;
	int ret = 0;

	mutex_lock(&dev->mode_config.mutex);

	output = idr_find(&dev->mode_config.crtc_idr, mode_cmd->output_id);
	if (!output || (output->id != mode_cmd->output_id)) {
		ret = -EINVAL;
		goto out;
	}

	mode = drm_mode_create(dev);
	if (!mode) {
		ret = -ENOMEM;
		goto out;
	}
	
	drm_crtc_convert_umode(mode, umode);

	ret = drm_mode_attachmode(dev, output, mode);
out:
	mutex_unlock(&dev->mode_config.mutex);
	return ret;
}


/**
 * drm_fb_detachmode - Detach a user specified mode from an output
 * @inode: inode from the ioctl
 * @filp: file * from the ioctl
 * @cmd: cmd from ioctl
 * @arg: arg from ioctl
 *
 * Called by the user via ioctl.
 *
 * RETURNS:
 * Zero on success, errno on failure.
 */
int drm_mode_detachmode_ioctl(struct drm_device *dev,
			      void *data, struct drm_file *file_priv)
{
	struct drm_mode_mode_cmd *mode_cmd = data;
	struct drm_output *output;
	struct drm_display_mode mode;
	struct drm_mode_modeinfo *umode = &mode_cmd->mode;
	int ret = 0;

	mutex_lock(&dev->mode_config.mutex);

	output = idr_find(&dev->mode_config.crtc_idr, mode_cmd->output_id);
	if (!output || (output->id != mode_cmd->output_id)) {
		ret = -EINVAL;
		goto out;
	}
	
	drm_crtc_convert_umode(&mode, umode);
	ret = drm_mode_detachmode(dev, output, &mode);
out:	       
	mutex_unlock(&dev->mode_config.mutex);
	return ret;
}

struct drm_property *drm_property_create(struct drm_device *dev, int flags,
					 const char *name, int num_values)
{
	struct drm_property *property = NULL;

	property = kzalloc(sizeof(struct drm_property), GFP_KERNEL);
	if (!property)
		return NULL;

	if (num_values) {
		property->values = kzalloc(sizeof(uint64_t)*num_values, GFP_KERNEL);
		if (!property->values)
			goto fail;
	}

	property->id = drm_idr_get(dev, property);
	property->flags = flags;
	property->num_values = num_values;
	INIT_LIST_HEAD(&property->enum_blob_list);

	if (name)
		strncpy(property->name, name, DRM_PROP_NAME_LEN);

	list_add_tail(&property->head, &dev->mode_config.property_list);
	return property;
fail:
	kfree(property);
	return NULL;
}
EXPORT_SYMBOL(drm_property_create);

int drm_property_add_enum(struct drm_property *property, int index,
			  uint64_t value, const char *name)
{
	struct drm_property_enum *prop_enum;

	if (!(property->flags & DRM_MODE_PROP_ENUM))
		return -EINVAL;

	if (!list_empty(&property->enum_blob_list)) {
		list_for_each_entry(prop_enum, &property->enum_blob_list, head) {
			if (prop_enum->value == value) {
				strncpy(prop_enum->name, name, DRM_PROP_NAME_LEN); 
				prop_enum->name[DRM_PROP_NAME_LEN-1] = '\0';
				return 0;
			}
		}
	}

	prop_enum = kzalloc(sizeof(struct drm_property_enum), GFP_KERNEL);
	if (!prop_enum)
		return -ENOMEM;

	strncpy(prop_enum->name, name, DRM_PROP_NAME_LEN); 
	prop_enum->name[DRM_PROP_NAME_LEN-1] = '\0';
	prop_enum->value = value;

	property->values[index] = value;
	list_add_tail(&prop_enum->head, &property->enum_blob_list);
	return 0;
}
EXPORT_SYMBOL(drm_property_add_enum);

void drm_property_destroy(struct drm_device *dev, struct drm_property *property)
{
	struct drm_property_enum *prop_enum, *pt;

	list_for_each_entry_safe(prop_enum, pt, &property->enum_blob_list, head) {
		list_del(&prop_enum->head);
		kfree(prop_enum);
	}

	if (property->num_values)
		kfree(property->values);
	drm_idr_put(dev, property->id);
	list_del(&property->head);
	kfree(property);	
}
EXPORT_SYMBOL(drm_property_destroy);

int drm_output_attach_property(struct drm_output *output,
			       struct drm_property *property, uint64_t init_val)
{
	int i;

	for (i = 0; i < DRM_OUTPUT_MAX_PROPERTY; i++) {
		if (output->property_ids[i] == 0) {
			output->property_ids[i] = property->id;
			output->property_values[i] = init_val;
			break;
		}
	}

	if (i == DRM_OUTPUT_MAX_PROPERTY)
		return -EINVAL;
	return 0;
}
EXPORT_SYMBOL(drm_output_attach_property);

int drm_output_property_set_value(struct drm_output *output,
				  struct drm_property *property, uint64_t value)
{
	int i;

	for (i = 0; i < DRM_OUTPUT_MAX_PROPERTY; i++) {
		if (output->property_ids[i] == property->id) {
			output->property_values[i] = value;
			break;
		}
	}

	if (i == DRM_OUTPUT_MAX_PROPERTY)
		return -EINVAL;
	return 0;
}
EXPORT_SYMBOL(drm_output_property_set_value);

int drm_output_property_get_value(struct drm_output *output,
				  struct drm_property *property, uint64_t *val)
{
	int i;

	for (i = 0; i < DRM_OUTPUT_MAX_PROPERTY; i++) {
		if (output->property_ids[i] == property->id) {
			*val = output->property_values[i];
			break;
		}
	}

	if (i == DRM_OUTPUT_MAX_PROPERTY)
		return -EINVAL;
	return 0;
}
EXPORT_SYMBOL(drm_output_property_get_value);

int drm_mode_getproperty_ioctl(struct drm_device *dev,
			       void *data, struct drm_file *file_priv)
{
	struct drm_mode_get_property *out_resp = data;
	struct drm_property *property;
	int enum_count = 0;
	int blob_count = 0;
	int value_count = 0;
	int ret = 0, i;
	int copied;
	struct drm_property_enum *prop_enum;
	struct drm_mode_property_enum __user *enum_ptr;
	struct drm_property_blob *prop_blob;
	uint32_t *blob_id_ptr;
	uint64_t __user *values_ptr;
	uint32_t __user *blob_length_ptr;

	mutex_lock(&dev->mode_config.mutex);
	property = idr_find(&dev->mode_config.crtc_idr, out_resp->prop_id);
	if (!property || (property->id != out_resp->prop_id)) {
		ret = -EINVAL;
		goto done;
	}

	if (property->flags & DRM_MODE_PROP_ENUM) {
		list_for_each_entry(prop_enum, &property->enum_blob_list, head)
			enum_count++;
	} else if (property->flags & DRM_MODE_PROP_BLOB) {
		list_for_each_entry(prop_blob, &property->enum_blob_list, head)
			blob_count++;
	}

	value_count = property->num_values;

	strncpy(out_resp->name, property->name, DRM_PROP_NAME_LEN);
	out_resp->name[DRM_PROP_NAME_LEN-1] = 0;
	out_resp->flags = property->flags;

	if ((out_resp->count_values >= value_count) && value_count) {
		values_ptr = (uint64_t *)(unsigned long)out_resp->values_ptr;
		for (i = 0; i < value_count; i++) {
			if (copy_to_user(values_ptr + i, &property->values[i], sizeof(uint64_t))) {
				ret = -EFAULT;
				goto done;
			}
		}
	}
	out_resp->count_values = value_count;

	if (property->flags & DRM_MODE_PROP_ENUM) {

		if ((out_resp->count_enum_blobs >= enum_count) && enum_count) {
			copied = 0;
			enum_ptr = (struct drm_mode_property_enum *)(unsigned long)out_resp->enum_blob_ptr;
			list_for_each_entry(prop_enum, &property->enum_blob_list, head) {
				
				if (copy_to_user(&enum_ptr[copied].value, &prop_enum->value, sizeof(uint64_t))) {
					ret = -EFAULT;
					goto done;
				}
				
				if (copy_to_user(&enum_ptr[copied].name,
						 &prop_enum->name, DRM_PROP_NAME_LEN)) {
					ret = -EFAULT;
					goto done;
				}
				copied++;
			}
		}
		out_resp->count_enum_blobs = enum_count;
	}

	if (property->flags & DRM_MODE_PROP_BLOB) {
		if ((out_resp->count_enum_blobs >= blob_count) && blob_count) {
			copied = 0;
			blob_id_ptr = (uint32_t *)(unsigned long)out_resp->enum_blob_ptr;
			blob_length_ptr = (uint32_t *)(unsigned long)out_resp->values_ptr;
			
			list_for_each_entry(prop_blob, &property->enum_blob_list, head) {
				if (put_user(prop_blob->id, blob_id_ptr + copied)) {
					ret = -EFAULT;
					goto done;
				}
				
				if (put_user(prop_blob->length, blob_length_ptr + copied)) {
					ret = -EFAULT;
					goto done;
				}
				
				copied++;
			}
		}
		out_resp->count_enum_blobs = enum_count;
	}
done:
	mutex_unlock(&dev->mode_config.mutex);
	return ret;
}

static struct drm_property_blob *drm_property_create_blob(struct drm_device *dev, int length,
							  void *data)
{
	struct drm_property_blob *blob;

	if (!length || !data)
		return NULL;

	blob = kzalloc(sizeof(struct drm_property_blob)+length, GFP_KERNEL);
	if (!blob)
		return NULL;

	blob->data = (void *)((char *)blob + sizeof(struct drm_property_blob));
	blob->length = length;

	memcpy(blob->data, data, length);

	blob->id = drm_idr_get(dev, blob);
	
	list_add_tail(&blob->head, &dev->mode_config.property_blob_list);
	return blob;
}

static void drm_property_destroy_blob(struct drm_device *dev,
			       struct drm_property_blob *blob)
{
	drm_idr_put(dev, blob->id);
	list_del(&blob->head);
	kfree(blob);
}

int drm_mode_getblob_ioctl(struct drm_device *dev,
			   void *data, struct drm_file *file_priv)
{
	struct drm_mode_get_blob *out_resp = data;
	struct drm_property_blob *blob;
	int ret = 0;
	void *blob_ptr;

	mutex_lock(&dev->mode_config.mutex);
	
	blob = idr_find(&dev->mode_config.crtc_idr, out_resp->blob_id);
	if (!blob || (blob->id != out_resp->blob_id)) {
		ret = -EINVAL;
		goto done;
	}

	if (out_resp->length == blob->length) {
		blob_ptr = (void *)(unsigned long)out_resp->data;
		if (copy_to_user(blob_ptr, blob->data, blob->length)){
			ret = -EFAULT;
			goto done;
		}
	}
	out_resp->length = blob->length;

done:
	mutex_unlock(&dev->mode_config.mutex);
	return ret;
}

int drm_mode_output_update_edid_property(struct drm_output *output, struct edid *edid)
{
	struct drm_device *dev = output->dev;
	int ret = 0;
	if (output->edid_blob_ptr)
		drm_property_destroy_blob(dev, output->edid_blob_ptr);

	output->edid_blob_ptr = drm_property_create_blob(output->dev, 128, edid);
	
	ret = drm_output_property_set_value(output, dev->mode_config.edid_property, output->edid_blob_ptr->id);
	return ret;
}
EXPORT_SYMBOL(drm_mode_output_update_edid_property);

int drm_mode_output_property_set_ioctl(struct drm_device *dev,
				       void *data, struct drm_file *file_priv)
{
	struct drm_mode_output_set_property *out_resp = data;
	struct drm_property *property;
	struct drm_output *output;
	int ret = -EINVAL;
	int i;

	mutex_lock(&dev->mode_config.mutex);
	output = idr_find(&dev->mode_config.crtc_idr, out_resp->output_id);
	if (!output || (output->id != out_resp->output_id)) {
		goto out;
	}

	for (i = 0; i < DRM_OUTPUT_MAX_PROPERTY; i++) {
		if (output->property_ids[i] == out_resp->prop_id)
			break;
	}

	if (i == DRM_OUTPUT_MAX_PROPERTY) {
		goto out;
	}
	
	property = idr_find(&dev->mode_config.crtc_idr, out_resp->prop_id);
	if (!property || (property->id != out_resp->prop_id)) {
		goto out;
	}

	if (property->flags & DRM_MODE_PROP_IMMUTABLE)
		goto out;

	if (property->flags & DRM_MODE_PROP_RANGE) {
		if (out_resp->value < property->values[0])
			goto out;

		if (out_resp->value > property->values[1])
			goto out;
	} else {
		int found = 0;
		for (i = 0; i < property->num_values; i++) {
			if (property->values[i] == out_resp->value) {
				found = 1;
				break;
			}
		}
		if (!found) {
			goto out;
		}
	}

	if (output->funcs->set_property)
		ret = output->funcs->set_property(output, property, out_resp->value);

out:
	mutex_unlock(&dev->mode_config.mutex);
	return ret;
}


int drm_mode_replacefb(struct drm_device *dev,
		       void *data, struct drm_file *file_priv)
{
	struct drm_mode_fb_cmd *r = data;
	struct drm_framebuffer *fb;
	struct drm_crtc *crtc;
	struct drm_buffer_object *bo;
	int found = 0;
	struct drm_framebuffer *fbl = NULL;
	int ret = 0;
	/* right replace the current bo attached to this fb with a new bo */
	mutex_lock(&dev->mode_config.mutex);
	ret = drm_get_buffer_object(dev, &bo, r->handle);
	if (ret || !bo) {
		ret = -EINVAL;
		goto out;
	}

	fb = idr_find(&dev->mode_config.crtc_idr, r->buffer_id);
	if (!fb || (r->buffer_id != fb->id)) {
		ret = -EINVAL;
		goto out;
	}

	list_for_each_entry(fbl, &file_priv->fbs, filp_head)
		if (fb == fbl)
			found = 1;

	if (!found) {
		DRM_ERROR("tried to replace an fb we didn't own\n");
		ret = -EINVAL;
		goto out;
	}

	if (fb->bo->type == drm_bo_type_kernel)
		DRM_ERROR("the bo should not be a kernel bo\n");

	fb->width = r->width;
	fb->height = r->height;
	fb->pitch = r->pitch;
	fb->bits_per_pixel = r->bpp;
	fb->depth = r->depth;
	fb->bo = bo;

	/* find all crtcs connected to this fb */
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		if (crtc->fb->id == r->buffer_id) {
			crtc->funcs->mode_set_base(crtc, crtc->x, crtc->y);
		}
	}
out:
	mutex_unlock(&dev->mode_config.mutex);
	return ret;

}