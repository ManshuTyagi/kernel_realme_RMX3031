/***************************************************************
** Copyright (C),  2018,  OPPO Mobile Comm Corp.,  Ltd
** VENDOR_EDIT
** File : oppo_display_private_api.h
** Description : oppo display private api implement
** Version : 1.0
** Date : 2018/03/20
** Author : Jie.Hu@PSW.MM.Display.Stability
**
** ------------------------------- Revision History: -----------
**  <author>        <data>        <version >        <desc>
**   Hu.Jie          2018/03/20        1.0           Build this moudle
**   Guo.Ling        2018/10/11        1.1           Modify for SDM660
**   Guo.Ling        2018/11/27        1.2           Modify for mt6779
**   Li.Ping         2019/11/18        1.3           Modify for mt6885
******************************************************************/
#include "oppo_display_private_api.h"
#include "mtk_disp_aal.h"
#include "oplus_display_panel_power.h"
/*
 * we will create a sysfs which called /sys/kernel/oppo_display,
 * In that directory, oppo display private api can be called
 */

#define PANEL_SERIAL_NUM_REG 0xA1
#define PANEL_REG_READ_LEN   10
static uint64_t serial_number = 0x0;

typedef struct panel_serial_info
{
	int reg_index;
	uint64_t year;
	uint64_t month;
	uint64_t day;
	uint64_t hour;
	uint64_t minute;
	uint64_t second;
	uint64_t reserved[2];
} PANEL_SERIAL_INFO;

extern struct drm_device* get_drm_device(void);
extern int mtk_drm_setbacklight(struct drm_crtc *crtc, unsigned int level);
extern int oplus_mtk_drm_sethbm(struct drm_crtc *crtc, unsigned int hbm_mode);
extern PANEL_VOLTAGE_BAK panel_vol_bak[PANEL_VOLTAGE_ID_MAX];
extern struct drm_panel *p_node;

unsigned long esd_mode = 0;
EXPORT_SYMBOL(esd_mode);
unsigned long oppo_display_brightness = 0;
unsigned long oplus_max_normal_brightness = 0;
unsigned long seed_mode = 0;
bool oppo_fp_notify_down_delay = false;
bool oppo_fp_notify_up_delay = false;
extern void fingerprint_send_notify(unsigned int fingerprint_op_mode);
extern int oplus_mtk_drm_setcabc(struct drm_crtc *crtc, unsigned int hbm_mode);

bool oplus_mtk_drm_get_hbm_state(void)
{
	bool hbm_en = false;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct drm_device *ddev = get_drm_device();
	struct mtk_dsi *dsi;
	struct mtk_ddp_comp *comp;
	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
		typeof(*crtc), head);
	if (!crtc) {
		DDPPR_ERR("get hbm find crtc fail\n");
		return 0;
	}
	mtk_crtc = to_mtk_crtc(crtc);
	comp = mtk_ddp_comp_request_output(mtk_crtc);

	dsi = container_of(comp, struct mtk_dsi, ddp_comp);
	if (mtk_crtc->panel_ext->funcs->hbm_get_state) {
		mtk_crtc->panel_ext->funcs->hbm_get_state(dsi->panel, &hbm_en);
	}
	return hbm_en;
}

static ssize_t oplus_display_get_brightness(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%ld\n", oppo_display_brightness);
}

static ssize_t oplus_display_set_brightness(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t num)
{
	int ret;
	struct drm_crtc *crtc;
	struct drm_device *ddev = get_drm_device();
	unsigned int oppo_set_brightness = 0;

	ret = kstrtouint(buf, 10, &oppo_set_brightness);

	printk("%s %d\n", __func__, oppo_set_brightness);

	if (oppo_set_brightness > OPPO_MAX_BRIGHTNESS || oppo_set_brightness < OPPO_MIN_BRIGHTNESS) {
		printk(KERN_ERR "%s, brightness:%d out of scope\n", __func__, oppo_set_brightness);
		return num;
	}

	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		printk(KERN_ERR "find crtc fail\n");
		return 0;
	}
	mtk_drm_setbacklight(crtc, oppo_set_brightness);

	return num;
}

static ssize_t oplus_display_get_max_brightness(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", OPPO_MAX_BRIGHTNESS);
}

static ssize_t oplus_display_get_maxbrightness(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", oplus_max_normal_brightness);
}

unsigned int hbm_mode = 0;
static ssize_t oplus_display_get_hbm(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%ld\n", hbm_mode);
}

static ssize_t oplus_display_set_hbm(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t num)
{
	int ret;
	struct drm_crtc *crtc;
	struct drm_device *ddev = get_drm_device();
	unsigned int tmp = 0;

	ret = kstrtouint(buf, 10, &tmp);

	printk("%s, %d to be %d\n", __func__, hbm_mode, tmp);

	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		printk(KERN_ERR "find crtc fail\n");
		return 0;
	}

	oplus_mtk_drm_sethbm(crtc, tmp);
	hbm_mode = tmp;

	if (tmp == 1)
		usleep_range(30000, 31000);
	return num;
}

static ssize_t oplus_display_set_panel_pwr(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf, size_t num)
{
	u32 panel_vol_value = 0, panel_vol_id = 0;
	int rc = 0;

	sscanf(buf, "%d %d", &panel_vol_id, &panel_vol_value);
	panel_vol_id = panel_vol_id & 0x0F;

	pr_err("debug for %s, buf = [%s], id = %d value = %d, num = %d\n",
		__func__, buf, panel_vol_id, panel_vol_value, num);

	if (panel_vol_id < 0 || panel_vol_id > PANEL_VOLTAGE_ID_MAX) {
		return -EINVAL;
	}

	if (panel_vol_value < panel_vol_bak[panel_vol_id].voltage_min ||
		panel_vol_id > panel_vol_bak[panel_vol_id].voltage_max) {
		return -EINVAL;
	}

	if (panel_vol_id == PANEL_VOLTAGE_ID_VG_BASE) {
		pr_err("%s: set the VGH_L pwr = %d \n", __func__, panel_vol_value);
		rc = oplus_panel_set_vg_base(panel_vol_value);
		if (rc < 0) {
			return rc;
		}

		return num;
	}

	if (p_node && p_node->funcs->oplus_set_power) {
		rc = p_node->funcs->oplus_set_power(panel_vol_id, panel_vol_value);
		if (rc) {
			pr_err("Set voltage(%s) fail, rc=%d\n",
				 __func__, rc);
			return -EINVAL;
		}
	}

	return num;
}

static ssize_t oplus_display_get_panel_pwr(struct device *dev,
	struct device_attribute *attr, char *buf) {
	int ret = 0;
	u32 i = 0;

	for (i = 0; i < (PANEL_VOLTAGE_ID_MAX-1); i++) {
		if (p_node && p_node->funcs->oplus_update_power_value) {
			ret = p_node->funcs->oplus_update_power_value(panel_vol_bak[i].voltage_id);
		}

		if (ret < 0) {
			pr_err("%s : update_current_voltage error = %d\n", __func__, ret);
		}
		else {
			panel_vol_bak[i].voltage_current = ret;
		}
	}

	return sprintf(buf, "%d %d %d %d %d %d %d %d %d %d %d %d\n",
		panel_vol_bak[0].voltage_id, panel_vol_bak[0].voltage_min,
		panel_vol_bak[0].voltage_current, panel_vol_bak[0].voltage_max,
		panel_vol_bak[1].voltage_id, panel_vol_bak[1].voltage_min,
		panel_vol_bak[1].voltage_current, panel_vol_bak[1].voltage_max,
		panel_vol_bak[2].voltage_id, panel_vol_bak[2].voltage_min,
		panel_vol_bak[2].voltage_current, panel_vol_bak[2].voltage_max);
}

static ssize_t fingerprint_notify_trigger(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf, size_t num)
{
	unsigned int fingerprint_op_mode = 0x0;

	if (kstrtouint(buf, 0, &fingerprint_op_mode)) {
		pr_err("%s kstrtouu8 buf error!\n", __func__);
		return num;
	}

	if (fingerprint_op_mode == 1) {
		oppo_fp_notify_down_delay = true;
	} else {
		oppo_fp_notify_up_delay = true;
	}

	printk(KERN_ERR "%s receive uiready %d\n", __func__, fingerprint_op_mode);
	return num;
}


/*
* add for lcd serial
*/
int panel_serial_number_read(char cmd, int num)
{
        char para[20] = {0};
        int count = 10;
        PANEL_SERIAL_INFO panel_serial_info;
        struct mtk_ddp_comp *comp;
        struct drm_crtc *crtc;
        struct mtk_drm_crtc *mtk_crtc;
	struct drm_device *ddev = get_drm_device();

        #ifdef NO_AOD_6873
        return 0;
        #endif

        para[0] = cmd;
        para[1] = num;

        /* this debug cmd only for crtc0 */
        crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
            typeof(*crtc), head);
        if (!crtc) {
        	DDPPR_ERR("find crtc fail\n");
        	return 0;
        }

        mtk_crtc = to_mtk_crtc(crtc);
        comp = mtk_ddp_comp_request_output(mtk_crtc);
        if (!comp->funcs || !comp->funcs->io_cmd) {
        	DDPINFO("cannot find output component\n");
        	return 0;
        }
        while (count > 0) {
                comp->funcs->io_cmd(comp, NULL, DSI_READ, para);
                count--;
                panel_serial_info.reg_index = 4;
                panel_serial_info.month     = para[panel_serial_info.reg_index] & 0x0F;
                panel_serial_info.year      = ((para[panel_serial_info.reg_index + 1] & 0xE0) >> 5) + 7;
                panel_serial_info.day       = para[panel_serial_info.reg_index + 1] & 0x1F;
                panel_serial_info.hour      = para[panel_serial_info.reg_index + 2] & 0x17;
                panel_serial_info.minute    = para[panel_serial_info.reg_index + 3];
                panel_serial_info.second    = para[panel_serial_info.reg_index + 4];
                panel_serial_info.reserved[0] = 0;
                panel_serial_info.reserved[1] = 0;
                serial_number = (panel_serial_info.year     << 56)\
                        + (panel_serial_info.month      << 48)\
                        + (panel_serial_info.day        << 40)\
                        + (panel_serial_info.hour       << 32)\
                        + (panel_serial_info.minute << 24)\
                        + (panel_serial_info.second << 16)\
                        + (panel_serial_info.reserved[0] << 8)\
                        + (panel_serial_info.reserved[1]);
		if (panel_serial_info.year <= 7) {
			continue;
		} else {
			printk("%s year:0x%llx, month:0x%llx, day:0x%llx, hour:0x%llx, minute:0x%llx, second:0x%llx!\n",
                 	__func__,
                 	panel_serial_info.year,
                 	panel_serial_info.month,
                 	panel_serial_info.day,
                 	panel_serial_info.hour,
                 	panel_serial_info.minute,
                 	panel_serial_info.second);
			break;
        	}
        }
	printk("%s Get panel serial number[0x%llx]\n", __func__, serial_number);
	return 1;
}

static ssize_t oplus_get_panel_serial_number(struct device *dev,
                struct device_attribute *attr, char *buf)
{
        int ret = 0;
        ret = panel_serial_number_read(PANEL_SERIAL_NUM_REG, PANEL_REG_READ_LEN);
        if (ret <= 0)
                return scnprintf(buf, PAGE_SIZE, "Get serial number failed: %d\n", ret);
        else
                return scnprintf(buf, PAGE_SIZE, "%llx\n", serial_number);
}

static ssize_t panel_serial_store(struct device *dev,
                struct device_attribute *attr,
                const char *buf, size_t count)
{
        printk("[soso] Lcm read 0xA1 reg = 0x%llx\n", serial_number);
        return count;
}

int oplus_panel_alpha = 0;
int oplus_underbrightness_alpha = 0;
int alpha_save = 0;
struct ba {
	u32 brightness;
	u32 alpha;
};
struct ba brightness_seed_alpha_lut_dc[] = {
	{0, 0xff},
	{1, 0xfc},
	{2, 0xfb},
	{3, 0xfa},
	{4, 0xf9},
	{5, 0xf8},
	{6, 0xf7},
	{8, 0xf6},
	{10, 0xf4},
	{15, 0xf0},
	{20, 0xea},
	{30, 0xe0},
	{45, 0xd0},
	{70, 0xbc},
	{100, 0x98},
	{120, 0x80},
	{140, 0x70},
	{160, 0x58},
	{180, 0x48},
	{200, 0x30},
	{220, 0x20},
	{240, 0x10},
	{260, 0x00},
};
struct ba brightness_alpha_lut[] = {
	{0, 0xff},
	{1, 0xee},
	{2, 0xe8},
	{3, 0xe6},
	{4, 0xe5},
	{6, 0xe4},
	{10, 0xe0},
	{20, 0xd5},
	{30, 0xce},
	{45, 0xc6},
	{70, 0xb7},
	{100, 0xad},
	{150, 0xa0},
	{227, 0x8a},
	{300, 0x80},
	{400, 0x6e},
	{500, 0x5b},
	{600, 0x50},
	{800, 0x38},
	{1023, 0x18},
};

static int interpolate(int x, int xa, int xb, int ya, int yb)
{
	int bf, factor, plus;
	int sub = 0;

	bf = 2 * (yb - ya) * (x - xa) / (xb - xa);
	factor = bf / 2;
	plus = bf % 2;
	if ((xa - xb) && (yb - ya))
		sub = 2 * (x - xa) * (x - xb) / (yb - ya) / (xa - xb);

	return ya + factor + plus + sub;
}

int bl_to_alpha(int brightness)
{
	int level = ARRAY_SIZE(brightness_alpha_lut);
	int i = 0;
	int alpha;

	for (i = 0; i < ARRAY_SIZE(brightness_alpha_lut); i++) {
		if (brightness_alpha_lut[i].brightness >= brightness)
			break;
	}

	if (i == 0)
		alpha = brightness_alpha_lut[0].alpha;
	else if (i == level)
		alpha = brightness_alpha_lut[level - 1].alpha;
	else
		alpha = interpolate(brightness,
			brightness_alpha_lut[i-1].brightness,
			brightness_alpha_lut[i].brightness,
			brightness_alpha_lut[i-1].alpha,
			brightness_alpha_lut[i].alpha);
	return alpha;
}

int brightness_to_alpha(int brightness)
{
	int alpha;

	if (brightness <= 3)
		return alpha_save;

	alpha = bl_to_alpha(brightness);

	alpha_save = alpha;

	return alpha;
}

int oplus_seed_bright_to_alpha(int brightness)
{
	int level = ARRAY_SIZE(brightness_seed_alpha_lut_dc);
	int i = 0;
	int alpha;

	for (i = 0; i < ARRAY_SIZE(brightness_seed_alpha_lut_dc); i++) {
		if (brightness_seed_alpha_lut_dc[i].brightness >= brightness)
			break;
	}

	if (i == 0)
		alpha = brightness_seed_alpha_lut_dc[0].alpha;
	else if (i == level)
		alpha = brightness_seed_alpha_lut_dc[level - 1].alpha;
	else
		alpha = interpolate(brightness,
			brightness_seed_alpha_lut_dc[i-1].brightness,
			brightness_seed_alpha_lut_dc[i].brightness,
			brightness_seed_alpha_lut_dc[i-1].alpha,
			brightness_seed_alpha_lut_dc[i].alpha);

	return alpha;
}

int oplus_get_panel_brightness_to_alpha(void)
{
	if (oplus_panel_alpha)
		return oplus_panel_alpha;

	return brightness_to_alpha(oppo_display_brightness);
}

static ssize_t oplus_display_get_dim_alpha(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	if (!oplus_mtk_drm_get_hbm_state())
		return sprintf(buf, "%d\n", 0);

	oplus_underbrightness_alpha = oplus_get_panel_brightness_to_alpha();

	return sprintf(buf, "%d\n", oplus_underbrightness_alpha);
}


static ssize_t oplus_display_set_dim_alpha(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf, size_t count)
{
	sscanf(buf, "%x", &oplus_panel_alpha);
	return count;
}

int oplus_dc_alpha = 0;
extern int oplus_dc_enable;
static ssize_t oplus_display_get_dc_enable(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", oplus_dc_enable);
}

static ssize_t oplus_display_set_dc_enable(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf, size_t count)
{
	sscanf(buf, "%x", &oplus_dc_enable);
	return count;
}

static ssize_t oplus_display_get_dim_dc_alpha(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", oplus_dc_alpha);
}

static ssize_t oplus_display_set_dim_dc_alpha(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf, size_t count)
{
	sscanf(buf, "%x", &oplus_dc_alpha);
	return count;
}

unsigned long silence_mode = 0;
static ssize_t silence_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	printk("%s silence_mode=%ld\n", __func__, silence_mode);
	return sprintf(buf, "%ld\n", silence_mode);
}

static ssize_t silence_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t num)
{
	int ret;
	msleep(1000);
	ret = kstrtoul(buf, 10, &silence_mode);
	printk("%s silence_mode=%ld\n", __func__, silence_mode);
	return num;
}

/*unsigned long CABC_mode = 2;
*
* add dre only use for camera
*
* extern void disp_aal_set_dre_en(int enable);*/

/*static ssize_t LCM_CABC_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
    printk("%s CABC_mode=%ld\n", __func__, CABC_mode);
    return sprintf(buf, "%ld\n", CABC_mode);
}

static ssize_t LCM_CABC_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t num)
{
    int ret = 0;

    ret = kstrtoul(buf, 10, &CABC_mode);
    if( CABC_mode > 3 ){
        CABC_mode = 3;
    }
    printk("%s CABC_mode=%ld\n", __func__, CABC_mode);

    if (CABC_mode == 0) {
        disp_aal_set_dre_en(1);
        printk("%s enable dre\n", __func__);

    } else {
        disp_aal_set_dre_en(0);
        printk("%s disable dre\n", __func__);
    }

    if (oppo_display_cabc_support) {
        ret = primary_display_set_cabc_mode((unsigned int)CABC_mode);
    }

    return num;
}*/

static ssize_t oplus_display_get_ESD(struct device *dev,
                struct device_attribute *attr, char *buf)
{
	printk("%s esd=%ld\n", __func__, esd_mode);
	return sprintf(buf, "%ld\n", esd_mode);
}

static ssize_t oplus_display_set_ESD(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t num)
{
	int ret = 0;

	ret = kstrtoul(buf, 10, &esd_mode);
	printk("%s,esd mode is %d\n", __func__, esd_mode);
	return num;
}

unsigned long cabc_mode = 1;
unsigned long cabc_true_mode = 1;
unsigned long cabc_sun_flag = 0;
unsigned long cabc_back_flag = 1;
extern void disp_aal_set_dre_en(int enable);

enum{
	CABC_LEVEL_0,
	CABC_LEVEL_1,
	CABC_LEVEL_2 = 3,
	CABC_EXIT_SPECIAL = 8,
	CABC_ENTER_SPECIAL = 9,
};

static ssize_t oplus_display_get_CABC(struct device *dev,
                struct device_attribute *attr, char *buf)
{
	printk("%s CABC_mode=%ld\n", __func__, cabc_true_mode);
	return sprintf(buf, "%ld\n", cabc_true_mode);
}

static ssize_t oplus_display_set_CABC(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t num)
{
	int ret = 0;
	struct drm_crtc *crtc;
	struct drm_device *ddev = get_drm_device();

	ret = kstrtoul(buf, 10, &cabc_mode);
	cabc_true_mode = cabc_mode;
	printk("%s,cabc mode is %d, cabc_back_flag is %d\n", __func__, cabc_mode, cabc_back_flag);
	if(cabc_mode < 4)
		cabc_back_flag = cabc_mode;

	if (cabc_mode == CABC_ENTER_SPECIAL) {
		cabc_sun_flag = 1;
		cabc_true_mode = 0;
	} else if (cabc_mode == CABC_EXIT_SPECIAL) {
		cabc_sun_flag = 0;
		cabc_true_mode = cabc_back_flag;
	} else if (cabc_sun_flag == 1) {
		if (cabc_back_flag == CABC_LEVEL_0) {
			disp_aal_set_dre_en(1);
			printk("%s sun enable dre\n", __func__);
		} else {
			disp_aal_set_dre_en(1);
			printk("%s sun disable dre\n", __func__);
		}
		return num;
	}

	printk("%s,cabc mode is %d\n", __func__, cabc_true_mode);

	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(ddev)->mode_config.crtc_list,
				typeof(*crtc), head);
	if (!crtc) {
		printk(KERN_ERR "find crtc fail\n");
		return 0;
	}
	if (cabc_true_mode == CABC_LEVEL_0 && cabc_back_flag == CABC_LEVEL_0) {
		disp_aal_set_dre_en(1);
		printk("%s enable dre\n", __func__);
	} else {
		disp_aal_set_dre_en(1);
		printk("%s disable dre\n", __func__);
	}
	oplus_mtk_drm_setcabc(crtc, cabc_true_mode);
	if (cabc_true_mode != cabc_back_flag) cabc_true_mode = cabc_back_flag;

	return num;
}

void oppo_cmdq_cb(struct cmdq_cb_data data) {
	struct mtk_cmdq_cb_data *cb_data = data.data;

	cmdq_pkt_destroy(cb_data->cmdq_handle);
	kfree(cb_data);
}

unsigned char aod_area_set_flag = 0;
static ssize_t oppo_display_set_aod_area(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	char *bufp = (char *)buf;
	char *token;
	int i, cnt = 0;
	char payload[RAMLESS_AOD_PAYLOAD_SIZE];

	memset(oppo_aod_area, 0, sizeof(struct aod_area) * RAMLESS_AOD_AREA_NUM);

	pr_err("yzq: %s %d\n", __func__, __LINE__);
	while ((token = strsep(&bufp, ":")) != NULL) {
		struct aod_area *area = &oppo_aod_area[cnt];
		if (!*token)
			continue;

		sscanf(token, "%d %d %d %d %d %d %d %d",
			&area->x, &area->y, &area->w, &area->h,
			&area->color, &area->bitdepth, &area->mono, &area->gray);
		pr_err("yzq: %s %d rect[%dx%d-%dx%d]-%d-%d-%d-%x\n", __func__, __LINE__,
			area->x, area->y, area->w, area->h,
			area->color, area->bitdepth, area->mono, area->gray);
		area->enable = true;
		cnt++;
	}

	memset(payload, 0, RAMLESS_AOD_PAYLOAD_SIZE);
	memset(send_cmd, 0, RAMLESS_AOD_PAYLOAD_SIZE);

	for (i = 0; i < RAMLESS_AOD_AREA_NUM; i++) {
		struct aod_area *area = &oppo_aod_area[i];

		payload[0] |= (!!area->enable) << (RAMLESS_AOD_AREA_NUM - i - 1);
		if (area->enable) {
			int h_start = area->x;
			int h_block = area->w / 100;
			int v_start = area->y;
			int v_end = area->y + area->h;
			int off = i * 5;

			/* Rect Setting */
			payload[1 + off] = h_start >> 4;
			payload[2 + off] = ((h_start & 0xf) << 4) | (h_block & 0xf);
			payload[3 + off] = v_start >> 4;
			payload[4 + off] = ((v_start & 0xf) << 4) | ((v_end >> 8) & 0xf);
			payload[5 + off] = v_end & 0xff;

			/* Mono Setting */
			#define SET_MONO_SEL(index, shift) \
				if (i == index) \
					payload[31] |= area->mono << shift;

			SET_MONO_SEL(0, 6);
			SET_MONO_SEL(1, 5);
			SET_MONO_SEL(2, 4);
			SET_MONO_SEL(3, 2);
			SET_MONO_SEL(4, 1);
			SET_MONO_SEL(5, 0);
			#undef SET_MONO_SEL

			/* Depth Setting */
			if (i < 4)
				payload[32] |= (area->bitdepth & 0x3) << ((3 - i) * 2);
			else if (i == 4)
				payload[33] |= (area->bitdepth & 0x3) << 6;
			else if (i == 5)
				payload[33] |= (area->bitdepth & 0x3) << 4;
			/* Color Setting */
			#define SET_COLOR_SEL(index, reg, shift) \
				if (i == index) \
					payload[reg] |= (area->color & 0x7) << shift;
			SET_COLOR_SEL(0, 34, 4);
			SET_COLOR_SEL(1, 34, 0);
			SET_COLOR_SEL(2, 35, 4);
			SET_COLOR_SEL(3, 35, 0);
			SET_COLOR_SEL(4, 36, 4);
			SET_COLOR_SEL(5, 36, 0);
			#undef SET_COLOR_SEL
			/* Area Gray Setting */
			payload[37 + i] = area->gray & 0xff;
		}
	}
	payload[43] = 0x00;
	send_cmd[0] = 0x81;
	for(i = 0; i< 44; i++){
		pr_err("payload[%d] = 0x%x- send_cmd[%d] = 0x%x-", i,payload[i],i,send_cmd[i]);
		send_cmd[i+1] = payload[i];
	}
	aod_area_set_flag = 1;
	return count;
}

static ssize_t oplus_display_get_seed(struct device *dev,
		struct device_attribute *attr, char *buf)
{
        printk(KERN_INFO "oplus_display_get_seed = %d\n", seed_mode);

        return sprintf(buf, "%d\n", seed_mode);
}
static ssize_t oplus_display_set_seed(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	ret = kstrtoul(buf, 10, &seed_mode);
	printk("%s,esd mode is %d\n", __func__, seed_mode);
	return count;
}

static struct kobject *oplus_display_kobj;
static DEVICE_ATTR(oppo_brightness, S_IRUGO|S_IWUSR, oplus_display_get_brightness, oplus_display_set_brightness);
static DEVICE_ATTR(oppo_max_brightness, S_IRUGO|S_IWUSR, oplus_display_get_max_brightness, NULL);
static DEVICE_ATTR(max_brightness, S_IRUGO|S_IWUSR, oplus_display_get_maxbrightness, NULL);
static DEVICE_ATTR(hbm, S_IRUGO|S_IWUSR, oplus_display_get_hbm, oplus_display_set_hbm);
static DEVICE_ATTR(panel_pwr, S_IRUGO|S_IWUSR, oplus_display_get_panel_pwr, oplus_display_set_panel_pwr);
static DEVICE_ATTR(oppo_notify_fppress, S_IRUGO|S_IWUSR, NULL, fingerprint_notify_trigger);
static DEVICE_ATTR(dim_alpha, S_IRUGO|S_IWUSR, oplus_display_get_dim_alpha, oplus_display_set_dim_alpha);
static DEVICE_ATTR(dimlayer_bl_en, S_IRUGO|S_IWUSR, oplus_display_get_dc_enable, oplus_display_set_dc_enable);
static DEVICE_ATTR(dim_dc_alpha, S_IRUGO|S_IWUSR, oplus_display_get_dim_dc_alpha, oplus_display_set_dim_dc_alpha);
static DEVICE_ATTR(panel_serial_number, S_IRUGO|S_IWUSR, oplus_get_panel_serial_number, panel_serial_store);
static DEVICE_ATTR(LCM_CABC, S_IRUGO|S_IWUSR, oplus_display_get_CABC, oplus_display_set_CABC);
static DEVICE_ATTR(esd, S_IRUGO|S_IWUSR, oplus_display_get_ESD, oplus_display_set_ESD);
static DEVICE_ATTR(sau_closebl_node, S_IRUGO|S_IWUSR, silence_show, silence_store);
static DEVICE_ATTR(seed, S_IRUGO|S_IWUSR, oplus_display_get_seed, oplus_display_set_seed);

/*
static DEVICE_ATTR(oppo_brightness, S_IRUGO|S_IWUSR, oppo_display_get_brightness, oppo_display_set_brightness);
static DEVICE_ATTR(oppo_max_brightness, S_IRUGO|S_IWUSR, oppo_display_get_max_brightness, NULL);
static DEVICE_ATTR(fingerprint_notify, S_IRUGO|S_IWUSR, NULL, fingerprint_notify_trigger);*/
static DEVICE_ATTR(aod_area, S_IRUGO|S_IWUSR, NULL, oppo_display_set_aod_area);
/*static DEVICE_ATTR(sau_closebl_node, S_IRUGO|S_IWUSR, silence_show, silence_store);
static DEVICE_ATTR(panel_serial_number, S_IRUGO|S_IWUSR, oplus_get_panel_serial_number, panel_serial_store);
static DEVICE_ATTR(ffl_set, S_IRUGO|S_IWUSR, FFL_SET_show, FFL_SET_store);
static DEVICE_ATTR(LCM_CABC, S_IRUGO|S_IWUSR, LCM_CABC_show, LCM_CABC_store);
static DEVICE_ATTR(aod_light_mode_set, S_IRUGO|S_IWUSR, oppo_get_aod_light_mode, oppo_set_aod_light_mode);
static DEVICE_ATTR(cabc, S_IRUGO|S_IWUSR, oplus_display_get_CABC, oplus_display_set_CABC);
static DEVICE_ATTR(esd, S_IRUGO|S_IWUSR, oplus_display_get_ESD, oplus_display_set_ESD);*/

/*
 * Create a group of attributes so that we can create and destroy them all
 * at once.
 */
static struct attribute *oplus_display_attrs[] = {
	&dev_attr_oppo_brightness.attr,
	&dev_attr_oppo_max_brightness.attr,
	&dev_attr_max_brightness.attr,
	&dev_attr_hbm.attr,
	&dev_attr_panel_pwr.attr,
	&dev_attr_oppo_notify_fppress.attr,
	&dev_attr_dim_alpha.attr,
	&dev_attr_dimlayer_bl_en.attr,
	&dev_attr_dim_dc_alpha.attr,
	&dev_attr_panel_serial_number.attr,
	&dev_attr_LCM_CABC.attr,
	&dev_attr_esd.attr,
	&dev_attr_sau_closebl_node.attr,
	&dev_attr_seed.attr,
/*
	&dev_attr_oppo_brightness.attr,
	&dev_attr_oppo_max_brightness.attr,
	&dev_attr_fingerprint_notify.attr,*/
	&dev_attr_aod_area.attr,
	/*&dev_attr_sau_closebl_node.attr,
	&dev_attr_panel_serial_number.attr,
	&dev_attr_ffl_set.attr,
	&dev_attr_LCM_CABC.attr,
	&dev_attr_cabc.attr,
	&dev_attr_esd.attr,
	&dev_attr_aod_light_mode_set.attr,*/
	NULL,	/* need to NULL terminate the list of attributes */
};

static struct attribute_group oplus_display_attr_group = {
	.attrs = oplus_display_attrs,
};

static int __init oplus_display_private_api_init(void)
{
	int retval;

	oplus_display_kobj = kobject_create_and_add("oppo_display", kernel_kobj);
	if (!oplus_display_kobj)
		return -ENOMEM;

	/* Create the files associated with this kobject */
	retval = sysfs_create_group(oplus_display_kobj, &oplus_display_attr_group);
	if (retval)
		kobject_put(oplus_display_kobj);

	return retval;
}

static void __exit oplus_display_private_api_exit(void)
{
	kobject_put(oplus_display_kobj);
}

module_init(oplus_display_private_api_init);
module_exit(oplus_display_private_api_exit);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Hujie <hujie@oppo.com>");
