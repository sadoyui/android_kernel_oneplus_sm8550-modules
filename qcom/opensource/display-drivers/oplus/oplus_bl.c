/***************************************************************
** Copyright (C), 2022, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_bl.c
** Description : oplus display backlight
** Version : 2.0
** Date : 2021/02/22
** Author : Display
******************************************************************/

#include "oplus_bl.h"
#include "oplus_display_interface.h"
#include "oplus_display_panel_common.h"
#if defined(CONFIG_PXLW_IRIS)
#include "dsi_iris_api.h"
#endif

char oplus_global_hbm_flags = 0x0;
static int enable_hbm_enter_dly_on_flags = 0;
static int enable_hbm_exit_dly_on_flags = 0;
extern u32 oplus_last_backlight;

int oplus_panel_parse_bl_config(struct dsi_panel *panel)
{
	int rc = 0;
	u32 val = 0;
	struct dsi_parser_utils *utils = &panel->utils;

#if defined(CONFIG_PXLW_IRIS)
	if (iris_is_chip_supported() && (!strcmp(panel->type, "secondary"))) {
		LCD_INFO("iris secondary panel no need config\n");
		return 0;
	}
#endif

	rc = utils->read_u32(utils->data, "oplus,dsi-bl-normal-max-level", &val);
	if (rc) {
		LCD_INFO("[%s] oplus,dsi-bl-normal-max-level undefined, default to bl max\n",
				panel->oplus_priv.vendor_name);
		panel->bl_config.bl_normal_max_level = panel->bl_config.bl_max_level;
	} else {
		panel->bl_config.bl_normal_max_level = val;
	}
	LCD_INFO("[%s] bl_max_level=%d\n", panel->oplus_priv.vendor_name,
			panel->bl_config.bl_max_level);

	rc = utils->read_u32(utils->data, "oplus,dsi-brightness-normal-max-level",
		&val);
	if (rc) {
		LCD_INFO("[%s] oplus,dsi-brightness-normal-max-level undefined, default to brightness max\n",
				panel->oplus_priv.vendor_name);
		panel->bl_config.brightness_normal_max_level = panel->bl_config.brightness_max_level;
	} else {
		panel->bl_config.brightness_normal_max_level = val;
	}
	LCD_INFO("[%s] brightness_normal_max_level=%d\n",
			panel->oplus_priv.vendor_name,
			panel->bl_config.brightness_normal_max_level);

	rc = utils->read_u32(utils->data, "oplus,dsi-brightness-default-level", &val);
	if (rc) {
		LCD_INFO("[%s] oplus,dsi-brightness-default-level undefined, default to brightness normal max\n",
				panel->oplus_priv.vendor_name);
		panel->bl_config.brightness_default_level = panel->bl_config.brightness_normal_max_level;
	} else {
		panel->bl_config.brightness_default_level = val;
	}
	LCD_INFO("[%s] brightness_default_level=%d\n",
			panel->oplus_priv.vendor_name,
			panel->bl_config.brightness_default_level);

	rc = utils->read_u32(utils->data, "oplus,dsi-dc-backlight-threshold", &val);
	if (rc) {
		LCD_INFO("[%s] oplus,dsi-dc-backlight-threshold undefined, default to 260\n",
				panel->oplus_priv.vendor_name);
		panel->bl_config.dc_backlight_threshold = 260;
		panel->bl_config.oplus_dc_mode = false;
	} else {
		panel->bl_config.dc_backlight_threshold = val;
		panel->bl_config.oplus_dc_mode = true;
	}
	LCD_INFO("[%s] dc_backlight_threshold=%d, oplus_dc_mode=%d\n",
			panel->oplus_priv.vendor_name,
			panel->bl_config.dc_backlight_threshold,
			panel->bl_config.oplus_dc_mode);

	rc = utils->read_u32(utils->data, "oplus,dsi-global-hbm-case-id", &val);
	if (rc) {
		LCD_INFO("[%s] oplus,dsi-global-hbm-case-id undefined, default to 0\n",
				panel->oplus_priv.vendor_name);
		val = GLOBAL_HBM_CASE_NONE;
	} else if (val >= GLOBAL_HBM_CASE_MAX) {
		LCD_ERR("[%s] oplus,dsi-global-hbm-case-id is invalid:%d\n",
				panel->oplus_priv.vendor_name, val);
		val = GLOBAL_HBM_CASE_NONE;
	}
	panel->bl_config.global_hbm_case_id = val;
	LCD_INFO("[%s] global_hbm_case_id=%d\n",
			panel->oplus_priv.vendor_name,
			panel->bl_config.global_hbm_case_id);

	rc = utils->read_u32(utils->data, "oplus,dsi-global-hbm-threshold", &val);
	if (rc) {
		LCD_INFO("[%s] oplus,dsi-global-hbm-threshold undefined, default to brightness normal max + 1\n",
				panel->oplus_priv.vendor_name);
		panel->bl_config.global_hbm_threshold = panel->bl_config.brightness_normal_max_level + 1;
	} else {
		panel->bl_config.global_hbm_threshold = val;
	}
	LCD_INFO("[%s] global_hbm_threshold=%d\n",
			panel->oplus_priv.vendor_name,
			panel->bl_config.global_hbm_threshold);

	panel->bl_config.global_hbm_scale_mapping = utils->read_bool(utils->data,
			"oplus,dsi-global-hbm-scale-mapping");
	LCD_INFO("oplus,dsi-global-hbm-scale-mapping: %s\n",
			panel->bl_config.global_hbm_scale_mapping ? "true" : "false");

	rc = utils->read_u32(utils->data, "oplus,pwm-switch-backlight-threshold", &val);
	if (rc) {
		panel->oplus_priv.pwm_switch_support = false;
	} else {
		panel->bl_config.pwm_bl_threshold = val;
	}
	panel->pwm_power_on = false;
	panel->pwm_hbm_state = false;
	LCD_INFO("[%s] oplus,pwm-switch-backlight-threshold=%d\n",
			panel->oplus_priv.vendor_name,
			panel->bl_config.pwm_bl_threshold);

	return 0;
}

static int oplus_display_panel_dly(struct dsi_panel *panel, bool hbm_switch)
{
	if (hbm_switch) {
		if (enable_hbm_enter_dly_on_flags)
			enable_hbm_enter_dly_on_flags++;
		if (0 == oplus_global_hbm_flags) {
			if (dsi_panel_tx_cmd_set(panel, DSI_CMD_DLY_ON)) {
				return 0;
			}
			enable_hbm_enter_dly_on_flags = 1;
		} else if (4 == enable_hbm_enter_dly_on_flags) {
			if (dsi_panel_tx_cmd_set(panel, DSI_CMD_DLY_OFF)) {
				return 0;
			}
			enable_hbm_enter_dly_on_flags = 0;
		}
	} else {
		if (oplus_global_hbm_flags == 1) {
			if (dsi_panel_tx_cmd_set(panel, DSI_CMD_DLY_ON)) {
				return 0;
			}
			enable_hbm_exit_dly_on_flags = 1;
		} else {
			if (enable_hbm_exit_dly_on_flags)
				enable_hbm_exit_dly_on_flags++;
			if (3 == enable_hbm_exit_dly_on_flags) {
				enable_hbm_exit_dly_on_flags = 0;
				if (dsi_panel_tx_cmd_set(panel, DSI_CMD_DLY_OFF)) {
					return 0;
				}
			}
		}
	}
	return 0;
}

int oplus_panel_global_hbm_mapping(struct dsi_panel *panel, u32 *backlight_level)
{
	int rc = 0;
	u32 bl_lvl = *backlight_level;
	u32 global_hbm_switch_cmd = 0;
	bool global_hbm_dly = false;
	struct dsi_cmd_desc *cmds;
	size_t tx_len;
	u8 *tx_buf;
	u32 count;
	int i;

	if (bl_lvl > panel->bl_config.bl_normal_max_level) {
		if (!oplus_global_hbm_flags) {
			global_hbm_switch_cmd = DSI_CMD_HBM_ENTER_SWITCH;
		}
	} else if (oplus_global_hbm_flags) {
		global_hbm_switch_cmd = DSI_CMD_HBM_EXIT_SWITCH;
	}

	switch (panel->bl_config.global_hbm_case_id) {
	case GLOBAL_HBM_CASE_1:
		break;
	case GLOBAL_HBM_CASE_2:
		if (bl_lvl > panel->bl_config.bl_normal_max_level) {
			if (panel->bl_config.global_hbm_scale_mapping) {
				bl_lvl = (bl_lvl - panel->bl_config.bl_normal_max_level) * 100000
						/ (panel->bl_config.bl_max_level - panel->bl_config.bl_normal_max_level)
						* (panel->bl_config.bl_max_level - panel->bl_config.global_hbm_threshold)
						/ 100000 + panel->bl_config.global_hbm_threshold;
			} else if (bl_lvl < panel->bl_config.global_hbm_threshold) {
				bl_lvl = panel->bl_config.global_hbm_threshold;
			}
		}
		break;
	case GLOBAL_HBM_CASE_3:
		if (bl_lvl > panel->bl_config.bl_normal_max_level) {
			bl_lvl = bl_lvl + panel->bl_config.global_hbm_threshold
					- panel->bl_config.bl_normal_max_level - 1;
		}
		break;
	case GLOBAL_HBM_CASE_4:
		global_hbm_switch_cmd = 0;
		if (bl_lvl <= PANEL_MAX_NOMAL_BRIGHTNESS) {
			if (oplus_global_hbm_flags) {
				global_hbm_switch_cmd = DSI_CMD_HBM_EXIT_SWITCH;
			}
			bl_lvl = backlight_buf[bl_lvl];
		} else if (bl_lvl > HBM_BASE_600NIT) {
			if (!oplus_global_hbm_flags) {
				global_hbm_switch_cmd = DSI_CMD_HBM_ENTER_SWITCH;
			}
			global_hbm_dly = true;
			bl_lvl = backlight_600_800nit_buf[bl_lvl - HBM_BASE_600NIT];
		} else if (bl_lvl > PANEL_MAX_NOMAL_BRIGHTNESS) {
			if (oplus_global_hbm_flags) {
				global_hbm_switch_cmd = DSI_CMD_HBM_EXIT_SWITCH;
			}
			bl_lvl = backlight_500_600nit_buf[bl_lvl - PANEL_MAX_NOMAL_BRIGHTNESS];
		}
		break;
	default:
		global_hbm_switch_cmd = 0;
		break;
	}

	bl_lvl = bl_lvl < panel->bl_config.bl_max_level ? bl_lvl :
			panel->bl_config.bl_max_level;

	if (global_hbm_switch_cmd > 0) {
		/* Update the 0x51 value when sending hbm enter/exit command */
		cmds = panel->cur_mode->priv_info->cmd_sets[global_hbm_switch_cmd].cmds;
		count = panel->cur_mode->priv_info->cmd_sets[global_hbm_switch_cmd].count;
		for (i = 0; i < count; i++) {
			tx_len = cmds[i].msg.tx_len;
			tx_buf = (u8 *)cmds[i].msg.tx_buf;
			if ((3 == tx_len) && (0x51 == tx_buf[0])) {
				tx_buf[1] = (bl_lvl >> 8) & 0xFF;
				tx_buf[2] = bl_lvl & 0xFF;
				break;
			}
		}

		if (global_hbm_dly) {
			oplus_display_panel_dly(panel, true);
		}

		rc = dsi_panel_tx_cmd_set(panel, global_hbm_switch_cmd);
		oplus_global_hbm_flags = (global_hbm_switch_cmd == DSI_CMD_HBM_ENTER_SWITCH);
	}

	*backlight_level = bl_lvl;
	return 0;
}

int oplus_display_panel_get_global_hbm_status(void)
{
	return oplus_global_hbm_flags;
}

void oplus_display_panel_set_global_hbm_status(int global_hbm_status)
{
	oplus_global_hbm_flags = global_hbm_status;
	LCD_INFO("set oplus_global_hbm_flags = %d\n", global_hbm_status);
}

int oplus_hbm_pwm_state(struct dsi_panel *panel, bool hbm_state)
{
	if (!panel) {
		LCD_ERR("Invalid panel params\n");
		return -EINVAL;
	}

	if (panel->oplus_priv.pwm_switch_support && hbm_state) {
		oplus_panel_event_data_notifier_trigger(panel, DRM_PANEL_EVENT_PWM_TURBO, !hbm_state, true);
	}

	if (panel->oplus_priv.pwm_switch_support) {
		panel->pwm_hbm_state = hbm_state;

		if (!hbm_state) {
			panel->pwm_power_on = true;
		}
	}
	LCD_INFO("set oplus pwm_hbm_state = %d\n", hbm_state);
	return 0;
}

int oplus_panel_pwm_switch_backlight(struct dsi_panel *panel, u32 bl_lvl)
{
	int rc = 0;
	u32 pwm_switch_state_last = panel->oplus_pwm_switch_state;
	u32 pwm_switch_cmd = 0;
	u32 disable_compensation_cmd = 0;
	int pulse = 0;

	if (!panel->oplus_priv.pwm_switch_support)
		return rc;

	if (panel->pwm_hbm_state) {
		LCD_INFO("panel pwm_hbm_state true disable pwm switch!\n");
		return rc;
	}

	if (bl_lvl > panel->bl_config.pwm_bl_threshold) {
		panel->oplus_pwm_switch_state = PWM_SWITCH_HIGH_STATE;
		pwm_switch_cmd = DSI_CMD_PWM_SWITCH_HIGH;
		if (panel->pwm_power_on || oplus_last_backlight == 0)
			pwm_switch_cmd = DSI_CMD_TIMMING_PWM_SWITCH_HIGH;
		pulse = 0;
	} else {
		panel->oplus_pwm_switch_state = PWM_SWITCH_LOW_STATE;
		pwm_switch_cmd = DSI_CMD_PWM_SWITCH_LOW;
		if (panel->pwm_power_on || oplus_last_backlight == 0)
			pwm_switch_cmd = DSI_CMD_TIMMING_PWM_SWITCH_LOW;
		pulse = 1;
	}
	if (!strcmp(panel->name, "AC052 P 3 A0003 dsc cmd mode panel")
		|| !strcmp(panel->name, "AC052 S 3 A0001 dsc cmd mode panel")
		|| !strcmp(panel->name, "AA536 P 3 A0001 dsc cmd mode panel")) {
		disable_compensation_cmd = DSI_CMD_DISABLE_PWM_BACKLIGHT_COMPENSATION;
		oplus_panel_pwm_switch_wait_te_tx_cmd(panel, pwm_switch_cmd, disable_compensation_cmd, pwm_switch_state_last);
	} else {
		if (pwm_switch_state_last != panel->oplus_pwm_switch_state ||
			oplus_last_backlight == 0 || panel->pwm_power_on) {
			panel->pwm_power_on = false;
			rc = dsi_panel_tx_cmd_set(panel, pwm_switch_cmd);
		}
	}
	oplus_panel_event_data_notifier_trigger(panel, DRM_PANEL_EVENT_PWM_TURBO, pulse, true);

	return 0;
}

int oplus_panel_pwm_switch_timing_switch(struct dsi_panel *panel)
{
	int rc = 0;
	u32 pwm_switch_cmd = DSI_CMD_TIMMING_PWM_SWITCH_LOW;

	if (!panel->oplus_priv.pwm_switch_support)
		return rc;

	if (panel->pwm_hbm_state) {
		LCD_INFO("panel pwm_hbm_state true disable pwm switch!\n");
		return rc;
	}


	if (panel->oplus_pwm_switch_state  == PWM_SWITCH_HIGH_STATE) {
		pwm_switch_cmd = DSI_CMD_TIMMING_PWM_SWITCH_HIGH;
	}

	rc = dsi_panel_tx_cmd_set(panel, pwm_switch_cmd);

	return rc;
}
int oplus_panel_pwm_switch_wait_te_tx_cmd(struct dsi_panel *panel, u32 pwm_switch_cmd, u32 disable_compensation_cmd, u32 pwm_switch_state_last)
{
	int rc = 0;
	unsigned int refresh_rate = panel->cur_mode->timing.refresh_rate;

	if (panel->pwm_power_on == true || oplus_last_backlight == 0) {
		rc = dsi_panel_tx_cmd_set(panel, pwm_switch_cmd);
		panel->pwm_power_on = false;
		return rc;
	}
	if (pwm_switch_state_last != panel->oplus_pwm_switch_state ||
		oplus_last_backlight == 0) {
		oplus_sde_early_wakeup(panel);
		oplus_wait_for_vsync(panel);
		if (refresh_rate == 60) {
			oplus_need_to_sync_te(panel);
		}
		rc = dsi_panel_tx_cmd_set(panel, pwm_switch_cmd);

		if (!strcmp(panel->name, "AC052 P 3 A0003 dsc cmd mode panel")
			|| !strcmp(panel->name, "AC052 S 3 A0001 dsc cmd mode panel")) {
			oplus_wait_for_vsync(panel);
			if (refresh_rate == 60) {
				oplus_need_to_sync_te(panel);
			}
			rc = dsi_panel_tx_cmd_set(panel, disable_compensation_cmd);
		}
	}
	return rc;
}

