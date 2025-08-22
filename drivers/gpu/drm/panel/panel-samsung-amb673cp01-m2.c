// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2025 alfaonyt <alex@pillou.net>
// Generated with linux-mdss-dsi-panel-driver-generator from vendor device tree:
//   Copyright (c) 2025, The Linux Foundation. All rights reserved.

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>

#include <drm/display/drm_dsc.h>
#include <drm/display/drm_dsc_helper.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <drm/drm_probe_helper.h>

struct m2_38_0c_0a_dsc {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct drm_dsc_config dsc;
	struct gpio_desc *reset_gpio;

	struct regulator *avdd;
	struct regulator *vci;
	struct regulator *vddio;
	struct regulator *vddd;
};

static inline
struct m2_38_0c_0a_dsc *to_m2_38_0c_0a_dsc(struct drm_panel *panel)
{
	return container_of(panel, struct m2_38_0c_0a_dsc, panel);
}

static void m2_38_0c_0a_dsc_reset(struct m2_38_0c_0a_dsc *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(1000, 2000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(5000, 6000);
}

static int m2_38_0c_0a_dsc_on(struct m2_38_0c_0a_dsc *ctx)
{
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi };

	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x9e,
				     0x12, 0x00, 0x00, 0xab, 0x30, 0x80, 0x0c,
				     0x80, 0x05, 0xa0, 0x00, 0x19, 0x02, 0xd0,
				     0x02, 0xd0, 0x02, 0x00, 0x02, 0x86, 0x00,
				     0x20, 0x02, 0x9e, 0x00, 0x0a, 0x00, 0x0d,
				     0x04, 0x56, 0x03, 0x0d, 0x18, 0x00, 0x10,
				     0xf0, 0x07, 0x10, 0x20, 0x00, 0x06, 0x0f,
				     0x0f, 0x33, 0x0e, 0x1c, 0x2a, 0x38, 0x46,
				     0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7b,
				     0x7d, 0x7e, 0x02, 0x02, 0x22, 0x00, 0x2a,
				     0x40, 0x2a, 0xbe, 0x3a, 0xfc, 0x3a, 0xfa,
				     0x3a, 0xf8, 0x3b, 0x38, 0x3b, 0x78, 0x3b,
				     0xb6, 0x4b, 0xb6, 0x4b, 0xf4, 0x4b, 0xf4,
				     0x6c, 0x34, 0x84, 0x74, 0x00, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				     0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x9d, 0x01);
	mipi_dsi_dcs_exit_sleep_mode_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 120);
	mipi_dsi_dcs_set_tear_on_multi(&dsi_ctx, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
	mipi_dsi_dcs_set_column_address_multi(&dsi_ctx, 0x0000, 0x059f);
	mipi_dsi_dcs_set_page_address_multi(&dsi_ctx, 0x0000, 0x0c7f);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0x5a, 0x5a);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc3, 0x0c);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0xa5, 0xa5);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0x5a, 0x5a);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb9, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb0, 0x00, 0x06, 0xb9);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb9,
				     0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf7, 0x0f);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0xa5, 0xa5);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0x5a, 0x5a);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb0, 0x00, 0x08, 0xcb);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcb, 0x12);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xbd, 0x21, 0x82);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb0, 0x00, 0x10, 0xbd);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xbd, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb0, 0x00, 0x21, 0xbd);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xbd,
				     0x03, 0x00, 0x06, 0x00, 0x09, 0x00, 0x0c,
				     0x00, 0x01, 0x00, 0x15, 0x00, 0x21, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb0, 0x00, 0x61, 0xbd);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xbd,
				     0x04, 0x00, 0x08, 0x00, 0x0c, 0x00, 0x10,
				     0x00, 0x74);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb0, 0x00, 0x12, 0xbd);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xbd, 0x01);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb0, 0x00, 0x16, 0xbd);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xbd, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xbd, 0x21);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x60, 0x00); /* 0x00:120Hz, 0x01:60Hz, 0x03:30Hz, 0x07:10Hz */
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf7, 0x0f);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0xa5, 0xa5);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0x5a, 0x5a);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb0, 0x00, 0x0d, 0x63);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x63, 0x08);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb0, 0x00, 0x0c, 0x63);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x63, 0x20);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0xa5, 0xa5);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, MIPI_DCS_WRITE_CONTROL_DISPLAY,
				     0x20);
	mipi_dsi_dcs_set_display_brightness_multi(&dsi_ctx, 0x0000);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf7, 0x0f);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0xa5, 0xa5);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0x5a, 0x5a);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb0, 0x00, 0x47, 0xf4);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf4, 0x01);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xed, 0x44, 0x05, 0x20);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0xa5, 0xa5);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0x5a, 0x5a);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf1, 0x5a, 0x5a);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb0, 0x00, 0x22, 0xb9);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb9, 0xb1, 0xa1);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb0, 0x00, 0x05, 0xf2);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf2, 0x52);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf7, 0x0f);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf1, 0xa5, 0xa5);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0xa5, 0xa5);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0x5a, 0x5a);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc3, 0x0c);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0xa5, 0xa5);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0x5a, 0x5a);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb0, 0x02, 0x67, 0x1d);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x1d, 0x81);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0xa5, 0xa5);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0x5a, 0x5a);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb0, 0x00, 0x52, 0x1f);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x1f, 0x02);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb0, 0x00, 0x53, 0x1f);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x1f,
				     0x07, 0x01, 0x00, 0x02, 0xb9, 0x02, 0xba,
				     0x07, 0xa2, 0x07, 0xaa, 0x12, 0xc0, 0x12,
				     0xc1, 0x1a, 0xba, 0x1b, 0xc0, 0x1f, 0xe0,
				     0x00, 0x53, 0x00, 0xc3, 0x03, 0x43, 0x07,
				     0xa0, 0x07, 0xa1, 0x07, 0xa2, 0x07, 0xa6,
				     0x07, 0xa8, 0x07, 0xaa, 0x07, 0xac, 0x07,
				     0xae, 0x07, 0xb0, 0x07, 0xb2, 0x07, 0xb4,
				     0x07, 0xb6, 0x00, 0x03, 0x00, 0xa4, 0x00,
				     0xc0, 0x00, 0xd1, 0x01, 0x86, 0x03, 0xff,
				     0x07, 0xff, 0x30, 0x30, 0x35, 0x40, 0x35,
				     0x30, 0x40, 0x40, 0x35, 0x80, 0x80, 0x80,
				     0x0a, 0x0f, 0x14, 0x18, 0x1e, 0x3a, 0x78,
				     0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				     0x80, 0x01, 0xb1);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb0, 0x00, 0x78, 0xbd);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xbd, 0x10);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf7, 0x0f);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0xa5, 0xa5);
	mipi_dsi_dcs_set_display_on_multi(&dsi_ctx);

	return dsi_ctx.accum_err;
}

static int m2_38_0c_0a_dsc_off(struct m2_38_0c_0a_dsc *ctx)
{
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi };

	mipi_dsi_dcs_set_display_off_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 20);
	mipi_dsi_dcs_enter_sleep_mode_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 100);

	return dsi_ctx.accum_err;
}

static int m2_38_0c_0a_dsc_prepare(struct drm_panel *panel)
{
	struct m2_38_0c_0a_dsc *ctx = to_m2_38_0c_0a_dsc(panel);
	struct device *dev = &ctx->dsi->dev;
	struct drm_dsc_picture_parameter_set pps;
	int ret;

	ret = regulator_enable(ctx->avdd);
	if (ret) {
		dev_err(dev, "failed to enable avdd regulator: %d\n", ret);
		return ret;
	}
	
	ret = regulator_enable(ctx->vci);
	if (ret) {
		dev_err(dev, "failed to enable vci regulator: %d\n", ret);
		return ret;
	}

	ret = regulator_enable(ctx->vddio);
	if (ret) {
		dev_err(dev, "failed to enable vddio regulator: %d\n", ret);
		return ret;
	}
	
	ret = regulator_enable(ctx->vddd);
	if (ret) {
		dev_err(dev, "failed to enable vddd regulator: %d\n", ret);
		return ret;
	}

	m2_38_0c_0a_dsc_reset(ctx);

	ret = m2_38_0c_0a_dsc_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		return ret;
	}

	drm_dsc_pps_payload_pack(&pps, &ctx->dsc);

	ret = mipi_dsi_picture_parameter_set(ctx->dsi, &pps);
	if (ret < 0) {
		dev_err(panel->dev, "failed to transmit PPS: %d\n", ret);
		return ret;
	}

	ret = mipi_dsi_compression_mode(ctx->dsi, true);
	if (ret < 0) {
		dev_err(dev, "failed to enable compression mode: %d\n", ret);
		return ret;
	}

	msleep(28); /* TODO: Is this panel-dependent? */

	return 0;
}

static int m2_38_0c_0a_dsc_unprepare(struct drm_panel *panel)
{
	struct m2_38_0c_0a_dsc *ctx = to_m2_38_0c_0a_dsc(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	ret = m2_38_0c_0a_dsc_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);

	return 0;
}

static const struct drm_display_mode m2_38_0c_0a_dsc_mode = {
	/* 120Hz */
	.clock = (1440 + 32 + 16 + 32) * (3200 + 24 + 8 + 24) * 120 / 1000,
	.hdisplay = 1440,
	.hsync_start = 1440 + 32,
	.hsync_end = 1440 + 32 + 16,
	.htotal = 1440 + 32 + 16 + 32,
	.vdisplay = 3200,
	.vsync_start = 3200 + 24,
	.vsync_end = 3200 + 24 + 8,
	.vtotal = 3200 + 24 + 8 + 24,
	.width_mm = 70,
	.height_mm = 156,
	.type = DRM_MODE_TYPE_DRIVER,
};

static int m2_38_0c_0a_dsc_get_modes(struct drm_panel *panel,
				     struct drm_connector *connector)
{
	return drm_connector_helper_get_modes_fixed(connector, &m2_38_0c_0a_dsc_mode);
}

static const struct drm_panel_funcs m2_38_0c_0a_dsc_panel_funcs = {
	.prepare = m2_38_0c_0a_dsc_prepare,
	.unprepare = m2_38_0c_0a_dsc_unprepare,
	.get_modes = m2_38_0c_0a_dsc_get_modes,
};

static int m2_38_0c_0a_dsc_bl_update_status(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	u16 brightness = backlight_get_brightness(bl);
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_brightness_large(dsi, brightness);
	if (ret < 0)
		return ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	return 0;
}

// TODO: Check if /sys/class/backlight/.../actual_brightness actually returns
// correct values. If not, remove this function.
static int m2_38_0c_0a_dsc_bl_get_brightness(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	u16 brightness;
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_get_display_brightness_large(dsi, &brightness);
	if (ret < 0)
		return ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	return brightness;
}

static const struct backlight_ops m2_38_0c_0a_dsc_bl_ops = {
	.update_status = m2_38_0c_0a_dsc_bl_update_status,
	.get_brightness = m2_38_0c_0a_dsc_bl_get_brightness,
};

static struct backlight_device *
m2_38_0c_0a_dsc_create_backlight(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	const struct backlight_properties props = {
		.type = BACKLIGHT_RAW,
		.brightness = 1433,
		.max_brightness = 4095,
	};

	return devm_backlight_device_register(dev, dev_name(dev), dev, dsi,
					      &m2_38_0c_0a_dsc_bl_ops, &props);
}

static int m2_38_0c_0a_dsc_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct m2_38_0c_0a_dsc *ctx;
	int ret;

	/*
	ctx = devm_drm_panel_alloc(dev, struct m2_38_0c_0a_dsc, panel,
				   &m2_38_0c_0a_dsc_panel_funcs,
				   DRM_MODE_CONNECTOR_DSI);
	if (IS_ERR(ctx))
		return PTR_ERR(ctx);
	*/

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->avdd = devm_regulator_get(dev, "avdd");
	if (IS_ERR(ctx->avdd))
		return dev_err_probe(dev, PTR_ERR(ctx->avdd), "failed to get avdd regulator\n");
	
	ctx->vci = devm_regulator_get(dev, "vci");
	if (IS_ERR(ctx->vci))
		return dev_err_probe(dev, PTR_ERR(ctx->vci), "failed to get vci regulator\n");

	ctx->vddio = devm_regulator_get(dev, "vddio");
	if (IS_ERR(ctx->vddio))
		return dev_err_probe(dev, PTR_ERR(ctx->vddio), "failed to get vddio regulator\n");
	
	ctx->vddd = devm_regulator_get(dev, "vddd");
	if (IS_ERR(ctx->vddd))
		return dev_err_probe(dev, PTR_ERR(ctx->vddd), "failed to get vddd regulator\n");

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio),
				     "Failed to get reset-gpios\n");

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB101010;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS | MIPI_DSI_MODE_LPM;

	drm_panel_init(&ctx->panel, dev, &m2_38_0c_0a_dsc_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);
	ctx->panel.prepare_prev_first = true;

	ctx->panel.prepare_prev_first = true;

	ctx->panel.backlight = m2_38_0c_0a_dsc_create_backlight(dsi);
	if (IS_ERR(ctx->panel.backlight))
		return dev_err_probe(dev, PTR_ERR(ctx->panel.backlight),
				     "Failed to create backlight\n");

	drm_panel_add(&ctx->panel);

	/* This panel only supports DSC; unconditionally enable it */
	dsi->dsc = &ctx->dsc;

	ctx->dsc.dsc_version_major = 1;
	ctx->dsc.dsc_version_minor = 2;

	/* TODO: Pass slice_per_pkt = 2 */
	ctx->dsc.slice_height = 25;
	ctx->dsc.slice_width = 720;
	/*
	 * TODO: hdisplay should be read from the selected mode once
	 * it is passed back to drm_panel (in prepare?)
	 */
	WARN_ON(1440 % ctx->dsc.slice_width);
	ctx->dsc.slice_count = 1440 / ctx->dsc.slice_width;
	ctx->dsc.bits_per_component = 10;
	ctx->dsc.bits_per_pixel = 8 << 4; /* 4 fractional bits */
	ctx->dsc.block_pred_enable = true;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		drm_panel_remove(&ctx->panel);
		return dev_err_probe(dev, ret, "Failed to attach to DSI host\n");
	}

	return 0;
}

static void m2_38_0c_0a_dsc_remove(struct mipi_dsi_device *dsi)
{
	struct m2_38_0c_0a_dsc *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id m2_38_0c_0a_dsc_of_match[] = {
	{ .compatible = "samsung,amb673cp01-m2" }, //Xiaomi 13 Pro
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, m2_38_0c_0a_dsc_of_match);

static struct mipi_dsi_driver m2_38_0c_0a_dsc_driver = {
	.probe = m2_38_0c_0a_dsc_probe,
	.remove = m2_38_0c_0a_dsc_remove,
	.driver = {
		.name = "panel-m2-38-0c-0a-dsc",
		.of_match_table = m2_38_0c_0a_dsc_of_match,
	},
};
module_mipi_dsi_driver(m2_38_0c_0a_dsc_driver);

MODULE_AUTHOR("alfaonyt <alex@pillou.net>");
MODULE_DESCRIPTION("DRM driver for Xiaomi M2 variant of Samsung AMB673CP01 panel");
MODULE_LICENSE("GPL");
