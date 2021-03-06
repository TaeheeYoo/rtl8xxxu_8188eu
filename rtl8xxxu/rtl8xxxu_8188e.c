/******************************************************************************
 *
 * Copyright(c) 2007 - 2011 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 * 
 * Taehee Yoo <ap420073@gmail.com>
 * Larry Finger <Larry.Finger@lwfinger.net>
 *
 ******************************************************************************/


#define DEBUG

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/usb.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/wireless.h>
#include <linux/firmware.h>
#include <linux/moduleparam.h>
#include <net/mac80211.h>
#include "rtl8xxxu.h"
#include "rtl8xxxu_regs.h"

#include "wifi.h"
#include "efuse.h"
#include "fw.h"
#include "cam.h"
#include "pwrseqcmd.h"
#include "pwrseq.h"
#include "reg.h"
#include "def.h"
#include "dm.h"
#include "led.h"
#include "hw.h"
#include "pwrseq.h"
#include "table.h"
#include "trx.h"

struct wlan_pwr_cfg rtl8188ee_power_on_flow[RTL8188EE_TRANS_CARDEMU_TO_ACT_STEPS
					+ RTL8188EE_TRANS_END_STEPS] = {
	RTL8188EE_TRANS_CARDEMU_TO_ACT
	RTL8188EE_TRANS_END
};

/*3Radio off GPIO Array */
struct wlan_pwr_cfg rtl8188ee_radio_off_flow[RTL8188EE_TRANS_ACT_TO_CARDEMU_STEPS
					+ RTL8188EE_TRANS_END_STEPS] = {
	RTL8188EE_TRANS_ACT_TO_CARDEMU
	RTL8188EE_TRANS_END
};

/*3Card Disable Array*/
struct wlan_pwr_cfg rtl8188ee_card_disable_flow
		[RTL8188EE_TRANS_ACT_TO_CARDEMU_STEPS +
		 RTL8188EE_TRANS_CARDEMU_TO_PDN_STEPS +
		 RTL8188EE_TRANS_END_STEPS] = {
	RTL8188EE_TRANS_ACT_TO_CARDEMU
	RTL8188EE_TRANS_CARDEMU_TO_CARDDIS
	RTL8188EE_TRANS_END
};

/*3 Card Enable Array*/
struct wlan_pwr_cfg rtl8188ee_card_enable_flow
		[RTL8188EE_TRANS_ACT_TO_CARDEMU_STEPS +
		 RTL8188EE_TRANS_CARDEMU_TO_PDN_STEPS +
		 RTL8188EE_TRANS_END_STEPS] = {
	RTL8188EE_TRANS_CARDDIS_TO_CARDEMU
	RTL8188EE_TRANS_CARDEMU_TO_ACT
	RTL8188EE_TRANS_END
};

/*3Suspend Array*/
struct wlan_pwr_cfg rtl8188ee_suspend_flow[RTL8188EE_TRANS_ACT_TO_CARDEMU_STEPS
					+ RTL8188EE_TRANS_CARDEMU_TO_SUS_STEPS
					+ RTL8188EE_TRANS_END_STEPS] = {
	RTL8188EE_TRANS_ACT_TO_CARDEMU
	RTL8188EE_TRANS_CARDEMU_TO_SUS
	RTL8188EE_TRANS_END
};

/*3 Resume Array*/
struct wlan_pwr_cfg rtl8188ee_resume_flow[RTL8188EE_TRANS_ACT_TO_CARDEMU_STEPS
					+ RTL8188EE_TRANS_CARDEMU_TO_SUS_STEPS
					+ RTL8188EE_TRANS_END_STEPS] = {
	RTL8188EE_TRANS_SUS_TO_CARDEMU
	RTL8188EE_TRANS_CARDEMU_TO_ACT
	RTL8188EE_TRANS_END
};

/*3HWPDN Array*/
struct wlan_pwr_cfg rtl8188ee_hwpdn_flow[RTL8188EE_TRANS_ACT_TO_CARDEMU_STEPS
				+ RTL8188EE_TRANS_CARDEMU_TO_PDN_STEPS
				+ RTL8188EE_TRANS_END_STEPS] = {
	RTL8188EE_TRANS_ACT_TO_CARDEMU
	RTL8188EE_TRANS_CARDEMU_TO_PDN
	RTL8188EE_TRANS_END
};

/*3 Enter LPS */
struct wlan_pwr_cfg rtl8188ee_enter_lps_flow[RTL8188EE_TRANS_ACT_TO_LPS_STEPS
					+ RTL8188EE_TRANS_END_STEPS] = {
	/*FW behavior*/
	RTL8188EE_TRANS_ACT_TO_LPS
	RTL8188EE_TRANS_END
};

/*3 Leave LPS */
struct wlan_pwr_cfg rtl8188ee_leave_lps_flow[RTL8188EE_TRANS_LPS_TO_ACT_STEPS
					+ RTL8188EE_TRANS_END_STEPS] = {
	/*FW behavior*/
	RTL8188EE_TRANS_LPS_TO_ACT
	RTL8188EE_TRANS_END
};

u32 RTL8188EUPHY_REG_ARRAY_PG[] = {
		0xe00, 0x06070809,
		0xe04, 0x02020405,
		0xe08, 0x00000006,
		0x86c, 0x00020400,
		0xe10, 0x08090a0b,
		0xe14, 0x01030607,
		0xe18, 0x08090a0b,
		0xe1c, 0x01030607,
		0xe00, 0x00000000,
		0xe04, 0x00000000,
		0xe08, 0x00000000,
		0x86c, 0x00000000,
		0xe10, 0x00000000,
		0xe14, 0x00000000,
		0xe18, 0x00000000,
		0xe1c, 0x00000000,
		0xe00, 0x02020202,
		0xe04, 0x00020202,
		0xe08, 0x00000000,
		0x86c, 0x00000000,
		0xe10, 0x04040404,
		0xe14, 0x00020404,
		0xe18, 0x00000000,
		0xe1c, 0x00000000,
		0xe00, 0x02020202,
		0xe04, 0x00020202,
		0xe08, 0x00000000,
		0x86c, 0x00000000,
		0xe10, 0x04040404,
		0xe14, 0x00020404,
		0xe18, 0x00000000,
		0xe1c, 0x00000000,
		0xe00, 0x00000000,
		0xe04, 0x00000000,
		0xe08, 0x00000000,
		0x86c, 0x00000000,
		0xe10, 0x00000000,
		0xe14, 0x00000000,
		0xe18, 0x00000000,
		0xe1c, 0x00000000,
		0xe00, 0x02020202,
		0xe04, 0x00020202,
		0xe08, 0x00000000,
		0x86c, 0x00000000,
		0xe10, 0x04040404,
		0xe14, 0x00020404,
		0xe18, 0x00000000,
		0xe1c, 0x00000000,
		0xe00, 0x00000000,
		0xe04, 0x00000000,
		0xe08, 0x00000000,
		0x86c, 0x00000000,
		0xe10, 0x00000000,
		0xe14, 0x00000000,
		0xe18, 0x00000000,
		0xe1c, 0x00000000,
		0xe00, 0x00000000,
		0xe04, 0x00000000,
		0xe08, 0x00000000,
		0x86c, 0x00000000,
		0xe10, 0x00000000,
		0xe14, 0x00000000,
		0xe18, 0x00000000,
		0xe1c, 0x00000000,
		0xe00, 0x00000000,
		0xe04, 0x00000000,
		0xe08, 0x00000000,
		0x86c, 0x00000000,
		0xe10, 0x00000000,
		0xe14, 0x00000000,
		0xe18, 0x00000000,
		0xe1c, 0x00000000,
		0xe00, 0x00000000,
		0xe04, 0x00000000,
		0xe08, 0x00000000,
		0x86c, 0x00000000,
		0xe10, 0x00000000,
		0xe14, 0x00000000,
		0xe18, 0x00000000,
		0xe1c, 0x00000000,
		0xe00, 0x00000000,
		0xe04, 0x00000000,
		0xe08, 0x00000000,
		0x86c, 0x00000000,
		0xe10, 0x00000000,
		0xe14, 0x00000000,
		0xe18, 0x00000000,
		0xe1c, 0x00000000,
};

static struct rtl8xxxu_rfregval rtl8188eu_radioa_1t_init_table[] = {
	{0x00, 0x00030000}, {0x08, 0x00084000},
	{0x18, 0x00000407}, {0x19, 0x00000012},
	{0x1e, 0x00080009}, {0x1f, 0x00000880},
	{0x2f, 0x0001a060}, {0x3f, 0x00000000},
	{0x42, 0x000060c0}, {0x57, 0x000d0000},
	{0x58, 0x000be180}, {0x67, 0x00001552},
	{0x83, 0x00000000}, {0xb0, 0x000ff8fc},
	{0xb1, 0x00054400}, {0xb2, 0x000ccc19},
	{0xb4, 0x00043003}, {0xb6, 0x0004953e},
	{0xb7, 0x0001c718}, {0xb8, 0x000060ff},
	{0xb9, 0x00080001}, {0xba, 0x00040000},
	{0xbb, 0x00000400}, {0xbf, 0x000c0000},
	{0xc2, 0x00002400}, {0xc3, 0x00000009},
	{0xc4, 0x00040c91}, {0xc5, 0x00099999},
	{0xc6, 0x000000a3}, {0xc7, 0x00088820},
	{0xc8, 0x00076c06}, {0xc9, 0x00000000},
	{0xca, 0x00080000}, {0xdf, 0x00000180},
	{0xef, 0x000001a0}, {0x51, 0x0006b27d},
	{0x52, 0x0007e4dd}, {0x52, 0x0007e49d},
	{0x53, 0x00000073}, {0x56, 0x00051ff3},
	{0x35, 0x00000086}, {0x35, 0x00000186},
	{0x35, 0x00000286}, {0x36, 0x00001c25},
	{0x36, 0x00009c25}, {0x36, 0x00011c25},
	{0x36, 0x00019c25}, {0xb6, 0x00048538},
	{0x18, 0x00000c07}, {0x5a, 0x0004bd00},
	{0x19, 0x000739d0}, {0x34, 0x0000adf3},
	{0x34, 0x00009df0}, {0x34, 0x00008ded},
	{0x34, 0x00007dea}, {0x34, 0x00006de7},
	{0x34, 0x000054ee}, {0x34, 0x000044eb},
	{0x34, 0x000034e8}, {0x34, 0x0000246b},
	{0x34, 0x00001468}, {0x34, 0x0000006d},
	{0x00, 0x00030159}, {0x84, 0x00068200},
	{0x86, 0x000000ce}, {0x87, 0x00048a00},
	{0x8e, 0x00065540}, {0x8f, 0x00088000},
	{0xef, 0x000020a0}, {0x3b, 0x000f02b0},
	{0x3b, 0x000ef7b0}, {0x3b, 0x000d4fb0},
	{0x3b, 0x000cf060}, {0x3b, 0x000b0090},
	{0x3b, 0x000a0080}, {0x3b, 0x00090080},
	{0x3b, 0x0008f780}, {0x3b, 0x000722b0},
	{0x3b, 0x0006f7b0}, {0x3b, 0x00054fb0},
	{0x3b, 0x0004f060}, {0x3b, 0x00030090},
	{0x3b, 0x00020080}, {0x3b, 0x00010080},
	{0x3b, 0x0000f780}, {0xef, 0x000000a0},
	{0x00, 0x00010159}, {0x18, 0x0000f407},
	{0xfe, 0x00000000}, {0xfe, 0x00000000},
	{0x1f, 0x00080003}, {0xfe, 0x00000000},
	{0xfe, 0x00000000}, {0x1e, 0x00000001},
	{0x1f, 0x00080000}, {0x00, 0x00033e60},
	{0xff, 0xffffffff},
};

struct rtl8xxxu_reg8val rtl8188eu_mac_init_table[] = {
	{0x026, 0x41}, {0x027, 0x35}, {0x428, 0x0A}, {0x429, 0x10},
	{0x430, 0x00}, {0x431, 0x01}, {0x432, 0x02}, {0x433, 0x04},
	{0x434, 0x05}, {0x435, 0x06}, {0x436, 0x07}, {0x437, 0x08},
	{0x438, 0x00}, {0x439, 0x00}, {0x43A, 0x01}, {0x43B, 0x02},
	{0x43C, 0x04}, {0x43D, 0x05}, {0x43E, 0x06}, {0x43F, 0x07},
	{0x440, 0x5D}, {0x441, 0x01}, {0x442, 0x00}, {0x444, 0x15},
	{0x445, 0xF0}, {0x446, 0x0F}, {0x447, 0x00}, {0x458, 0x41},
	{0x459, 0xA8}, {0x45A, 0x72}, {0x45B, 0xB9}, {0x460, 0x66},
	{0x461, 0x66}, {0x480, 0x08}, {0x4C8, 0xFF}, {0x4C9, 0x08},
	{0x4CC, 0xFF}, {0x4CD, 0xFF}, {0x4CE, 0x01}, {0x4D3, 0x01},
	{0x500, 0x26}, {0x501, 0xA2}, {0x502, 0x2F}, {0x503, 0x00},
	{0x504, 0x28}, {0x505, 0xA3}, {0x506, 0x5E}, {0x507, 0x00},
	{0x508, 0x2B}, {0x509, 0xA4}, {0x50A, 0x5E}, {0x50B, 0x00},
	{0x50C, 0x4F}, {0x50D, 0xA4}, {0x50E, 0x00}, {0x50F, 0x00},
	{0x512, 0x1C}, {0x514, 0x0A}, {0x516, 0x0A}, {0x525, 0x4F},
	{0x550, 0x10}, {0x551, 0x10}, {0x559, 0x02}, {0x55D, 0xFF},
	{0x605, 0x30}, {0x608, 0x0E}, {0x609, 0x2A}, {0x620, 0xFF},
	{0x621, 0xFF}, {0x622, 0xFF}, {0x623, 0xFF}, {0x624, 0xFF},
	{0x625, 0xFF}, {0x626, 0xFF}, {0x627, 0xFF}, {0x652, 0x20},
	{0x63C, 0x0A}, {0x63D, 0x0A}, {0x63E, 0x0E}, {0x63F, 0x0E},
	{0x640, 0x40}, {0x66E, 0x05}, {0x700, 0x21}, {0x701, 0x43},
	{0x702, 0x65}, {0x703, 0x87}, {0x708, 0x21}, {0x709, 0x43},
	{0x70A, 0x65}, {0x70B, 0x87}, {0xffff, 0xff},
};

static struct rtl8xxxu_power_base rtl8188eu_power_base = {
	.reg_0e00 = 0x06070809,
	.reg_0e04 = 0x02020405,
	.reg_0e08 = 0x00000006,
	.reg_086c = 0x00020400,

	.reg_0e10 = 0x08090a0b,
	.reg_0e14 = 0x01030607,
	.reg_0e18 = 0x08090a0b,
	.reg_0e1c = 0x01030607,

	.reg_0830 = 0x00000000,
	.reg_0834 = 0x00000000,
	.reg_0838 = 0x00000000,
	.reg_086c_2 = 0x00000000,

	.reg_083c = 0x00000000,
	.reg_0848 = 0x00000000,
	.reg_084c = 0x00000000,
	.reg_0868 = 0x00000000,
};

int reg_bcn_ctrl_val;
spinlock_t rf_lock;

struct rtl_efuse rtlefuse;
struct rtl_phy rtlphy;
struct rtl_dm rtldm;

static inline void RT_TRACE(struct rtl8xxxu_priv *priv,
			    int comp, int level,
			    const char *fmt, ...)
{
}

static u32 _rtl88e_phy_calculate_bit_shift(u32 bitmask)
{
	u32 i;

	for (i = 0; i <= 31; i++) {
		if (((bitmask >> i) & 0x1) == 1)
			break;
	}
	return i;
}

u32 rtl_get_bbreg(struct rtl8xxxu_priv *priv, u32 regaddr, u32 bitmask)
{
	u32 returnvalue, originalvalue, bitshift;

	RT_TRACE(priv, COMP_RF, DBG_TRACE,
		 "regaddr(%#x), bitmask(%#x)\n", regaddr, bitmask);
	originalvalue = rtl8xxxu_read32(priv, regaddr);
	bitshift = _rtl88e_phy_calculate_bit_shift(bitmask);
	returnvalue = (originalvalue & bitmask) >> bitshift;

	RT_TRACE(priv, COMP_RF, DBG_TRACE,
		 "BBR MASK=0x%x Addr[0x%x]=0x%x\n", bitmask,
		 regaddr, originalvalue);

	return returnvalue;

}

void rtl_set_bbreg(struct rtl8xxxu_priv *priv,
			   u32 regaddr, u32 bitmask, u32 data)
{
	u32 originalvalue, bitshift;

	RT_TRACE(priv, COMP_RF, DBG_TRACE,
		 "regaddr(%#x), bitmask(%#x), data(%#x)\n",
		 regaddr, bitmask, data);

	if (bitmask != MASKDWORD) {
		originalvalue = rtl8xxxu_read32(priv, regaddr);
		bitshift = _rtl88e_phy_calculate_bit_shift(bitmask);
		data = ((originalvalue & (~bitmask)) | (data << bitshift));
	}

	rtl8xxxu_write32(priv, regaddr, data);

	RT_TRACE(priv, COMP_RF, DBG_TRACE,
		 "regaddr(%#x), bitmask(%#x), data(%#x)\n",
		 regaddr, bitmask, data);
}

static u32 _rtl88e_phy_rf_serial_read(struct rtl8xxxu_priv *priv,
				      enum radio_path rfpath, u32 offset)
{
	u32 newoffset;
	u32 tmplong, tmplong2;
	u8 rfpi_enable = 0;
	u32 retvalue;
	struct bb_reg_def *pphyreg;

	pphyreg = &rtlphy.phyreg_def[0];


	offset &= 0xff;
	newoffset = offset;
	tmplong = rtl_get_bbreg(priv, RFPGA0_XA_HSSIPARAMETER2, MASKDWORD);
	if (rfpath == RF90_PATH_A)
		tmplong2 = tmplong;
	else
		tmplong2 = rtl_get_bbreg(priv, RFPGA0_XA_HSSIPARAMETER2, MASKDWORD);
	tmplong2 = (tmplong2 & (~BLSSIREADADDRESS)) |
	    (newoffset << 23) | BLSSIREADEDGE;
	rtl_set_bbreg(priv, RFPGA0_XA_HSSIPARAMETER2, MASKDWORD,
		      tmplong & (~BLSSIREADEDGE));
	mdelay(1);
	rtl_set_bbreg(priv, RFPGA0_XA_HSSIPARAMETER2, MASKDWORD, tmplong2);
	mdelay(2);
	if (rfpath == RF90_PATH_A)
		rfpi_enable = (u8)rtl_get_bbreg(priv, RFPGA0_XA_HSSIPARAMETER1,
						BIT(8));
	else if (rfpath == RF90_PATH_B)
		rfpi_enable = (u8)rtl_get_bbreg(priv, RFPGA0_XB_HSSIPARAMETER1,
						BIT(8));
	if (rfpi_enable)
		retvalue = rtl_get_bbreg(priv, TRANSCEIVEA_HSPI_READBACK,
					 BLSSIREADBACKDATA);
	else
		retvalue = rtl_get_bbreg(priv, RFPGA0_XA_LSSIREADBACK,
					 BLSSIREADBACKDATA);
	RT_TRACE(priv, COMP_RF, DBG_TRACE,
		 "RFR-%d Addr[0x%x]=0x%x\n",
		 rfpath, RFPGA0_XA_LSSIREADBACK, retvalue);
	return retvalue;
}

static void _rtl88e_phy_rf_serial_write(struct rtl8xxxu_priv *priv,
					enum radio_path rfpath, u32 offset,
					u32 data)
{
	u32 data_and_addr;
	u32 newoffset;
	struct bb_reg_def *pphyreg;

	pphyreg = &rtlphy.phyreg_def[0];



	offset &= 0xff;
	newoffset = offset;
	data_and_addr = ((newoffset << 20) | (data & 0x000fffff)) & 0x0fffffff;
	rtl_set_bbreg(priv, RFPGA0_XA_LSSIPARAMETER, MASKDWORD, data_and_addr);
	RT_TRACE(priv, COMP_RF, DBG_TRACE,
		 "RFW-%d Addr[0x%x]=0x%x\n",
		 rfpath, RFPGA0_XA_LSSIPARAMETER, data_and_addr);
}

u32 rtl_get_rfreg(struct rtl8xxxu_priv *priv,
			    enum radio_path rfpath, u32 regaddr, u32 bitmask)
{
	u32 original_value, readback_value, bitshift;
	unsigned long flags;

	RT_TRACE(priv, COMP_RF, DBG_TRACE,
		 "regaddr(%#x), rfpath(%#x), bitmask(%#x)\n",
		 regaddr, rfpath, bitmask);

	spin_lock_irqsave(&rf_lock, flags);


	original_value = _rtl88e_phy_rf_serial_read(priv, rfpath, regaddr);
	bitshift = _rtl88e_phy_calculate_bit_shift(bitmask);
	readback_value = (original_value & bitmask) >> bitshift;
	spin_unlock_irqrestore(&rf_lock, flags);

	RT_TRACE(priv, COMP_RF, DBG_TRACE,
		 "regaddr(%#x), rfpath(%#x), bitmask(%#x), original_value(%#x)\n",
		  regaddr, rfpath, bitmask, original_value);
	return readback_value;
}

void rtl_set_rfreg(struct rtl8xxxu_priv *priv,
			   enum radio_path rfpath,
			   u32 regaddr, u32 bitmask, u32 data)
{
	u32 original_value, bitshift;
	unsigned long flags;

	RT_TRACE(priv, COMP_RF, DBG_TRACE,
		 "regaddr(%#x), bitmask(%#x), data(%#x), rfpath(%#x)\n",
		  regaddr, bitmask, data, rfpath);
	spin_lock_irqsave(&rf_lock, flags);

	if (bitmask != RFREG_OFFSET_MASK) {
			original_value = _rtl88e_phy_rf_serial_read(priv,
								    rfpath,
								    regaddr);
			bitshift = _rtl88e_phy_calculate_bit_shift(bitmask);
			data =
			    ((original_value & (~bitmask)) |
			     (data << bitshift));
		}

	_rtl88e_phy_rf_serial_write(priv, rfpath, regaddr, data);

	spin_unlock_irqrestore(&rf_lock, flags);

	RT_TRACE(priv, COMP_RF, DBG_TRACE,
		 "regaddr(%#x), bitmask(%#x), data(%#x), rfpath(%#x)\n",
		 regaddr, bitmask, data, rfpath);
}

void rtl88e_phy_rf6052_set_bandwidth(struct rtl8xxxu_priv *priv, u8 bandwidth)
{
	switch (bandwidth) {
	case HT_CHANNEL_WIDTH_20:
		rtlphy.rfreg_chnlval[0] = ((rtlphy.rfreg_chnlval[0] &
					     0xfffff3ff) | BIT(10) | BIT(11));
		rtl_set_rfreg(priv, RF90_PATH_A, RF_CHNLBW, RFREG_OFFSET_MASK,
			      rtlphy.rfreg_chnlval[0]);
		break;
	case HT_CHANNEL_WIDTH_20_40:
		rtlphy.rfreg_chnlval[0] = ((rtlphy.rfreg_chnlval[0] &
					     0xfffff3ff) | BIT(10));
		rtl_set_rfreg(priv, RF90_PATH_A, RF_CHNLBW, RFREG_OFFSET_MASK,
			      rtlphy.rfreg_chnlval[0]);
		break;
	default:
		RT_TRACE(priv, COMP_ERR, DBG_EMERG,
			 "unknown bandwidth: %#X\n", bandwidth);
		break;
	}
}

void rtl88e_phy_rf6052_set_cck_txpower(struct rtl8xxxu_priv *priv,
				       u8 *ppowerlevel)
{
	u32 tx_agc[2] = {0, 0}, tmpval;
	bool turbo_scanoff = false;
	u8 idx1, idx2;
	u8 *ptr;
	u8 direction;
	u32 pwrtrac_value;

	if (rtlefuse.eeprom_regulatory != 0)
		turbo_scanoff = true;

#if 0
	if (mac->act_scanning == true) {
#else
	if (0 ) {
#endif
		tx_agc[RF90_PATH_A] = 0x3f3f3f3f;
		tx_agc[RF90_PATH_B] = 0x3f3f3f3f;

		if (turbo_scanoff) {
			for (idx1 = RF90_PATH_A; idx1 <= RF90_PATH_B; idx1++) {
				tx_agc[idx1] = ppowerlevel[idx1] |
				    (ppowerlevel[idx1] << 8) |
				    (ppowerlevel[idx1] << 16) |
				    (ppowerlevel[idx1] << 24);
			}
		}
	} else {
		for (idx1 = RF90_PATH_A; idx1 <= RF90_PATH_B; idx1++) {
			tx_agc[idx1] = ppowerlevel[idx1] |
			    (ppowerlevel[idx1] << 8) |
			    (ppowerlevel[idx1] << 16) |
			    (ppowerlevel[idx1] << 24);
		}

		if (rtlefuse.eeprom_regulatory == 0) {
			tmpval =
			    (rtlphy.mcs_txpwrlevel_origoffset[0][6]) +
			    (rtlphy.mcs_txpwrlevel_origoffset[0][7] <<
			     8);
			tx_agc[RF90_PATH_A] += tmpval;

			tmpval = (rtlphy.mcs_txpwrlevel_origoffset[0][14]) +
			    (rtlphy.mcs_txpwrlevel_origoffset[0][15] <<
			     24);
			tx_agc[RF90_PATH_B] += tmpval;
		}
	}

	for (idx1 = RF90_PATH_A; idx1 <= RF90_PATH_B; idx1++) {
		ptr = (u8 *)(&tx_agc[idx1]);
		for (idx2 = 0; idx2 < 4; idx2++) {
			if (*ptr > RF6052_MAX_TX_PWR)
				*ptr = RF6052_MAX_TX_PWR;
			ptr++;
		}
	}

	//rtl88e_dm_txpower_track_adjust(priv, 1, &direction, &pwrtrac_value);
	if (direction == 1) {
		tx_agc[0] += pwrtrac_value;
		tx_agc[1] += pwrtrac_value;
	} else if (direction == 2) {
		tx_agc[0] -= pwrtrac_value;
		tx_agc[1] -= pwrtrac_value;
	}

	tmpval = tx_agc[RF90_PATH_A] & 0xff;
	rtl_set_bbreg(priv, RTXAGC_A_CCK1_MCS32, MASKBYTE1, tmpval);

	tmpval = tx_agc[RF90_PATH_A] >> 8;

	/*tmpval = tmpval & 0xff00ffff;*/

	rtl_set_bbreg(priv, RTXAGC_B_CCK11_A_CCK2_11, 0xffffff00, tmpval);

	tmpval = tx_agc[RF90_PATH_B] >> 24;
	rtl_set_bbreg(priv, RTXAGC_B_CCK11_A_CCK2_11, MASKBYTE0, tmpval);

	tmpval = tx_agc[RF90_PATH_B] & 0x00ffffff;
	rtl_set_bbreg(priv, RTXAGC_B_CCK1_55_MCS32, 0xffffff00, tmpval);
}

static void _rtl88e_phy_get_power_base(struct rtl8xxxu_priv *priv,
				      u8 *ppowerlevel_ofdm,
				      u8 *ppowerlevel_bw20,
				      u8 *ppowerlevel_bw40, u8 channel,
				      u32 *ofdmbase, u32 *mcsbase)
{
	u32 powerbase0, powerbase1;
	u8 i, powerlevel[2];

	for (i = 0; i < 2; i++) {
		powerbase0 = ppowerlevel_ofdm[i];

		powerbase0 = (powerbase0 << 24) | (powerbase0 << 16) |
		    (powerbase0 << 8) | powerbase0;
		*(ofdmbase + i) = powerbase0;
#if 0
		RTPRINT(rtlpriv, FPHY, PHY_TXPWR,
			" [OFDM power base index rf(%c) = 0x%x]\n",
			 ((i == 0) ? 'A' : 'B'), *(ofdmbase + i));
#endif
	}

	for (i = 0; i < 2; i++) {
		if (rtlphy.current_chan_bw == HT_CHANNEL_WIDTH_20)
			powerlevel[i] = ppowerlevel_bw20[i];
		else
			powerlevel[i] = ppowerlevel_bw40[i];

		powerbase1 = powerlevel[i];
		powerbase1 = (powerbase1 << 24) |
		    (powerbase1 << 16) | (powerbase1 << 8) | powerbase1;

		*(mcsbase + i) = powerbase1;
#if 0
		RTPRINT(rtlpriv, FPHY, PHY_TXPWR,
			" [MCS power base index rf(%c) = 0x%x]\n",
			 ((i == 0) ? 'A' : 'B'), *(mcsbase + i));
#endif
	}
}

static void _rtl88e_get_txpower_writeval_by_regulatory(struct rtl8xxxu_priv *priv,
						       u8 channel, u8 index,
						       u32 *powerbase0,
						       u32 *powerbase1,
						       u32 *p_outwriteval)
{
	u8 i, chnlgroup = 0, pwr_diff_limit[4], pwr_diff = 0, customer_pwr_diff;
	u32 writeval, customer_limit, rf;

	for (rf = 0; rf < 2; rf++) {
		switch (rtlefuse.eeprom_regulatory) {
		case 0:
			chnlgroup = 0;

			writeval =
			    rtlphy.mcs_txpwrlevel_origoffset
				[chnlgroup][index + (rf ? 8 : 0)]
			    + ((index < 2) ? powerbase0[rf] : powerbase1[rf]);
			break;
		case 1:
			if (rtlphy.pwrgroup_cnt == 1) {
				chnlgroup = 0;
			} else {
				if (channel < 3)
					chnlgroup = 0;
				else if (channel < 6)
					chnlgroup = 1;
				else if (channel < 9)
					chnlgroup = 2;
				else if (channel < 12)
					chnlgroup = 3;
				else if (channel < 14)
					chnlgroup = 4;
				else if (channel == 14)
					chnlgroup = 5;
			}

			writeval =
			    rtlphy.mcs_txpwrlevel_origoffset[chnlgroup]
			    [index + (rf ? 8 : 0)] + ((index < 2) ?
						      powerbase0[rf] :
						      powerbase1[rf]);

			break;
		case 2:
			writeval =
			    ((index < 2) ? powerbase0[rf] : powerbase1[rf]);
			break;
		case 3:
			chnlgroup = 0;

			if (index < 2)
				pwr_diff =
				   rtlefuse.txpwr_legacyhtdiff[rf][channel-1];
			else if (rtlphy.current_chan_bw == HT_CHANNEL_WIDTH_20)
				pwr_diff =
					rtlefuse.txpwr_ht20diff[rf][channel-1];

			if (rtlphy.current_chan_bw == HT_CHANNEL_WIDTH_20_40)
				customer_pwr_diff =
					rtlefuse.pwrgroup_ht40[rf][channel-1];
			else
				customer_pwr_diff =
					rtlefuse.pwrgroup_ht20[rf][channel-1];

			if (pwr_diff > customer_pwr_diff)
				pwr_diff = 0;
			else
				pwr_diff = customer_pwr_diff - pwr_diff;

			for (i = 0; i < 4; i++) {
				pwr_diff_limit[i] =
				    (u8)((rtlphy.mcs_txpwrlevel_origoffset
					  [chnlgroup][index +
					  (rf ? 8 : 0)] & (0x7f <<
					  (i * 8))) >> (i * 8));

				if (pwr_diff_limit[i] > pwr_diff)
					pwr_diff_limit[i] = pwr_diff;
			}

			customer_limit = (pwr_diff_limit[3] << 24) |
			    (pwr_diff_limit[2] << 16) |
			    (pwr_diff_limit[1] << 8) | (pwr_diff_limit[0]);

			writeval = customer_limit +
			    ((index < 2) ? powerbase0[rf] : powerbase1[rf]);
			break;
		default:
			chnlgroup = 0;
			writeval =
			    rtlphy.mcs_txpwrlevel_origoffset[chnlgroup]
			    [index + (rf ? 8 : 0)]
			    + ((index < 2) ? powerbase0[rf] : powerbase1[rf]);
			break;
		}

#if 1
		/* dm 추가시 구현할 것 */
		if (rtldm.dynamic_txhighpower_lvl == TXHIGHPWRLEVEL_BT1)
			writeval = writeval - 0x06060606;
		else if (rtldm.dynamic_txhighpower_lvl ==
			 TXHIGHPWRLEVEL_BT2)
			writeval = writeval - 0x0c0c0c0c;
		*(p_outwriteval + rf) = writeval;
#endif
	}
}

static void _rtl88e_write_ofdm_power_reg(struct rtl8xxxu_priv *priv,
					 u8 index, u32 *value)
{
	u16 regoffset_a[6] = {
		RTXAGC_A_RATE18_06, RTXAGC_A_RATE54_24,
		RTXAGC_A_MCS03_MCS00, RTXAGC_A_MCS07_MCS04,
		RTXAGC_A_MCS11_MCS08, RTXAGC_A_MCS15_MCS12
	};
	u16 regoffset_b[6] = {
		RTXAGC_B_RATE18_06, RTXAGC_B_RATE54_24,
		RTXAGC_B_MCS03_MCS00, RTXAGC_B_MCS07_MCS04,
		RTXAGC_B_MCS11_MCS08, RTXAGC_B_MCS15_MCS12
	};
	u8 i, rf, pwr_val[4];
	u32 writeval;
	u16 regoffset;

	for (rf = 0; rf < 2; rf++) {
		writeval = value[rf];
		for (i = 0; i < 4; i++) {
			pwr_val[i] = (u8)((writeval & (0x7f <<
					   (i * 8))) >> (i * 8));

			if (pwr_val[i] > RF6052_MAX_TX_PWR)
				pwr_val[i] = RF6052_MAX_TX_PWR;
		}
		writeval = (pwr_val[3] << 24) | (pwr_val[2] << 16) |
		    (pwr_val[1] << 8) | pwr_val[0];

		if (rf == 0)
			regoffset = regoffset_a[index];
		else
			regoffset = regoffset_b[index];
		rtl_set_bbreg(priv, regoffset, MASKDWORD, writeval);

	}
}

void rtl88e_phy_rf6052_set_ofdm_txpower(struct rtl8xxxu_priv *priv,
					u8 *ppowerlevel_ofdm,
					u8 *ppowerlevel_bw20,
					u8 *ppowerlevel_bw40, u8 channel)
{
	u32 writeval[2], powerbase0[2], powerbase1[2];
	u8 index;
	u8 direction;
	u32 pwrtrac_value = 0;

	_rtl88e_phy_get_power_base(priv, ppowerlevel_ofdm,
				  ppowerlevel_bw20, ppowerlevel_bw40,
				  channel, &powerbase0[0], &powerbase1[0]);

	/* 진짜 필요한건가?? */
	//rtl88e_dm_txpower_track_adjust(priv, 1, &direction, &pwrtrac_value);

	for (index = 0; index < 6; index++) {
		_rtl88e_get_txpower_writeval_by_regulatory(priv,
							   channel, index,
							   &powerbase0[0],
							   &powerbase1[0],
							   &writeval[0]);
		if (direction == 1) {
			writeval[0] += pwrtrac_value;
			writeval[1] += pwrtrac_value;
		} else if (direction == 2) {
			writeval[0] -= pwrtrac_value;
			writeval[1] -= pwrtrac_value;
		}
		_rtl88e_write_ofdm_power_reg(priv, index, &writeval[0]);
	}
}

int rtl8188eu_init_phy_rf(struct rtl8xxxu_priv *priv)
{
	return rtl8xxxu_init_phy_rf(priv, rtl8188eu_radioa_1t_init_table,
				    RF_A);
}

static void store_pwrindex_rate_offset(struct rtl8xxxu_priv *priv,
				       u32 regaddr, u32 bitmask,
				       u32 data)
{
	int count = rtlphy.pwrgroup_cnt;
#if 0
	if (regaddr == RTXAGC_A_RATE18_06)
		rtlphy.mcs_txpwrlevel_origoffset[count][0] = data;
	if (regaddr == RTXAGC_A_RATE54_24)
		rtlphy.mcs_txpwrlevel_origoffset[count][1] = data;
	if (regaddr == RTXAGC_A_CCK1_MCS32)
		rtlphy.mcs_txpwrlevel_origoffset[count][6] = data;
	if (regaddr == RTXAGC_B_CCK11_A_CCK2_11 && bitmask == 0xffffff00)
		rtlphy.mcs_txpwrlevel_origoffset[count][7] = data;
	if (regaddr == RTXAGC_A_MCS03_MCS00)
		rtlphy.mcs_txpwrlevel_origoffset[count][2] = data;
	if (regaddr == RTXAGC_A_MCS07_MCS04)
		rtlphy.mcs_txpwrlevel_origoffset[count][3] = data;
	if (regaddr == RTXAGC_A_MCS11_MCS08)
		rtlphy.mcs_txpwrlevel_origoffset[count][4] = data;
	if (regaddr == RTXAGC_A_MCS15_MCS12) {
		rtlphy.mcs_txpwrlevel_origoffset[count][5] = data;
		count++;
		rtlphy.pwrgroup_cnt = count;
	}
#endif
}

static bool phy_config_bb_with_pghdr(struct rtl8xxxu_priv *priv)
{
	int i;
	u32 *phy_reg_page;
	u16 phy_reg_page_len;
	u32 v1 = 0, v2 = 0, v3 = 0;

	phy_reg_page_len = RTL8188EUPHY_REG_ARRAY_PGLEN;
	phy_reg_page = RTL8188EUPHY_REG_ARRAY_PG;

	for (i = 0; i < phy_reg_page_len; i = i + 3) {
		v1 = phy_reg_page[i];
		v2 = phy_reg_page[i+1];
		v3 = phy_reg_page[i+2];

		store_pwrindex_rate_offset(priv, phy_reg_page[i],
				phy_reg_page[i + 1],
				phy_reg_page[i + 2]);
	}
	return true;
}

static void handle_path_a(u8 index,
			  u8 *cckpowerlevel, u8 *ofdmpowerlevel,
			  u8 *bw20powerlevel, u8 *bw40powerlevel)
{
	cckpowerlevel[RF90_PATH_A] =
	    rtlefuse.txpwrlevel_cck[RF90_PATH_A][index];
		/*-8~7 */
	if (rtlefuse.txpwr_ht20diff[RF90_PATH_A][index] > 0x0f)
		bw20powerlevel[RF90_PATH_A] =
		  rtlefuse.txpwrlevel_ht40_1s[RF90_PATH_A][index] -
		  (~(rtlefuse.txpwr_ht20diff[RF90_PATH_A][index]) + 1);
	else
		bw20powerlevel[RF90_PATH_A] =
		  rtlefuse.txpwrlevel_ht40_1s[RF90_PATH_A][index] +
		  rtlefuse.txpwr_ht20diff[RF90_PATH_A][index];
	if (rtlefuse.txpwr_legacyhtdiff[RF90_PATH_A][index] > 0xf)
		ofdmpowerlevel[RF90_PATH_A] =
		  rtlefuse.txpwrlevel_ht40_1s[RF90_PATH_A][index] -
		  (~(rtlefuse.txpwr_legacyhtdiff[RF90_PATH_A][index])+1);
	else
		ofdmpowerlevel[RF90_PATH_A] =
		rtlefuse.txpwrlevel_ht40_1s[RF90_PATH_A][index] +
		  rtlefuse.txpwr_legacyhtdiff[RF90_PATH_A][index];
	bw40powerlevel[RF90_PATH_A] =
	  rtlefuse.txpwrlevel_ht40_1s[RF90_PATH_A][index];
}

static void _rtl88e_get_txpower_index(struct rtl8xxxu_priv *priv, u8 channel,
				      u8 *cckpowerlevel, u8 *ofdmpowerlevel,
				      u8 *bw20powerlevel, u8 *bw40powerlevel)
{
	u8 index = (channel - 1);

	handle_path_a(index, cckpowerlevel,
			ofdmpowerlevel, bw20powerlevel,
			bw40powerlevel);

}

static void _rtl88e_ccxpower_index_check(struct rtl8xxxu_priv *priv,
					 u8 channel, u8 *cckpowerlevel,
					 u8 *ofdmpowerlevel, u8 *bw20powerlevel,
					 u8 *bw40powerlevel)
{
	rtlphy.cur_cck_txpwridx = cckpowerlevel[0];
	rtlphy.cur_ofdm24g_txpwridx = ofdmpowerlevel[0];
	rtlphy.cur_bw20_txpwridx = bw20powerlevel[0];
	rtlphy.cur_bw40_txpwridx = bw40powerlevel[0];

}

void rtl88e_phy_set_txpower_level(struct rtl8xxxu_priv *priv, int channel, bool ht40)
{
	u8 cckpowerlevel[MAX_TX_COUNT]  = {0};
	u8 ofdmpowerlevel[MAX_TX_COUNT] = {0};
	u8 bw20powerlevel[MAX_TX_COUNT] = {0};
	u8 bw40powerlevel[MAX_TX_COUNT] = {0};
#if 0
	/* 지우지 말것 */
	_rtl88e_get_txpower_index(priv, channel,
				  &cckpowerlevel[0], &ofdmpowerlevel[0],
				  &bw20powerlevel[0], &bw40powerlevel[0]);
	_rtl88e_ccxpower_index_check(priv, channel,
				     &cckpowerlevel[0], &ofdmpowerlevel[0],
				     &bw20powerlevel[0], &bw40powerlevel[0]);
	rtl88e_phy_rf6052_set_cck_txpower(priv, &cckpowerlevel[0]);
	rtl88e_phy_rf6052_set_ofdm_txpower(priv, &ofdmpowerlevel[0],
					   &bw20powerlevel[0],
					   &bw40powerlevel[0], channel);
#endif
}

void rtl88e_phy_set_bw_mode_callback(struct rtl8xxxu_priv *priv)
{
	u8 reg_bw_opmode;
	u8 reg_prsr_rsc;
#if 0
	RT_TRACE(priv, COMP_SCAN, DBG_TRACE,
		 "Switch to %s bandwidth\n",
		  rtlphy.current_chan_bw == HT_CHANNEL_WIDTH_20 ?
		  "20MHz" : "40MHz");
#endif

	reg_bw_opmode = rtl8xxxu_read8(priv, REG_BWOPMODE);
	reg_prsr_rsc = rtl8xxxu_read8(priv, REG_RRSR + 2);
	switch (rtlphy.current_chan_bw) {
	case HT_CHANNEL_WIDTH_20:
		reg_bw_opmode |= BW_OPMODE_20MHZ;
		rtl8xxxu_write8(priv, REG_BWOPMODE, reg_bw_opmode);
		break;
	case HT_CHANNEL_WIDTH_20_40:
#if 0
		/* 꼭 구현할것 */
		reg_bw_opmode &= ~BW_OPMODE_20MHZ;
		rtl8xxxu_write8(priv, REG_BWOPMODE, reg_bw_opmode);
		reg_prsr_rsc =
		    (reg_prsr_rsc & 0x90) | (1 << 5);
		rtl8xxxu_write8(priv, REG_RRSR + 2, reg_prsr_rsc);
#else
		reg_bw_opmode |= BW_OPMODE_20MHZ;
		rtl8xxxu_write8(priv, REG_BWOPMODE, reg_bw_opmode);
		break;

#endif
		break;
	default:
		RT_TRACE(priv, COMP_ERR, DBG_EMERG,
			 "unknown bandwidth: %#X\n", rtlphy.current_chan_bw);
		break;
	}

	switch (rtlphy.current_chan_bw) {
	case HT_CHANNEL_WIDTH_20:
		rtl_set_bbreg(priv, RFPGA0_RFMOD, BRFMOD, 0x0);
		rtl_set_bbreg(priv, RFPGA1_RFMOD, BRFMOD, 0x0);
	/*	rtl_set_bbreg(priv, RFPGA0_ANALOGPARAMETER2, BIT(10), 1);*/
		break;
	case HT_CHANNEL_WIDTH_20_40:
#if 0
		/* 꼭 구현할것 */
		rtl_set_bbreg(priv, RFPGA0_RFMOD, BRFMOD, 0x1);
		rtl_set_bbreg(priv, RFPGA1_RFMOD, BRFMOD, 0x1);
		rtl_set_bbreg(priv, RCCK0_SYSTEM, BCCK_SIDEBAND,
			      (mac->cur_40_prime_sc >> 1));
		rtl_set_bbreg(priv, ROFDM1_LSTF, 0xC00, mac->cur_40_prime_sc);
		/*rtl_set_bbreg(priv, RFPGA0_ANALOGPARAMETER2, BIT(10), 0);*/

		rtl_set_bbreg(priv, 0x818, (BIT(26) | BIT(27)),
			      (mac->cur_40_prime_sc ==
			       HAL_PRIME_CHNL_OFFSET_LOWER) ? 2 : 1);
#else
		rtl_set_bbreg(priv, RFPGA0_RFMOD, BRFMOD, 0x0);
		rtl_set_bbreg(priv, RFPGA1_RFMOD, BRFMOD, 0x0);

#endif
		break;
	default:
		RT_TRACE(priv, COMP_ERR, DBG_EMERG,
			 "unknown bandwidth: %#X\n", rtlphy.current_chan_bw);
		break;
	}
	rtl88e_phy_rf6052_set_bandwidth(priv, rtlphy.current_chan_bw);
	RT_TRACE(priv, COMP_SCAN, DBG_LOUD, "\n");
}

static bool _rtl88e_phy_set_sw_chnl_cmdarray(struct swchnlcmd *cmdtable,
					     u32 cmdtableidx, u32 cmdtablesz,
					     enum swchnlcmd_id cmdid,
					     u32 para1, u32 para2, u32 msdelay)
{
	struct swchnlcmd *pcmd;

	if (cmdtable == NULL) {
		return false;
	}

	if (cmdtableidx >= cmdtablesz)
		return false;

	pcmd = cmdtable + cmdtableidx;
	pcmd->cmdid = cmdid;
	pcmd->para1 = para1;
	pcmd->para2 = para2;
	pcmd->msdelay = msdelay;
	return true;
}

static bool _rtl88e_phy_sw_chnl_step_by_step(struct rtl8xxxu_priv *priv,
					     u8 channel, u8 *stage, u8 *step,
					     u32 *delay)
{
	struct swchnlcmd precommoncmd[MAX_PRECMD_CNT];
	u32 precommoncmdcnt;
	struct swchnlcmd postcommoncmd[MAX_POSTCMD_CNT];
	u32 postcommoncmdcnt;
	struct swchnlcmd rfdependcmd[MAX_RFDEPENDCMD_CNT];
	u32 rfdependcmdcnt;
	struct swchnlcmd *currentcmd = NULL;
	u8 rfpath;
	u8 num_total_rfpath = rtlphy.num_total_rfpath;

	precommoncmdcnt = 0;
	_rtl88e_phy_set_sw_chnl_cmdarray(precommoncmd, precommoncmdcnt++,
					 MAX_PRECMD_CNT,
					 CMDID_SET_TXPOWEROWER_LEVEL, 0, 0, 0);
	_rtl88e_phy_set_sw_chnl_cmdarray(precommoncmd, precommoncmdcnt++,
					 MAX_PRECMD_CNT, CMDID_END, 0, 0, 0);

	postcommoncmdcnt = 0;

	_rtl88e_phy_set_sw_chnl_cmdarray(postcommoncmd, postcommoncmdcnt++,
					 MAX_POSTCMD_CNT, CMDID_END, 0, 0, 0);

	rfdependcmdcnt = 0;

#if 0
	RT_ASSERT((channel >= 1 && channel <= 14),
		  "illegal channel for Zebra: %d\n", channel);
#endif

	_rtl88e_phy_set_sw_chnl_cmdarray(rfdependcmd, rfdependcmdcnt++,
					 MAX_RFDEPENDCMD_CNT, CMDID_RF_WRITEREG,
					 RF_CHNLBW, channel, 10);

	_rtl88e_phy_set_sw_chnl_cmdarray(rfdependcmd, rfdependcmdcnt++,
					 MAX_RFDEPENDCMD_CNT, CMDID_END, 0, 0,
					 0);

	do {
		switch (*stage) {
		case 0:
			currentcmd = &precommoncmd[*step];
			break;
		case 1:
			currentcmd = &rfdependcmd[*step];
			break;
		case 2:
			currentcmd = &postcommoncmd[*step];
			break;
		default:
			RT_TRACE(priv, COMP_ERR, DBG_EMERG,
				 "Invalid 'stage' = %d, Check it!\n", *stage);
			return true;
		}

		if (currentcmd->cmdid == CMDID_END) {
			if ((*stage) == 2)
				return true;
			(*stage)++;
			(*step) = 0;
			continue;
		}

		switch (currentcmd->cmdid) {
		case CMDID_SET_TXPOWEROWER_LEVEL:
			rtl88e_phy_set_txpower_level(priv, channel, false);
			break;
		case CMDID_WRITEPORT_ULONG:
			rtl8xxxu_write32(priv, currentcmd->para1,
					currentcmd->para2);
			break;
		case CMDID_WRITEPORT_USHORT:
			rtl8xxxu_write16(priv, currentcmd->para1,
				       (u16)currentcmd->para2);
			break;
		case CMDID_WRITEPORT_UCHAR:
			rtl8xxxu_write8(priv, currentcmd->para1,
				       (u8)currentcmd->para2);
			break;
		case CMDID_RF_WRITEREG:
			for (rfpath = 0; rfpath < num_total_rfpath; rfpath++) {
				rtlphy.rfreg_chnlval[rfpath] =
				    ((rtlphy.rfreg_chnlval[rfpath] &
				      0xfffffc00) | currentcmd->para2);

				rtl_set_rfreg(priv, (enum radio_path)rfpath,
					      currentcmd->para1,
					      RFREG_OFFSET_MASK,
					      rtlphy.rfreg_chnlval[rfpath]);
			}
			break;
		default:
			RT_TRACE(priv, COMP_ERR, DBG_LOUD,
				 "switch case not process\n");
			break;
		}

		break;
	} while (true);

	(*delay) = currentcmd->msdelay;
	(*step)++;
	return false;
}

static u8 _rtl88e_phy_path_a_iqk(struct rtl8xxxu_priv *priv, bool config_pathb)
{
	u32 reg_eac, reg_e94, reg_e9c, reg_ea4;
	u8 result = 0x00;

	rtl_set_bbreg(priv, 0xe30, MASKDWORD, 0x10008c1c);
	rtl_set_bbreg(priv, 0xe34, MASKDWORD, 0x30008c1c);
	rtl_set_bbreg(priv, 0xe38, MASKDWORD, 0x8214032a);
	rtl_set_bbreg(priv, 0xe3c, MASKDWORD, 0x28160000);

	rtl_set_bbreg(priv, 0xe4c, MASKDWORD, 0x00462911);
	rtl_set_bbreg(priv, 0xe48, MASKDWORD, 0xf9000000);
	rtl_set_bbreg(priv, 0xe48, MASKDWORD, 0xf8000000);

	mdelay(IQK_DELAY_TIME);

	reg_eac = rtl_get_bbreg(priv, 0xeac, MASKDWORD);
	reg_e94 = rtl_get_bbreg(priv, 0xe94, MASKDWORD);
	reg_e9c = rtl_get_bbreg(priv, 0xe9c, MASKDWORD);
	reg_ea4 = rtl_get_bbreg(priv, 0xea4, MASKDWORD);

	if (!(reg_eac & BIT(28)) &&
	    (((reg_e94 & 0x03FF0000) >> 16) != 0x142) &&
	    (((reg_e9c & 0x03FF0000) >> 16) != 0x42))
		result |= 0x01;
	return result;
}

static u8 _rtl88e_phy_path_a_rx_iqk(struct rtl8xxxu_priv *priv, bool config_pathb)
{
	u32 reg_eac, reg_e94, reg_e9c, reg_ea4, u32temp;
	u8 result = 0x00;

	/*Get TXIMR Setting*/
	/*Modify RX IQK mode table*/
	rtl_set_bbreg(priv, RFPGA0_IQK, MASKDWORD, 0x00000000);
	rtl_set_rfreg(priv, RF90_PATH_A, RF_WE_LUT, RFREG_OFFSET_MASK, 0x800a0);
	rtl_set_rfreg(priv, RF90_PATH_A, RF_RCK_OS, RFREG_OFFSET_MASK, 0x30000);
	rtl_set_rfreg(priv, RF90_PATH_A, RF_TXPA_G1, RFREG_OFFSET_MASK, 0x0000f);
	rtl_set_rfreg(priv, RF90_PATH_A, RF_TXPA_G2, RFREG_OFFSET_MASK, 0xf117b);
	rtl_set_bbreg(priv, RFPGA0_IQK, MASKDWORD, 0x80800000);

	/*IQK Setting*/
	rtl_set_bbreg(priv, RTX_IQK, MASKDWORD, 0x01007c00);
	rtl_set_bbreg(priv, RRX_IQK, MASKDWORD, 0x81004800);

	/*path a IQK setting*/
	rtl_set_bbreg(priv, RTX_IQK_TONE_A, MASKDWORD, 0x10008c1c);
	rtl_set_bbreg(priv, RRX_IQK_TONE_A, MASKDWORD, 0x30008c1c);
	rtl_set_bbreg(priv, RTX_IQK_PI_A, MASKDWORD, 0x82160804);
	rtl_set_bbreg(priv, RRX_IQK_PI_A, MASKDWORD, 0x28160000);

	/*LO calibration Setting*/
	rtl_set_bbreg(priv, RIQK_AGC_RSP, MASKDWORD, 0x0046a911);
	/*one shot,path A LOK & iqk*/
	rtl_set_bbreg(priv, RIQK_AGC_PTS, MASKDWORD, 0xf9000000);
	rtl_set_bbreg(priv, RIQK_AGC_PTS, MASKDWORD, 0xf8000000);

	mdelay(IQK_DELAY_TIME);

	reg_eac = rtl_get_bbreg(priv, RRX_POWER_AFTER_IQK_A_2, MASKDWORD);
	reg_e94 = rtl_get_bbreg(priv, RTX_POWER_BEFORE_IQK_A, MASKDWORD);
	reg_e9c = rtl_get_bbreg(priv, RTX_POWER_AFTER_IQK_A, MASKDWORD);


	if (!(reg_eac & BIT(28)) &&
	    (((reg_e94 & 0x03FF0000) >> 16) != 0x142) &&
	    (((reg_e9c & 0x03FF0000) >> 16) != 0x42))
		result |= 0x01;
	else
		return result;

	u32temp = 0x80007C00 | (reg_e94&0x3FF0000) |
		  ((reg_e9c&0x3FF0000) >> 16);
	rtl_set_bbreg(priv, RTX_IQK, MASKDWORD, u32temp);
	/*RX IQK*/
	/*Modify RX IQK mode table*/
	rtl_set_bbreg(priv, RFPGA0_IQK, MASKDWORD, 0x00000000);
	rtl_set_rfreg(priv, RF90_PATH_A, RF_WE_LUT, RFREG_OFFSET_MASK, 0x800a0);
	rtl_set_rfreg(priv, RF90_PATH_A, RF_RCK_OS, RFREG_OFFSET_MASK, 0x30000);
	rtl_set_rfreg(priv, RF90_PATH_A, RF_TXPA_G1, RFREG_OFFSET_MASK, 0x0000f);
	rtl_set_rfreg(priv, RF90_PATH_A, RF_TXPA_G2, RFREG_OFFSET_MASK, 0xf7ffa);
	rtl_set_bbreg(priv, RFPGA0_IQK, MASKDWORD, 0x80800000);

	/*IQK Setting*/
	rtl_set_bbreg(priv, RRX_IQK, MASKDWORD, 0x01004800);

	/*path a IQK setting*/
	rtl_set_bbreg(priv, RTX_IQK_TONE_A, MASKDWORD, 0x30008c1c);
	rtl_set_bbreg(priv, RRX_IQK_TONE_A, MASKDWORD, 0x10008c1c);
	rtl_set_bbreg(priv, RTX_IQK_PI_A, MASKDWORD, 0x82160c05);
	rtl_set_bbreg(priv, RRX_IQK_PI_A, MASKDWORD, 0x28160c05);

	/*LO calibration Setting*/
	rtl_set_bbreg(priv, RIQK_AGC_RSP, MASKDWORD, 0x0046a911);
	/*one shot,path A LOK & iqk*/
	rtl_set_bbreg(priv, RIQK_AGC_PTS, MASKDWORD, 0xf9000000);
	rtl_set_bbreg(priv, RIQK_AGC_PTS, MASKDWORD, 0xf8000000);

	mdelay(IQK_DELAY_TIME);

	reg_eac = rtl_get_bbreg(priv, RRX_POWER_AFTER_IQK_A_2, MASKDWORD);
	reg_e94 = rtl_get_bbreg(priv, RTX_POWER_BEFORE_IQK_A, MASKDWORD);
	reg_e9c = rtl_get_bbreg(priv, RTX_POWER_AFTER_IQK_A, MASKDWORD);
	reg_ea4 = rtl_get_bbreg(priv, RRX_POWER_BEFORE_IQK_A_2, MASKDWORD);

	if (!(reg_eac & BIT(27)) &&
	    (((reg_ea4 & 0x03FF0000) >> 16) != 0x132) &&
	    (((reg_eac & 0x03FF0000) >> 16) != 0x36))
		result |= 0x02;
	return result;
}

static void _rtl88e_phy_path_a_fill_iqk_matrix(struct rtl8xxxu_priv *priv,
					       bool iqk_ok, long result[][8],
					       u8 final_candidate, bool btxonly)
{
	u32 oldval_0, x, tx0_a, reg;
	long y, tx0_c;

	if (final_candidate == 0xFF) {
		return;
	} else if (iqk_ok) {
		oldval_0 = (rtl_get_bbreg(priv, ROFDM0_XATXIQIMBALANCE,
					  MASKDWORD) >> 22) & 0x3FF;
		x = result[final_candidate][0];
		if ((x & 0x00000200) != 0)
			x = x | 0xFFFFFC00;
		tx0_a = (x * oldval_0) >> 8;
		rtl_set_bbreg(priv, ROFDM0_XATXIQIMBALANCE, 0x3FF, tx0_a);
		rtl_set_bbreg(priv, ROFDM0_ECCATHRESHOLD, BIT(31),
			      ((x * oldval_0 >> 7) & 0x1));
		y = result[final_candidate][1];
		if ((y & 0x00000200) != 0)
			y = y | 0xFFFFFC00;
		tx0_c = (y * oldval_0) >> 8;
		rtl_set_bbreg(priv, ROFDM0_XCTXAFE, 0xF0000000,
			      ((tx0_c & 0x3C0) >> 6));
		rtl_set_bbreg(priv, ROFDM0_XATXIQIMBALANCE, 0x003F0000,
			      (tx0_c & 0x3F));
		rtl_set_bbreg(priv, ROFDM0_ECCATHRESHOLD, BIT(29),
			      ((y * oldval_0 >> 7) & 0x1));
		if (btxonly)
			return;
		reg = result[final_candidate][2];
		rtl_set_bbreg(priv, ROFDM0_XARXIQIMBALANCE, 0x3FF, reg);
		reg = result[final_candidate][3] & 0x3F;
		rtl_set_bbreg(priv, ROFDM0_XARXIQIMBALANCE, 0xFC00, reg);
		reg = (result[final_candidate][3] >> 6) & 0xF;
		rtl_set_bbreg(priv, 0xca0, 0xF0000000, reg);
	}
}

static void _rtl88e_phy_save_adda_registers(struct rtl8xxxu_priv *priv,
					    u32 *addareg, u32 *addabackup,
					    u32 registernum)
{
	u32 i;

	for (i = 0; i < registernum; i++)
		addabackup[i] = rtl_get_bbreg(priv, addareg[i], MASKDWORD);
}

static void _rtl88e_phy_save_mac_registers(struct rtl8xxxu_priv *priv,
					   u32 *macreg, u32 *macbackup)
{
	u32 i;

	for (i = 0; i < (IQK_MAC_REG_NUM - 1); i++)
		macbackup[i] = rtl8xxxu_read8(priv, macreg[i]);
	macbackup[i] = rtl8xxxu_read32(priv, macreg[i]);
}

static void _rtl88e_phy_reload_adda_registers(struct rtl8xxxu_priv *priv,
					      u32 *addareg, u32 *addabackup,
					      u32 regiesternum)
{
	u32 i;

	for (i = 0; i < regiesternum; i++)
		rtl_set_bbreg(priv, addareg[i], MASKDWORD, addabackup[i]);
}

static void _rtl88e_phy_reload_mac_registers(struct rtl8xxxu_priv *priv,
					     u32 *macreg, u32 *macbackup)
{
	u32 i;

	for (i = 0; i < (IQK_MAC_REG_NUM - 1); i++)
		rtl8xxxu_write8(priv, macreg[i], (u8) macbackup[i]);
	rtl8xxxu_write32(priv, macreg[i], macbackup[i]);
}

static void _rtl88e_phy_path_adda_on(struct rtl8xxxu_priv *priv,
				     u32 *addareg, bool is_patha_on, bool is2t)
{
	u32 pathon;
	u32 i;

	pathon = is_patha_on ? 0x04db25a4 : 0x0b1b25a4;
	if (false == is2t) {
		pathon = 0x0bdb25a0;
		rtl_set_bbreg(priv, addareg[0], MASKDWORD, 0x0b1b25a0);
	} else {
		rtl_set_bbreg(priv, addareg[0], MASKDWORD, pathon);
	}

	for (i = 1; i < IQK_ADDA_REG_NUM; i++)
		rtl_set_bbreg(priv, addareg[i], MASKDWORD, pathon);
}

static void _rtl88e_phy_mac_setting_calibration(struct rtl8xxxu_priv *priv,
						u32 *macreg, u32 *macbackup)
{
	u32 i = 0;

	rtl8xxxu_write8(priv, macreg[i], 0x3F);

	for (i = 1; i < (IQK_MAC_REG_NUM - 1); i++)
		rtl8xxxu_write8(priv, macreg[i],
			       (u8) (macbackup[i] & (~BIT(3))));
	rtl8xxxu_write8(priv, macreg[i], (u8) (macbackup[i] & (~BIT(5))));
}

static void _rtl88e_phy_pi_mode_switch(struct rtl8xxxu_priv *priv, bool pi_mode)
{
	u32 mode;

	mode = pi_mode ? 0x01000100 : 0x01000000;
	rtl_set_bbreg(priv, 0x820, MASKDWORD, mode);
	rtl_set_bbreg(priv, 0x828, MASKDWORD, mode);
}

static bool _rtl88e_phy_simularity_compare(struct rtl8xxxu_priv *priv,
					   long result[][8], u8 c1, u8 c2)
{
	u32 i, j, diff, simularity_bitmap, bound;
	u8 final_candidate[2] = { 0xFF, 0xFF };
	int is2t = false;
	bool bresult = true;
	bound = 4;

	simularity_bitmap = 0;

	for (i = 0; i < bound; i++) {
		diff = (result[c1][i] > result[c2][i]) ?
		    (result[c1][i] - result[c2][i]) :
		    (result[c2][i] - result[c1][i]);

		if (diff > MAX_TOLERANCE) {
			if ((i == 2 || i == 6) && !simularity_bitmap) {
				if (result[c1][i] + result[c1][i + 1] == 0)
					final_candidate[(i / 4)] = c2;
				else if (result[c2][i] + result[c2][i + 1] == 0)
					final_candidate[(i / 4)] = c1;
				else
					simularity_bitmap = simularity_bitmap |
					    (1 << i);
			} else
				simularity_bitmap =
				    simularity_bitmap | (1 << i);
		}
	}

	if (simularity_bitmap == 0) {
		for (i = 0; i < (bound / 4); i++) {
			if (final_candidate[i] != 0xFF) {
				for (j = i * 4; j < (i + 1) * 4 - 2; j++)
					result[3][j] =
					    result[final_candidate[i]][j];
				bresult = false;
			}
		}
		return bresult;
	} else if (!(simularity_bitmap & 0x0F)) {
		for (i = 0; i < 4; i++)
			result[3][i] = result[c1][i];
		return false;
	} else if (!(simularity_bitmap & 0xF0) && is2t) {
		for (i = 4; i < 8; i++)
			result[3][i] = result[c1][i];
		return false;
	} else {
		return false;
	}

}

static void _rtl88e_phy_iq_calibrate(struct rtl8xxxu_priv *priv,
				     long result[][8], u8 t, bool is2t)
{
	u32 i;
	u8 patha_ok;
	u32 adda_reg[IQK_ADDA_REG_NUM] = {
		0x85c, 0xe6c, 0xe70, 0xe74,
		0xe78, 0xe7c, 0xe80, 0xe84,
		0xe88, 0xe8c, 0xed0, 0xed4,
		0xed8, 0xedc, 0xee0, 0xeec
	};
	u32 iqk_mac_reg[IQK_MAC_REG_NUM] = {
		0x522, 0x550, 0x551, 0x040
	};
	u32 iqk_bb_reg[IQK_BB_REG_NUM] = {
		ROFDM0_TRXPATHENABLE, ROFDM0_TRMUXPAR,
		RFPGA0_XCD_RFINTERFACESW, 0xb68, 0xb6c,
		0x870, 0x860, 0x864, 0x800
	};
	const u32 retrycount = 2;

	if (t == 0) {
		_rtl88e_phy_save_adda_registers(priv, adda_reg,
						priv->adda_backup, 16);
		_rtl88e_phy_save_mac_registers(priv, iqk_mac_reg,
					       priv->mac_backup);
		_rtl88e_phy_save_adda_registers(priv, iqk_bb_reg,
						priv->bb_backup,
						IQK_BB_REG_NUM);
	}
	_rtl88e_phy_path_adda_on(priv, adda_reg, true, is2t);
	if (t == 0) {
		priv->pi_enabled =
		  (u8)rtl_get_bbreg(priv, RFPGA0_XA_HSSIPARAMETER1, BIT(8));
	}

	if (!priv->pi_enabled)
		_rtl88e_phy_pi_mode_switch(priv, true);
	/*BB Setting*/
	rtl_set_bbreg(priv, 0x800, BIT(24), 0x00);
	rtl_set_bbreg(priv, 0xc04, MASKDWORD, 0x03a05600);
	rtl_set_bbreg(priv, 0xc08, MASKDWORD, 0x000800e4);
	rtl_set_bbreg(priv, 0x874, MASKDWORD, 0x22204000);

	rtl_set_bbreg(priv, 0x870, BIT(10), 0x01);
	rtl_set_bbreg(priv, 0x870, BIT(26), 0x01);
	rtl_set_bbreg(priv, 0x860, BIT(10), 0x00);
	rtl_set_bbreg(priv, 0x864, BIT(10), 0x00);

	if (is2t) {
		rtl_set_bbreg(priv, 0x840, MASKDWORD, 0x00010000);
		rtl_set_bbreg(priv, 0x844, MASKDWORD, 0x00010000);
	}
	_rtl88e_phy_mac_setting_calibration(priv, iqk_mac_reg,
					    priv->mac_backup);
	rtl_set_bbreg(priv, 0xb68, MASKDWORD, 0x0f600000);
	if (is2t)
		rtl_set_bbreg(priv, 0xb6c, MASKDWORD, 0x0f600000);

	rtl_set_bbreg(priv, 0xe28, MASKDWORD, 0x80800000);
	rtl_set_bbreg(priv, 0xe40, MASKDWORD, 0x01007c00);
	rtl_set_bbreg(priv, 0xe44, MASKDWORD, 0x81004800);
	for (i = 0; i < retrycount; i++) {
		patha_ok = _rtl88e_phy_path_a_iqk(priv, is2t);
		if (patha_ok == 0x01) {
			RT_TRACE(priv, COMP_INIT, DBG_LOUD,
				 "Path A Tx IQK Success!!\n");
			result[t][0] = (rtl_get_bbreg(priv, 0xe94, MASKDWORD) &
					0x3FF0000) >> 16;
			result[t][1] = (rtl_get_bbreg(priv, 0xe9c, MASKDWORD) &
					0x3FF0000) >> 16;
			break;
		}
	}

	for (i = 0; i < retrycount; i++) {
		patha_ok = _rtl88e_phy_path_a_rx_iqk(priv, is2t);
		if (patha_ok == 0x03) {
			RT_TRACE(priv, COMP_INIT, DBG_LOUD,
				 "Path A Rx IQK Success!!\n");
			result[t][2] = (rtl_get_bbreg(priv, 0xea4, MASKDWORD) &
					0x3FF0000) >> 16;
			result[t][3] = (rtl_get_bbreg(priv, 0xeac, MASKDWORD) &
					0x3FF0000) >> 16;
			break;
		} else {
			RT_TRACE(priv, COMP_INIT, DBG_LOUD,
				 "Path a RX iqk fail!!!\n");
		}
	}

	if (0 == patha_ok)
		RT_TRACE(priv, COMP_INIT, DBG_LOUD,
			 "Path A IQK Success!!\n");
	
	rtl_set_bbreg(priv, 0xe28, MASKDWORD, 0);

	if (t != 0) {
		if (!priv->pi_enabled)
			_rtl88e_phy_pi_mode_switch(priv, false);
		_rtl88e_phy_reload_adda_registers(priv, adda_reg,
						  priv->adda_backup, 16);
		_rtl88e_phy_reload_mac_registers(priv, iqk_mac_reg,
						 priv->mac_backup);
		_rtl88e_phy_reload_adda_registers(priv, iqk_bb_reg,
						  priv->bb_backup,
						  IQK_BB_REG_NUM);

		rtl_set_bbreg(priv, 0x840, MASKDWORD, 0x00032ed3);
		if (is2t)
			rtl_set_bbreg(priv, 0x844, MASKDWORD, 0x00032ed3);
		rtl_set_bbreg(priv, 0xe30, MASKDWORD, 0x01008c00);
		rtl_set_bbreg(priv, 0xe34, MASKDWORD, 0x01008c00);
	}
	RT_TRACE(priv, COMP_INIT, DBG_LOUD, "88ee IQK Finish!!\n");
}

static void _rtl88e_phy_lc_calibrate(struct rtl8xxxu_priv *priv, bool is2t)
{
	u8 tmpreg;
	u32 rf_a_mode = 0, rf_b_mode = 0, lc_cal;

	tmpreg = rtl8xxxu_read8(priv, 0xd03);

	if ((tmpreg & 0x70) != 0)
		rtl8xxxu_write8(priv, 0xd03, tmpreg & 0x8F);
	else
		rtl8xxxu_write8(priv, REG_TXPAUSE, 0xFF);

	if ((tmpreg & 0x70) != 0) {
		rf_a_mode = rtl_get_rfreg(priv, RF90_PATH_A, 0x00, MASK12BITS);

		if (is2t)
			rf_b_mode = rtl_get_rfreg(priv, RF90_PATH_B, 0x00,
						  MASK12BITS);

		rtl_set_rfreg(priv, RF90_PATH_A, 0x00, MASK12BITS,
			      (rf_a_mode & 0x8FFFF) | 0x10000);

		if (is2t)
			rtl_set_rfreg(priv, RF90_PATH_B, 0x00, MASK12BITS,
				      (rf_b_mode & 0x8FFFF) | 0x10000);
	}
	lc_cal = rtl_get_rfreg(priv, RF90_PATH_A, 0x18, MASK12BITS);

	rtl_set_rfreg(priv, RF90_PATH_A, 0x18, MASK12BITS, lc_cal | 0x08000);

	mdelay(100);

	if ((tmpreg & 0x70) != 0) {
		rtl8xxxu_write8(priv, 0xd03, tmpreg);
		rtl_set_rfreg(priv, RF90_PATH_A, 0x00, MASK12BITS, rf_a_mode);

		if (is2t)
			rtl_set_rfreg(priv, RF90_PATH_B, 0x00, MASK12BITS,
				      rf_b_mode);
	} else {
		rtl8xxxu_write8(priv, REG_TXPAUSE, 0x00);
	}
	RT_TRACE(priv, COMP_INIT, DBG_LOUD, "\n");

}

static void _rtl88e_phy_set_rfpath_switch(struct rtl8xxxu_priv *priv,
					  bool bmain, bool is2t)
{
	RT_TRACE(priv, COMP_INIT, DBG_LOUD, "\n");

#if 0
	if (is_hal_stop(rtlhal)) {
		u8 u1btmp;
		u1btmp = rtl8xxxu_read8(priv, REG_LEDCFG0);
		rtl8xxxu_write8(priv, REG_LEDCFG0, u1btmp | BIT(7));
		rtl_set_bbreg(priv, RFPGA0_XAB_RFPARAMETER, BIT(13), 0x01);
	}
#endif
	rtl_set_bbreg(priv, RFPGA0_XAB_RFINTERFACESW, BIT(8) | BIT(9), 0);
	rtl_set_bbreg(priv, 0x914, MASKLWORD, 0x0201);

	/* We use the RF definition of MAIN and AUX,
	 * left antenna and right antenna repectively.
	 * Default output at AUX.
	 */
	if (bmain) {
		rtl_set_bbreg(priv, RFPGA0_XA_RFINTERFACEOE,
				BIT(14) | BIT(13) | BIT(12), 0);
		rtl_set_bbreg(priv, RFPGA0_XB_RFINTERFACEOE,
				BIT(5) | BIT(4) | BIT(3), 0);
		if (rtlefuse.antenna_div_type == CGCS_RX_HW_ANTDIV)
			rtl_set_bbreg(priv, RCONFIG_RAM64x16, BIT(31), 0);
	} else {
		rtl_set_bbreg(priv, RFPGA0_XA_RFINTERFACEOE,
				BIT(14) | BIT(13) | BIT(12), 1);
		rtl_set_bbreg(priv, RFPGA0_XB_RFINTERFACEOE,
				BIT(5) | BIT(4) | BIT(3), 1);
		if (rtlefuse.antenna_div_type == CGCS_RX_HW_ANTDIV)
			rtl_set_bbreg(priv, RCONFIG_RAM64x16, BIT(31), 1);
	}
}

static void rtl88ee_phy_set_rf_on(struct rtl8xxxu_priv *priv)
{

	rtl8xxxu_write8(priv, REG_SPS0_CTRL, 0x2b);
	rtl8xxxu_write8(priv, REG_SYS_FUNC_EN, 0xE3);
	/*rtl8xxxu_write8(priv, REG_APSD_CTRL, 0x00);*/
	rtl8xxxu_write8(priv, REG_SYS_FUNC_EN, 0xE2);
	rtl8xxxu_write8(priv, REG_SYS_FUNC_EN, 0xE3);
	rtl8xxxu_write8(priv, REG_TXPAUSE, 0x00);
}

static void _rtl88ee_phy_set_rf_sleep(struct rtl8xxxu_priv *priv)
{

	rtl8xxxu_write8(priv, REG_TXPAUSE, 0xFF);
	rtl_set_rfreg(priv, RF90_PATH_A, 0x00, RFREG_OFFSET_MASK, 0x00);
	rtl8xxxu_write8(priv, REG_SYS_FUNC_EN, 0xE2);
	rtl8xxxu_write8(priv, REG_SPS0_CTRL, 0x22);
}

void rtl88e_phy_sw_chnl_callback(struct rtl8xxxu_priv *priv)
{
	u32 delay;

	RT_TRACE(priv, COMP_SCAN, DBG_TRACE,
		 "switch to channel%d\n", rtlphy.current_channel);
#if 0
	if (is_hal_stop(rtlhal))
		return;
#endif
	do {
		if (!rtlphy.sw_chnl_inprogress)
			break;
		if (!_rtl88e_phy_sw_chnl_step_by_step
		    (priv, rtlphy.current_channel, &rtlphy.sw_chnl_stage,
		     &rtlphy.sw_chnl_step, &delay)) {
			if (delay > 0)
				mdelay(delay);
			else
				continue;
		} else {
			rtlphy.sw_chnl_inprogress = false;
		}
		break;
	} while (true);
	RT_TRACE(priv, COMP_SCAN, DBG_TRACE, "\n");
}

/* 테스트 hw_init후 바로 지울것 */
#if 1
static int
rtl8188eu_init_mac(struct rtl8xxxu_priv *priv)
{
	struct rtl8xxxu_reg8val *array = priv->fops->mactable;
	int i, ret;
	u16 reg;
	u8 val;

	for (i = 0; ; i++) {
		reg = array[i].reg;
		val = array[i].val;

		if (reg == 0xffff && val == 0xff)
			break;

		ret = rtl8xxxu_write8(priv, reg, val);
		if (ret != 1) {
			dev_warn(&priv->udev->dev,
				 "Failed to initialize MAC "
				 "(reg: %04x, val %02x)\n", reg, val);
			return -EAGAIN;
		}
	}

	if (priv->rtl_chip != RTL8723B && priv->rtl_chip != RTL8192E)
		rtl8xxxu_write8(priv, REG_MAX_AGGR_NUM, 0x0a);
	else if(priv->rtl_chip == RTL8188E)
		rtl8xxxu_write8(priv, REG_MAX_AGGR_NUM, 0x0b);


	return 0;
}
#endif

void rtl88e_phy_set_bw_mode(struct rtl8xxxu_priv *priv,
			    enum nl80211_channel_type ch_type)
{
	if (rtlphy.set_bwmode_inprogress)
		return;
	rtlphy.set_bwmode_inprogress = true;
	rtlphy.current_chan_bw = ch_type;
	rtl88e_phy_set_bw_mode_callback(priv);
}

void rtl88e_phy_lc_calibrate(struct rtl8xxxu_priv *priv, bool is2t)
{
	u32 timeout = 2000, timecount = 0;
#if 0
	while (rtlpriv->mac80211.act_scanning && timecount < timeout) {
		udelay(50);
		timecount += 50;
	}
#endif

	rtlphy.lck_inprogress = true;

	_rtl88e_phy_lc_calibrate(priv, false);

	rtlphy.lck_inprogress = false;
}

void rtl88e_phy_iq_calibrate(struct rtl8xxxu_priv *priv)
{
	long result[4][8];
	u8 i, final_candidate;
	bool b_patha_ok, b_pathb_ok;
	long reg_e94, reg_e9c, reg_ea4, reg_eac, reg_eb4, reg_ebc, reg_ec4,
	    reg_ecc, reg_tmp = 0;
	bool is12simular, is13simular, is23simular;
	u32 iqk_bb_reg[9] = {
		ROFDM0_XARXIQIMBALANCE,
		ROFDM0_XBRXIQIMBALANCE,
		ROFDM0_ECCATHRESHOLD,
		ROFDM0_AGCRSSITABLE,
		ROFDM0_XATXIQIMBALANCE,
		ROFDM0_XBTXIQIMBALANCE,
		ROFDM0_XCTXAFE,
		ROFDM0_XDTXAFE,
		ROFDM0_RXIQEXTANTA
	};

#if 0
	/* 추구 구현할 것 */
	if (b_recovery) {
		_rtl88e_phy_reload_adda_registers(hw,
						  iqk_bb_reg,
						  rtlphy.iqk_bb_backup, 9);
		return;
	}
#else
	if (rtlphy.iqk_initialized) {
		_rtl88e_phy_reload_adda_registers(priv,
						  iqk_bb_reg,
						  rtlphy.iqk_bb_backup, 9);
		return;
	}
#endif

	for (i = 0; i < 8; i++) {
		result[0][i] = 0;
		result[1][i] = 0;
		result[2][i] = 0;
		result[3][i] = 0;
	}
	final_candidate = 0xff;
	b_patha_ok = false;
	b_pathb_ok = false;
	is12simular = false;
	is23simular = false;
	is13simular = false;
	for (i = 0; i < 3; i++) {
		_rtl88e_phy_iq_calibrate(priv, result, i, false);
		
		if (i == 1) {
			is12simular =
			  _rtl88e_phy_simularity_compare(priv, result, 0, 1);
			if (is12simular) {
				final_candidate = 0;
				break;
			}
		}
		if (i == 2) {
			is13simular =
			  _rtl88e_phy_simularity_compare(priv, result, 0, 2);
			if (is13simular) {
				final_candidate = 0;
				break;
			}
			is23simular =
			   _rtl88e_phy_simularity_compare(priv, result, 1, 2);
			if (is23simular) {
				final_candidate = 1;
			} else {
				for (i = 0; i < 8; i++)
					reg_tmp += result[3][i];

				if (reg_tmp != 0)
					final_candidate = 3;
				else
					final_candidate = 0xFF;
			}
		}
	}
	for (i = 0; i < 4; i++) {
		reg_e94 = result[i][0];
		reg_e9c = result[i][1];
		reg_ea4 = result[i][2];
		reg_eac = result[i][3];
		reg_eb4 = result[i][4];
		reg_ebc = result[i][5];
		reg_ec4 = result[i][6];
		reg_ecc = result[i][7];
	}
	if (final_candidate != 0xff) {
		reg_e94 = result[final_candidate][0];
		reg_e9c = result[final_candidate][1];
		reg_ea4 = result[final_candidate][2];
		reg_eac = result[final_candidate][3];
		reg_eb4 = result[final_candidate][4];
		reg_ebc = result[final_candidate][5];
		reg_ec4 = result[final_candidate][6];
		reg_ecc = result[final_candidate][7];
		priv->regeb4 = reg_eb4;
		priv->regebc = reg_ebc;
		priv->rege94 = reg_e94;
		priv->rege9c = reg_e9c;
		b_patha_ok = true;
		b_pathb_ok = true;
	} else {
		priv->rege94 = 0x100;
		priv->regeb4 = 0x100;
		priv->rege9c = 0x0;
		priv->regebc = 0x0;
	}
	if (reg_e94 != 0) /*&&(reg_ea4 != 0) */
		_rtl88e_phy_path_a_fill_iqk_matrix(priv, b_patha_ok, result,
						   final_candidate,
						   (reg_ea4 == 0));
	/* 확인하고 구현할것 */
	if (final_candidate != 0xFF) {
		for (i = 0; i < IQK_MATRIX_REG_NUM; i++)
			rtlphy.iqk_matrix[0].value[0][i] =
				result[final_candidate][i];
		rtlphy.iqk_matrix[0].iqk_done = true;

	}
	_rtl88e_phy_save_adda_registers(priv, iqk_bb_reg,
					priv->bb_backup, 9);
}

void rtl88e_phy_set_rfpath_switch(struct rtl8xxxu_priv *priv, bool bmain)
{
	_rtl88e_phy_set_rfpath_switch(priv, bmain, false);
}

inline u32 rtl_systime_to_ms(u32 systime)
{
	return systime * 1000 / HZ;
}

inline s32 rtl_get_passing_time_ms(u32 start)
{
	return rtl_systime_to_ms(jiffies-start);
}

static void _rtl88eu_reset_8051(struct rtl8xxxu_priv *priv)
{
	u8 value8;

	value8 = rtl8xxxu_read8(priv, REG_SYS_FUNC_EN + 1);
	rtl8xxxu_write8(priv, REG_SYS_FUNC_EN+1, value8 & (~BIT(2)));
	rtl8xxxu_write8(priv, REG_SYS_FUNC_EN+1, value8 | (BIT(2)));
}

static void _rtl88eu_set_bcn_ctrl_reg(struct rtl8xxxu_priv *priv,
				      u8 set_bits, u8 clear_bits)
{
	reg_bcn_ctrl_val |= set_bits;
	reg_bcn_ctrl_val &= ~clear_bits;

	rtl8xxxu_write8(priv, REG_BCN_CTRL, (u8) reg_bcn_ctrl_val);
}

void rtl88eu_read_chip_version(struct rtl8xxxu_priv *priv)
{
	u32 value32;
	enum version_8188e version;

	value32 = rtl8xxxu_read32(priv, REG_SYS_CFG);
	version = VERSION_NORMAL_CHIP_88E;/* TODO */
	rtlphy.rf_type = RF_1T1R;
}

static void rtl88eu_hal_notch_filter(struct rtl8xxxu_priv *priv, bool enable)
{

	if (enable) {
		rtl8xxxu_write8(priv, ROFDM0_RXDSP+1,
			       rtl8xxxu_read8(priv, ROFDM0_RXDSP + 1) |
					     BIT(1));
	} else {
		rtl8xxxu_write8(priv, ROFDM0_RXDSP+1,
			       rtl8xxxu_read8(priv, ROFDM0_RXDSP + 1) &
					     ~BIT(1));
	}
}

static s32 _rtl88eu_llt_write(struct rtl8xxxu_priv *priv, u32 address, u32 data)
{
	s32 status = true;
	s32 count = 0;
	u32 value = _LLT_INIT_ADDR(address) | _LLT_INIT_DATA(data) |
		    _LLT_OP(_LLT_WRITE_ACCESS);
	u16 llt_reg = REG_LLT_INIT;

	rtl8xxxu_write32(priv, llt_reg, value);

	do {
		value = rtl8xxxu_read32(priv, llt_reg);
		if (_LLT_NO_ACTIVE == _LLT_OP_VALUE(value))
			break;

		if (count > POLLING_LLT_THRESHOLD) {
			RT_TRACE(priv, COMP_ERR, DBG_EMERG,
				 "Failed to polling write LLT done at address %d!\n", address);
			status = false;
			break;
		}
	} while (count++);

	return status;
}

int rtl88eu_init_llt_table(struct rtl8xxxu_priv *priv, u8 boundary)
{
	s32 status = false;
	u32 i;
	u32 last_entry_of_tx_pkt_buf = LAST_ENTRY_OF_TX_PKT_BUFFER;

	for (i = 0; i < (boundary - 1); i++) {
		status = _rtl88eu_llt_write(priv, i, i + 1);
		if (true != status)
			return status;
	}

	/* end of list */
	status = _rtl88eu_llt_write(priv, (boundary - 1), 0xFF);
	if (true != status)
		return status;

	/*  Make the other pages as ring buffer
	 *  This ring buffer is used as beacon buffer
	 *  if we config this MAC as two MAC transfer.
	 *  Otherwise used as local loopback buffer. */
	for (i = boundary; i < last_entry_of_tx_pkt_buf; i++) {
		status = _rtl88eu_llt_write(priv, i, (i + 1));
		if (true != status)
			return status;
	}

	/* Let last entry point to the start entry of ring buffer */
	status = _rtl88eu_llt_write(priv, last_entry_of_tx_pkt_buf,
			boundary);
	if (true != status) {
		return status;
	}
	return status;
}

static void _rtl88eu_read_pwrvalue_from_prom(struct rtl8xxxu_priv *priv,
			struct txpower_info_2g *pwrinfo24g, u8 *hwinfo)
{
	u32 rfpath, eeaddr = EEPROM_TX_PWR_INX, group, txcnt = 0;

	memset(pwrinfo24g, 0, sizeof(struct txpower_info_2g));

	for (rfpath = 0; rfpath < MAX_RF_PATH; rfpath++) {
		/* 2.4G default value */
		for (group = 0; group < MAX_CHNL_GROUP_24G; group++) {
			pwrinfo24g->index_cck_base[rfpath][group] =
				hwinfo[eeaddr++];
			if (pwrinfo24g->index_cck_base[rfpath][group] == 0xFF)
				pwrinfo24g->index_cck_base[rfpath][group] =
					EEPROM_DEFAULT_24G_INDEX;
		}
		for (group = 0; group < MAX_CHNL_GROUP_24G - 1; group++) {
			pwrinfo24g->index_bw40_base[rfpath][group] =
				hwinfo[eeaddr++];
			if (pwrinfo24g->index_bw40_base[rfpath][group] == 0xFF)
				pwrinfo24g->index_bw40_base[rfpath][group] =
					EEPROM_DEFAULT_24G_INDEX;
		}
		for (txcnt = 0; txcnt < MAX_TX_COUNT; txcnt++) {
			if (txcnt == 0) {
				pwrinfo24g->bw40_diff[rfpath][txcnt] = 0;
				if (hwinfo[eeaddr] == 0xFF) {
					pwrinfo24g->bw20_diff[rfpath][txcnt] =
						EEPROM_DEFAULT_24G_HT20_DIFF;
				} else {
					pwrinfo24g->bw20_diff[rfpath][txcnt] =
						(hwinfo[eeaddr] & 0xf0) >> 4;
					if (pwrinfo24g->bw20_diff[
							rfpath][txcnt] & BIT(3))
						pwrinfo24g->bw20_diff[
							rfpath][txcnt] |= 0xF0;
				}

				if (hwinfo[eeaddr] == 0xFF) {
					pwrinfo24g->ofdm_diff[rfpath][txcnt] =
						EEPROM_DEFAULT_24G_OFDM_DIFF;
				} else {
					pwrinfo24g->ofdm_diff[rfpath][txcnt] =
						(hwinfo[eeaddr] & 0x0f);
					if (pwrinfo24g->ofdm_diff[
							rfpath][txcnt] & BIT(3))
						pwrinfo24g->ofdm_diff[
							rfpath][txcnt] |= 0xF0;
				}
				pwrinfo24g->cck_diff[rfpath][txcnt] = 0;
				eeaddr++;
			} else {
				if (hwinfo[eeaddr] == 0xFF) {
					pwrinfo24g->bw40_diff[rfpath][txcnt] =
						EEPROM_DEFAULT_DIFF;
				} else {
					pwrinfo24g->bw40_diff[rfpath][txcnt] =
						(hwinfo[eeaddr] & 0xf0) >> 4;
					if (pwrinfo24g->bw40_diff[
							rfpath][txcnt] & BIT(3))
						pwrinfo24g->bw40_diff[
							rfpath][txcnt] |= 0xF0;
				}

				if (hwinfo[eeaddr] == 0xFF) {
					pwrinfo24g->bw20_diff[rfpath][txcnt] =
						EEPROM_DEFAULT_DIFF;
				} else {
					pwrinfo24g->bw20_diff[rfpath][txcnt] =
							(hwinfo[eeaddr]&0x0f);
					if (pwrinfo24g->bw20_diff[
							rfpath][txcnt] & BIT(3))
						pwrinfo24g->bw20_diff[
							rfpath][txcnt] |= 0xF0;
				}
				eeaddr++;

				if (hwinfo[eeaddr] == 0xFF) {
					pwrinfo24g->ofdm_diff[rfpath][txcnt] =
						EEPROM_DEFAULT_DIFF;
				} else {
					pwrinfo24g->ofdm_diff[rfpath][txcnt] =
						(hwinfo[eeaddr] & 0xf0) >> 4;
					if (pwrinfo24g->ofdm_diff[
							rfpath][txcnt] & BIT(3))
						pwrinfo24g->ofdm_diff[
							rfpath][txcnt] |= 0xF0;
				}

				if (hwinfo[eeaddr] == 0xFF) {
					pwrinfo24g->cck_diff[rfpath][txcnt] =
						EEPROM_DEFAULT_DIFF;
				} else {
					pwrinfo24g->cck_diff[rfpath][txcnt] =
						(hwinfo[eeaddr]&0x0f);
					if (pwrinfo24g->cck_diff[
							rfpath][txcnt] & BIT(3))
						pwrinfo24g->cck_diff[
							rfpath][txcnt] |= 0xF0;
				}
				eeaddr++;
			}
		}
	}
}

static u8 _rtl88e_get_chnl_group(u8 chnl)
{
	u8 group = 0;

	if (chnl < 3)
		group = 0;
	else if (chnl < 6)
		group = 1;
	else if (chnl < 9)
		group = 2;
	else if (chnl < 12)
		group = 3;
	else if (chnl < 14)
		group = 4;
	else if (chnl == 14)
		group = 5;

	return group;
}

static void _rtl88eu_read_txpower_info(struct rtl8xxxu_priv *priv, u8 *hwinfo)
{
	struct txpower_info_2g pwrinfo24g;
	u8 rf_path, index;
	u8 i;

	_rtl88eu_read_pwrvalue_from_prom(priv, &pwrinfo24g, hwinfo);

	for (rf_path = 0; rf_path < 2; rf_path++) {
		for (i = 0; i < 14; i++) {
			index = _rtl88e_get_chnl_group(i+1);

			rtlefuse.txpwrlevel_cck[rf_path][i] =
				pwrinfo24g.index_cck_base[rf_path][index];
			rtlefuse.txpwrlevel_ht40_1s[rf_path][i] =
				pwrinfo24g.index_bw40_base[rf_path][index];
			rtlefuse.txpwr_ht20diff[rf_path][i] =
				pwrinfo24g.bw20_diff[rf_path][0];
			rtlefuse.txpwr_legacyhtdiff[rf_path][i] =
				pwrinfo24g.ofdm_diff[rf_path][0];
		}
	}
	
	rtlefuse.eeprom_regulatory =
		(hwinfo[EEPROM_RF_BOARD_OPTION_88E] & 0x7);
	if (hwinfo[EEPROM_RF_BOARD_OPTION_88E] == 0xFF)
		rtlefuse.eeprom_regulatory =
			(EEPROM_DEFAULT_BOARD_OPTION & 0x7);
	
	if (rtlefuse.eeprom_regulatory) {
		dev_err(&priv->udev->dev, "%s:Unsupported regulatory : %d\n",
				__func__, rtlefuse.eeprom_regulatory);
	}
}

bool rtl_hal_pwrseqcmdparsing(struct rtl8xxxu_priv *priv, u8 cut_version,
			      u8 faversion, u8 interface_type,
			      struct wlan_pwr_cfg pwrcfgcmd[])
{
	struct wlan_pwr_cfg cfg_cmd = {0};
	bool polling_bit = false;
	u32 ary_idx = 0;
	u8 value = 0;
	u32 offset = 0;
	u32 polling_count = 0;
	u32 max_polling_cnt = 5000;

	do {
		cfg_cmd = pwrcfgcmd[ary_idx];
		RT_TRACE(priv, COMP_INIT, DBG_TRACE,
			 "rtl_hal_pwrseqcmdparsing(): offset(%#x),cut_msk(%#x), famsk(%#x), interface_msk(%#x), base(%#x), cmd(%#x), msk(%#x), value(%#x)\n",
			 GET_PWR_CFG_OFFSET(cfg_cmd),
			 GET_PWR_CFG_CUT_MASK(cfg_cmd),
			 GET_PWR_CFG_FAB_MASK(cfg_cmd),
			 GET_PWR_CFG_INTF_MASK(cfg_cmd),
			 GET_PWR_CFG_BASE(cfg_cmd),
			 GET_PWR_CFG_CMD(cfg_cmd),
			 GET_PWR_CFG_MASK(cfg_cmd),
			 GET_PWR_CFG_VALUE(cfg_cmd));

		if ((GET_PWR_CFG_FAB_MASK(cfg_cmd)&faversion) &&
		    (GET_PWR_CFG_CUT_MASK(cfg_cmd)&cut_version) &&
		    (GET_PWR_CFG_INTF_MASK(cfg_cmd)&interface_type)) {
			switch (GET_PWR_CFG_CMD(cfg_cmd)) {
			case PWR_CMD_READ:
				RT_TRACE(priv, COMP_INIT, DBG_TRACE,
					"rtl_hal_pwrseqcmdparsing(): PWR_CMD_READ\n");
				break;
			case PWR_CMD_WRITE:
				RT_TRACE(priv, COMP_INIT, DBG_TRACE,
					"rtl_hal_pwrseqcmdparsing(): PWR_CMD_WRITE\n");
				offset = GET_PWR_CFG_OFFSET(cfg_cmd);

				/*Read the value from system register*/
				value = rtl8xxxu_read8(priv, offset);
				value &= (~(GET_PWR_CFG_MASK(cfg_cmd)));
				value |= (GET_PWR_CFG_VALUE(cfg_cmd) &
					  GET_PWR_CFG_MASK(cfg_cmd));

				/*Write the value back to sytem register*/
				rtl8xxxu_write8(priv, offset, value);
				break;
			case PWR_CMD_POLLING:
				RT_TRACE(priv, COMP_INIT, DBG_TRACE,
					"rtl_hal_pwrseqcmdparsing(): PWR_CMD_POLLING\n");
				polling_bit = false;
				offset = GET_PWR_CFG_OFFSET(cfg_cmd);

				do {
					value = rtl8xxxu_read8(priv, offset);

					value &= GET_PWR_CFG_MASK(cfg_cmd);
					if (value ==
					    (GET_PWR_CFG_VALUE(cfg_cmd) &
					     GET_PWR_CFG_MASK(cfg_cmd)))
						polling_bit = true;
					else
						udelay(10);

					if (polling_count++ > max_polling_cnt)
						return false;
				} while (!polling_bit);
				break;
			case PWR_CMD_DELAY:
				RT_TRACE(priv, COMP_INIT, DBG_TRACE,
					 "rtl_hal_pwrseqcmdparsing(): PWR_CMD_DELAY\n");
				if (GET_PWR_CFG_VALUE(cfg_cmd) ==
				    PWRSEQ_DELAY_US)
					udelay(GET_PWR_CFG_OFFSET(cfg_cmd));
				else
					mdelay(GET_PWR_CFG_OFFSET(cfg_cmd));
				break;
			case PWR_CMD_END:
				RT_TRACE(priv, COMP_INIT, DBG_TRACE,
					 "rtl_hal_pwrseqcmdparsing(): PWR_CMD_END\n");
				return true;
			default:
#if 0
				RT_ASSERT(false,
					  "rtl_hal_pwrseqcmdparsing(): Unknown CMD!!\n");
#endif
				break;
			}
		}
		ary_idx++;
	} while (1);

	return true;
}

static int rtl8188eu_power_on(struct rtl8xxxu_priv *priv)
{
	u16 value16;
	
	/* TODO Rtl8188E_NIC... -> RTL8188eu_NIC...*/
	if (!rtl_hal_pwrseqcmdparsing(priv, PWR_CUT_ALL_MSK,
				      PWR_FAB_ALL_MSK, PWR_INTF_USB_MSK,
				      rtl8188ee_power_on_flow)) {
		RT_TRACE(priv, COMP_POWER, DBG_LOUD,
			 "run power on flow fail\n");
		return false;
	}

	/* Enable MAC DMA/WMAC/SCHEDULE/SEC block
	 * Set CR bit10 to enable 32k calibration. */
	rtl8xxxu_write16(priv, REG_CR, 0x00);

	/* Enable MAC DMA/WMAC/SCHEDULE/SEC block */
	value16 = rtl8xxxu_read16(priv, REG_CR);
	value16 |= (HCI_TXDMA_EN | HCI_RXDMA_EN | TXDMA_EN | RXDMA_EN
				| PROTOCOL_EN | SCHEDULE_EN | ENSEC | CALTMR_EN);
	/* for SDIO - Set CR bit10 to enable 32k calibration. */

	rtl8xxxu_write16(priv, REG_CR, value16);

	return true;
}

/*  Shall USB interface init this? */
static void _rtl88eu_init_interrupt(struct rtl8xxxu_priv *priv)
{
	u32 imr, imr_ex;
	u8  usb_opt;

	/* HISR write one to clear */
	rtl8xxxu_write32(priv, REG_HISR_88E, 0xffffffff);
	/*  HIMR - */
	imr = IMR_PSTIMEOUT_88E | IMR_TBDER_88E | IMR_CPWM_88E | IMR_CPWM2_88E;
	rtl8xxxu_write32(priv, REG_HIMR_88E, imr);

	imr_ex = IMR_TXERR_88E | IMR_RXERR_88E | IMR_TXFOVW_88E | IMR_RXFOVW_88E;
	rtl8xxxu_write32(priv, REG_HIMRE_88E, imr_ex);

	/* REG_USB_SPECIAL_OPTION - BIT(4) */
	/* 0; Use interrupt endpoint to upload interrupt pkt */
	/* 1; Use bulk endpoint to upload interrupt pkt, */
	usb_opt = rtl8xxxu_read8(priv, REG_USB_SPECIAL_OPTION);
#if 1
	usb_opt = usb_opt | (INT_BULK_SEL);
#else
	usb_opt = usb_opt & (~INT_BULK_SEL);
#endif
	
	rtl8xxxu_write8(priv, REG_USB_SPECIAL_OPTION, usb_opt);
}

static void _rtl88eu_init_queue_reserved_page(struct rtl8xxxu_priv *priv)
{
	//여기 if문으로 추가해야함.
	rtl8xxxu_write16(priv, REG_RQPN_NPQ, 0x0000);
	rtl8xxxu_write16(priv, REG_RQPN_NPQ, 0x0d);
	rtl8xxxu_write32(priv, REG_RQPN, 0x808e000d);
}

static void _rtl88eu_init_txbuffer_boundary(struct rtl8xxxu_priv *priv,
					    u8 boundary)
{

	rtl8xxxu_write8(priv, REG_TXPKTBUF_BCNQ_BDNY, boundary);
	rtl8xxxu_write8(priv, REG_TXPKTBUF_MGQ_BDNY, boundary);
	rtl8xxxu_write8(priv, REG_TXPKTBUF_WMAC_LBK_BF_HD, boundary);
	rtl8xxxu_write8(priv, REG_TRXFF_BNDY, boundary);
	rtl8xxxu_write8(priv, REG_TDECTRL + 1, boundary);
}

static void _rtl88eu_init_normal_chip_reg_priority(struct rtl8xxxu_priv *priv,
						   u16 be_q, u16 bk_q,
						   u16 vi_q, u16 vo_q,
						   u16 mgt_q, u16 hi_q)
{
	u16 value16 = (rtl8xxxu_read16(priv, REG_TRXDMA_CTRL) & 0x7);

	value16 |= _TXDMA_BEQ_MAP(be_q)	| _TXDMA_BKQ_MAP(bk_q) |
		   _TXDMA_VIQ_MAP(vi_q)	| _TXDMA_VOQ_MAP(vo_q) |
		   _TXDMA_MGQ_MAP(mgt_q) | _TXDMA_HIQ_MAP(hi_q);

	rtl8xxxu_write16(priv, REG_TRXDMA_CTRL, value16);
}

static void _rtl88eu_init_normal_chip_two_out_ep_priority(
							struct rtl8xxxu_priv *priv)
{
	u16 be_q, bk_q, vi_q, vo_q, mgt_q, hi_q;
	u16 value_hi = 0;
	u16 value_low = 0;
#if 1
	value_hi = QUEUE_HIGH;
	value_low = QUEUE_NORMAL;
#else
	value_hi = 0;
	value_low = 0;

#endif
	be_q	= value_low;
	bk_q	= value_low;
	vi_q	= value_hi;
	vo_q	= value_hi;
	mgt_q	= value_hi;
	hi_q	= value_hi;
	
	_rtl88eu_init_normal_chip_reg_priority(priv, be_q, bk_q, vi_q, vo_q, mgt_q, hi_q);
}

static void _rtl88eu_init_normal_chip_one_out_ep_priority(
							struct rtl8xxxu_priv *priv)
{
	u16 value = QUEUE_HIGH;
	_rtl88eu_init_normal_chip_reg_priority(priv, value, value, value, value,
					       value, value);
}

static void _rtl88eu_init_queue_priority(struct rtl8xxxu_priv *priv)
{
#if 0
	switch (rtlusb->out_ep_nums) {
	case 1:
		_rtl88eu_init_normal_chip_one_out_ep_priority(hw);
		break;
	case 2:
		_rtl88eu_init_normal_chip_two_out_ep_priority(hw);
		break;
	case 3:
		_rtl88eu_init_normal_chip_three_out_ep_priority(hw);
		break;
	default:
		break;
	}
#else
	//_rtl88eu_init_normal_chip_two_out_ep_priority(priv);
	_rtl88eu_init_normal_chip_one_out_ep_priority(priv);
#endif
}

static void _rtl88eu_enable_bcn_sub_func(struct rtl8xxxu_priv *priv)
{
	_rtl88eu_set_bcn_ctrl_reg(priv, 0, BIT(1));
}

static void _rtl88eu_disable_bcn_sub_func(struct rtl8xxxu_priv *priv)
{
	_rtl88eu_set_bcn_ctrl_reg(priv, BIT(1), 0);
}

static void _rtl88eu_resume_tx_beacon(struct rtl8xxxu_priv *priv)
{
	u8 tmp1byte;

	tmp1byte = rtl8xxxu_read8(priv, REG_FWHW_TXQ_CTRL + 2);
	rtl8xxxu_write8(priv, REG_FWHW_TXQ_CTRL + 2, tmp1byte | BIT(6));
	rtl8xxxu_write8(priv, REG_TBTT_PROHIBIT + 1, 0xff);
	tmp1byte = rtl8xxxu_read8(priv, REG_TBTT_PROHIBIT + 2);
	tmp1byte |= BIT(0);
	rtl8xxxu_write8(priv, REG_TBTT_PROHIBIT + 2, tmp1byte);
}

static void _rtl88eu_stop_tx_beacon(struct rtl8xxxu_priv *priv)
{
	u8 tmp1byte;

	tmp1byte = rtl8xxxu_read8(priv, REG_FWHW_TXQ_CTRL + 2);
	rtl8xxxu_write8(priv, REG_FWHW_TXQ_CTRL + 2, tmp1byte & (~BIT(6)));
	rtl8xxxu_write8(priv, REG_TBTT_PROHIBIT + 1, 0x64);
	tmp1byte = rtl8xxxu_read8(priv, REG_TBTT_PROHIBIT + 2);
	tmp1byte &= ~(BIT(0));
	rtl8xxxu_write8(priv, REG_TBTT_PROHIBIT + 2, tmp1byte);
}

static int _rtl88eu_set_media_status(struct rtl8xxxu_priv *priv,
				     enum nl80211_iftype type)
{
	u8 bt_msr = rtl8xxxu_read8(priv, MSR);
	enum led_ctl_mode ledaction = LED_CTL_NO_LINK;

	rtl8xxxu_write8(priv, REG_BCN_CTRL,
		       rtl8xxxu_read8(priv, REG_BCN_CTRL) | BIT(4));
	bt_msr &= 0x0c;
	switch (type) {
	case NL80211_IFTYPE_UNSPECIFIED:
		bt_msr |= MSR_NOLINK;
		ledaction = LED_CTL_LINK;
		RT_TRACE(priv, COMP_ERR, DBG_TRACE,
			 "Set Network type to NO LINK!\n");
		break;
	case NL80211_IFTYPE_ADHOC:
		bt_msr |= MSR_ADHOC;
		RT_TRACE(priv, COMP_ERR, DBG_TRACE,
			 "Set Network type to Ad Hoc!\n");
		break;
	case NL80211_IFTYPE_STATION:
		bt_msr |= MSR_INFRA;
		ledaction = LED_CTL_LINK;
		RT_TRACE(priv, COMP_ERR, DBG_TRACE,
			 "Set Network type to STA!\n");
		break;
	case NL80211_IFTYPE_AP:
		bt_msr |= MSR_AP;
		RT_TRACE(priv, COMP_ERR, DBG_TRACE,
			 "Set Network type to AP!\n");
		break;
	default:
		RT_TRACE(priv, COMP_ERR, DBG_EMERG,
			 "Network type %d not supported!\n", type);
		goto error_out;
	}
	rtl8xxxu_write8(priv, MSR, bt_msr);
	/* 나중에 */
	//rtlpriv->cfg->ops->led_control(hw, ledaction);

	if (type == NL80211_IFTYPE_UNSPECIFIED ||
	    type == NL80211_IFTYPE_STATION) {
		_rtl88eu_stop_tx_beacon(priv);
		rtl8xxxu_write8(priv, REG_BCN_CTRL, 0x19);
	} else if (type == NL80211_IFTYPE_ADHOC) {
		_rtl88eu_resume_tx_beacon(priv);
		rtl8xxxu_write8(priv, REG_BCN_CTRL, 0x1a);
	} else if (type == NL80211_IFTYPE_AP) {
		_rtl88eu_resume_tx_beacon(priv);
		rtl8xxxu_write8(priv, REG_BCN_CTRL, 0x12);
		rtl8xxxu_write32(priv, REG_RCR, 0x7000208e);
		rtl8xxxu_write16(priv, REG_RXFLTMAP2, 0xffff);
		rtl8xxxu_write16(priv, REG_RXFLTMAP1, 0x0400);
		rtl8xxxu_write8(priv, REG_BCNDMATIM, 0x02);
		rtl8xxxu_write8(priv, REG_ATIMWND, 0x0a);
		rtl8xxxu_write16(priv, REG_BCNTCFG, 0x00);
		rtl8xxxu_write16(priv, REG_TBTT_PROHIBIT, 0xff04);
		rtl8xxxu_write16(priv, REG_TSFTR_SYN_OFFSET, 0x7fff);
		rtl8xxxu_write8(priv, REG_DUAL_TSF_RST, BIT(0));
		rtl8xxxu_write8(priv, REG_MBID_NUM,
			       rtl8xxxu_read8(priv, REG_MBID_NUM) |
					     BIT(3) | BIT(4));
		rtl8xxxu_write8(priv, REG_BCN_CTRL,
			       (DIS_TSF_UDT0_NORMAL_CHIP|EN_BCN_FUNCTION |
				BIT(1)));
		rtl8xxxu_write8(priv, REG_BCN_CTRL_1,
			       rtl8xxxu_read8(priv, REG_BCN_CTRL_1) | BIT(0));

	} else {
		RT_TRACE(priv, COMP_ERR, DBG_WARNING,
			 "Set HW_VAR_MEDIA_STATUS:No such media status(%x)\n",
			 type);
	}
	return 0;
error_out:
	return 1;
}

static void _rtl88eu_init_driver_info_size(struct rtl8xxxu_priv *priv,
					   u8 drv_info_size)
{
}

static void _rtl88eu_init_wmac_setting(struct rtl8xxxu_priv *priv)
{
	u32 val32;

	//val32 부분은 에외처리 할것
	val32 = RCR_AAP | RCR_APM | RCR_AM | RCR_AB |
				  RCR_CBSSID_DATA | RCR_CBSSID_BCN |
				  RCR_APP_ICV | RCR_AMF | RCR_HTC_LOC_CTRL |
				  RCR_APP_MIC | RCR_APP_PHYSTS;

	rtl8xxxu_write32(priv, REG_RCR, val32);

	/* Accept all multicast address */
	rtl8xxxu_write32(priv, REG_MAR, 0xFFFFFFFF);
	rtl8xxxu_write32(priv, REG_MAR + 4, 0xFFFFFFFF);
}

static void _rtl88eu_init_adaptive_ctrl(struct rtl8xxxu_priv *priv)
{
	u16 value16;
	u32 value32;

	/* Response Rate Set */
	value32 = rtl8xxxu_read32(priv, REG_RRSR);
	value32 &= ~RATE_BITMAP_ALL;
	value32 |= RATE_RRSR_CCK_ONLY_1M;
	rtl8xxxu_write32(priv, REG_RRSR, value32);

	/* CF-END Threshold */

	/* SIFS (used in NAV) */
	value16 = _SPEC_SIFS_CCK(0x10) | _SPEC_SIFS_OFDM(0x10);
	rtl8xxxu_write16(priv, REG_SPEC_SIFS, value16);

	/* Retry Limit */
	value16 = _LRL(0x30) | _SRL(0x30);
	rtl8xxxu_write16(priv, REG_RL, value16);
}

static void _rtl88eu_init_edca(struct rtl8xxxu_priv *priv)
{

	/*  Set Spec SIFS (used in NAV) */
	rtl8xxxu_write16(priv, REG_SPEC_SIFS, 0x100a);
	rtl8xxxu_write16(priv, REG_MAC_SPEC_SIFS, 0x100a);

	/*  Set SIFS for CCK */
	rtl8xxxu_write16(priv, REG_SIFS_CTX, 0x100a);

	/*  Set SIFS for OFDM */
	rtl8xxxu_write16(priv, REG_SIFS_TRX, 0x100a);

	/*  TXOP */
	rtl8xxxu_write32(priv, REG_EDCA_BE_PARAM, 0x005EA42B);
	rtl8xxxu_write32(priv, REG_EDCA_BK_PARAM, 0x0000A44F);
	rtl8xxxu_write32(priv, REG_EDCA_VI_PARAM, 0x005EA324);
	rtl8xxxu_write32(priv, REG_EDCA_VO_PARAM, 0x002FA226);
}

static void _rtl88eu_init_retry_function(struct rtl8xxxu_priv *priv)
{
	u8 value8;

	value8 = rtl8xxxu_read8(priv, REG_FWHW_TXQ_CTRL);
	value8 |= EN_AMPDU_RTY_NEW;
	rtl8xxxu_write8(priv, REG_FWHW_TXQ_CTRL, value8);

	/* Set ACK timeout */
	rtl8xxxu_write8(priv, REG_ACKTO, 0x40);
}

static void _rtl88eu_init_beacon_parameters(struct rtl8xxxu_priv *priv)
{
	rtl8xxxu_write16(priv, REG_BCN_CTRL, 0x1010);
	rtl8xxxu_write16(priv, REG_TBTT_PROHIBIT, 0x6404);
	rtl8xxxu_write8(priv, REG_DRVERLYINT, DRIVER_EARLY_INT_TIME);
	rtl8xxxu_write8(priv, REG_BCNDMATIM, BCN_DMA_ATIME_INT_TIME);
	rtl8xxxu_write16(priv, REG_BCNTCFG, 0x660f);
	reg_bcn_ctrl_val = rtl8xxxu_read8(priv, REG_BCN_CTRL);
}

static void _rtl88eu_beacon_function_enable(struct rtl8xxxu_priv *priv)
	
{

	rtl8xxxu_write8(priv, REG_BCN_CTRL, (BIT(4) | BIT(3) | BIT(1)));
	rtl8xxxu_write8(priv, REG_RD_CTRL + 1, 0x6F);
}

static void _rtl88eu_hw_power_down(struct rtl8xxxu_priv *priv)
{
	/*  we need to enable register block contrl reg at 0x1c.
	 *  Then enable power down control bit of register 0x04 BIT(4)
	 *  and BIT(15) as 1. */

	/*  Enable register area 0x0-0xc. */
	rtl8xxxu_write8(priv, REG_RSV_CTRL, 0x0);
	rtl8xxxu_write16(priv, REG_APS_FSMCO, 0x8812);
}

static void _rtl88eu_card_disable(struct rtl8xxxu_priv *priv)
{
	u8 val8;

	RT_TRACE(priv, COMP_INIT, DBG_LOUD, "rtl8188eu card disable\n");

	/* Stop Tx Report Timer. 0x4EC[Bit1]=b'0 */
	val8 = rtl8xxxu_read8(priv, REG_TX_RPT_CTRL);
	rtl8xxxu_write8(priv, REG_TX_RPT_CTRL, val8&(~BIT(1)));

	/*  stop rx */
	rtl8xxxu_write8(priv, REG_CR, 0x0);

	/*  Run LPS WL RFOFF flow */
	rtl_hal_pwrseqcmdparsing(priv, PWR_CUT_ALL_MSK,
				 PWR_FAB_ALL_MSK, PWR_INTF_USB_MSK,
				 RTL8188EE_NIC_LPS_ENTER_FLOW);

	/*  2. 0x1F[7:0] = 0 turn off RF */
	val8 = rtl8xxxu_read8(priv, REG_MCUFWDL);
	if ((val8 & RAM_DL_SEL) /*&& rtlhal->fw_ready*/) { /* 8051 RAM code */
		/* Reset MCU 0x2[10]=0. */
		val8 = rtl8xxxu_read8(priv, REG_SYS_FUNC_EN+1);
		/*  0x2[10], FEN_CPUEN */
		val8 &= ~BIT(2);
		rtl8xxxu_write8(priv, REG_SYS_FUNC_EN+1, val8);
	}

	/* reset MCU ready status */
	rtl8xxxu_write8(priv, REG_MCUFWDL, 0);

	/* Disable 32k */
	val8 = rtl8xxxu_read8(priv, REG_32K_CTRL);
	rtl8xxxu_write8(priv, REG_32K_CTRL, val8&(~BIT(0)));

	/*  Card disable power action flow */
	rtl_hal_pwrseqcmdparsing(priv, PWR_CUT_ALL_MSK,
				 PWR_FAB_ALL_MSK, PWR_INTF_USB_MSK,
				 RTL8188EE_NIC_DISABLE_FLOW);

	/*  Reset MCU IO Wrapper */
	val8 = rtl8xxxu_read8(priv, REG_RSV_CTRL+1);
	rtl8xxxu_write8(priv, REG_RSV_CTRL+1, (val8&(~BIT(3))));
	val8 = rtl8xxxu_read8(priv, REG_RSV_CTRL+1);
	rtl8xxxu_write8(priv, REG_RSV_CTRL+1, val8|BIT(3));

	val8 = rtl8xxxu_read8(priv, GPIO_IN);
	rtl8xxxu_write8(priv, GPIO_OUT, val8);
	rtl8xxxu_write8(priv, GPIO_IO_SEL, 0xFF);

	val8 = rtl8xxxu_read8(priv, REG_GPIO_IO_SEL);
	rtl8xxxu_write8(priv, REG_GPIO_IO_SEL, (val8<<4));
	val8 = rtl8xxxu_read8(priv, REG_GPIO_IO_SEL+1);
	rtl8xxxu_write8(priv, REG_GPIO_IO_SEL+1, val8|0x0F);
	/* set LNA ,TRSW,EX_PA Pin to output mode */
	rtl8xxxu_write32(priv, REG_BB_PAD_CTRL, 0x00080808);
	//rtlhal->fw_ready = false;
}

void rtl88eu_card_disable(struct rtl8xxxu_priv *priv)
{
	enum nl80211_iftype opmode;

	RT_TRACE(priv, COMP_INIT, DBG_LOUD, "rtl8188eu card disable\n");
	//mac->link_state = MAC80211_NOLINK;
	opmode = NL80211_IFTYPE_UNSPECIFIED;

	_rtl88eu_set_media_status(priv, opmode);

	//rtlpriv->cfg->ops->led_control(hw, LED_CTL_POWER_OFF);
	//RT_SET_PS_LEVEL(ppsc, RT_RF_OFF_LEVL_HALT_NIC);

	rtl8xxxu_write32(priv, REG_HIMR_88E, IMR_DISABLED_88E);
	rtl8xxxu_write32(priv, REG_HIMRE_88E, IMR_DISABLED_88E);

	_rtl88eu_card_disable(priv);
	/* TODO */
#if 0
	_rtl88eu_hw_power_down(priv);
#endif
	rtlphy.iqk_initialized = false;
}

void rtl88eu_enable_hw_security_config(struct rtl8xxxu_priv *priv)
{
	u8 sec_reg_value;

	sec_reg_value = SCR_TXENCENABLE | SCR_RXDECENABLE;

	sec_reg_value |= SCR_TXUSEDK;
	sec_reg_value |= SCR_RXUSEDK;

	sec_reg_value |= (SCR_RXBCUSEDK | SCR_TXBCUSEDK);

	rtl8xxxu_write8(priv, REG_CR + 1, 0x02);

	RT_TRACE(priv, COMP_SEC, DBG_DMESG,
		 "The SECR-value %x\n", sec_reg_value);

	rtl8xxxu_write8(priv, REG_SECCFG, sec_reg_value);
}

int rtl88eu_hw_init(struct ieee80211_hw *hw)
{
	struct rtl8xxxu_priv *priv = hw->priv;
	u8 value8 = 0;
	u16 value16;
	u32 status = true;
	unsigned long flags;
	int i, err;

	reg_bcn_ctrl_val = 0;

	memset(&rtlefuse, 0, sizeof(struct rtl_efuse));
	memset(&rtlphy, 0, sizeof(struct rtl_phy));
	memset(&rtldm, 0, sizeof(struct rtl_dm));

	spin_lock_init(&rf_lock);
	local_save_flags(flags);
	local_irq_disable();

	local_irq_enable();
	err = priv->fops->power_on(priv);
	if (err == false) {
		RT_TRACE(priv, COMP_ERR, DBG_EMERG,
			 "Failed to init power on!\n");
		return err;
	}
#if 0
	if (false == rtl88eu_init_llt_table(priv, boundary)) {
	
		RT_TRACE(priv, COMP_ERR, DBG_EMERG,
			 "Failed to init LLT table\n");
		return -EINVAL;
	}
#else
	priv->fops->llt_init(priv, TX_PAGE_BOUNDARY_88E- 1);
#endif

	_rtl88eu_init_queue_reserved_page(priv);
	_rtl88eu_init_txbuffer_boundary(priv, 0);
	
	rtl8xxxu_write16(priv, REG_TRXFF_BNDY + 2, priv->fops->trxff_boundary);
	
	value8 = (priv->fops->pbp_rx << PBP_PAGE_SIZE_RX_SHIFT) |
		(priv->fops->pbp_tx << PBP_PAGE_SIZE_TX_SHIFT);
	if (priv->rtl_chip != RTL8192E)
		rtl8xxxu_write8(priv, REG_PBP, value8);

	_rtl88eu_init_queue_priority(priv);
	
	rtl8xxxu_write8(priv, REG_RX_DRVINFO_SZ, 4);

	_rtl88eu_init_interrupt(priv);
	_rtl88eu_init_wmac_setting(priv);
	_rtl88eu_init_adaptive_ctrl(priv);
	_rtl88eu_init_edca(priv);
	_rtl88eu_init_retry_function(priv);
	
	//여기 꼭 enable 해야함
	//rtl88e_phy_set_bw_mode(priv, NL80211_CHAN_HT20);
	_rtl88eu_init_beacon_parameters(priv);
	//표준루틴과 다름, boundary값 유일함.
	_rtl88eu_init_txbuffer_boundary(priv, TX_PAGE_BOUNDARY_88E);

#if 0
	rtlhal->last_hmeboxnum = 0;
	rtlphy.iqk_initialized = false;
	rtlphy.pwrgroup_cnt = 0;
	rtlhal->fw_ps_state = FW_PS_STATE_ALL_ON_88E;
	rtlhal->fw_clk_change_in_progress = false;
	rtlhal->allow_sw_to_change_hwclc = false;
	ppsc->fw_current_inpsmode = false;
#else
	rtlphy.iqk_initialized = false;
	rtlphy.pwrgroup_cnt = 0;
#endif
	rtl8188eu_init_mac(priv);
	priv->fops->init_phy_bb(priv);
	priv->fops->init_phy_rf(priv);
	rtlphy.rfreg_chnlval[0] = rtl_get_rfreg(priv, (enum radio_path)0,
				      RF_CHNLBW, RFREG_OFFSET_MASK);

	value16 = rtl8xxxu_read16(priv, REG_CR);
	value16 |= (MACTXEN | MACRXEN);
	rtl8xxxu_write8(priv, REG_CR, value16);

	value8 = rtl8xxxu_read8(priv, REG_TX_RPT_CTRL);
	rtl8xxxu_write8(priv,  REG_TX_RPT_CTRL, (value8 | BIT(1) | BIT(0)));
	rtl8xxxu_write8(priv,  REG_TX_RPT_CTRL+1, 2);
	rtl8xxxu_write16(priv, REG_TX_RPT_TIME, 0xCdf0);
	rtl8xxxu_write8(priv, REG_EARLY_MODE_CONTROL, 0);
	rtl8xxxu_write16(priv, REG_PKT_VO_VI_LIFE_TIME, 0x0400);
	rtl8xxxu_write16(priv, REG_PKT_BE_BK_LIFE_TIME, 0x0400);
	
	rtl_set_bbreg(priv, RFPGA0_RFMOD, BCCKEN, 0x1);
	rtl_set_bbreg(priv, RFPGA0_RFMOD, BOFDMEN, 0x1);

	rtl8xxxu_write32(priv, REG_CAM_CMD, CAM_CMD_POLLING | BIT(30));
	//rtl88eu_enable_hw_security_config(priv);

	for (i = 0; i < ETH_ALEN; i++)
		rtl8xxxu_write8(priv, REG_MACID + i, priv->mac_addr[i]);

	priv->fops->set_tx_power(priv, rtlphy.current_channel, false);
	rtl8xxxu_write32(priv, REG_BAR_MODE_CTRL, 0x0201ffff);
	rtl8xxxu_write8(priv, REG_HWSEQ_CTRL, 0xFF);

	rtl8xxxu_write8(priv, 0x652, 0x0);
	rtl8xxxu_write8(priv,  REG_FWHW_TXQ_CTRL+1, 0x0F);
	rtl8xxxu_write8(priv, REG_EARLY_MODE_CONTROL+3, 0x01);
	rtl8xxxu_write16(priv, REG_TX_RPT_TIME, 0x3DF0);
	rtl8xxxu_write16(priv, REG_TXDMA_OFFSET_CHK,
		       (rtl8xxxu_read16(priv, REG_TXDMA_OFFSET_CHK) |
			DROP_DATA_EN));
	
	rtl88e_phy_set_rfpath_switch(priv, false);

	if (rtlphy.iqk_initialized) {
		rtl88e_phy_iq_calibrate(priv);
	} else {
		rtl88e_phy_iq_calibrate(priv);
		rtlphy.iqk_initialized = true;
	}
	//rtl88e_dm_check_txpower_tracking(priv);
	rtl88e_phy_lc_calibrate(priv, false);

	rtl8xxxu_write8(priv, REG_USB_HRPWM, 0);
	rtl8xxxu_write32(priv, REG_FWHW_TXQ_CTRL,
			rtl8xxxu_read32(priv, REG_FWHW_TXQ_CTRL)|BIT(12));
#if 0
	rtl88e_dm_init(priv);
	rtl88eu_hal_notch_filter(priv, 0);
#endif
	local_irq_restore(flags);
	return 0;
exit:
	local_irq_restore(flags);
	complete(&priv->firmware_loading_complete);
	return status;
}

static int rtl8188eu_parse_efuse(struct rtl8xxxu_priv *priv)
{
	u16 i, usvalue;
	u8 hwinfo[HWSET_MAX_SIZE] = {0};
	u16 eeprom_id;

	memcpy((void *)hwinfo,
			(void *)priv->efuse_wifi.raw, HWSET_MAX_SIZE);

	eeprom_id = le16_to_cpu(*((__le16 *)hwinfo));
	if (eeprom_id != 0x8129) {
		RT_TRACE(priv, COMP_ERR, DBG_WARNING,
				"EEPROM ID(%#x) is invalid!!\n", eeprom_id);
		return -EINVAL;
	}

	for (i = 0; i < 6; i += 2) {
		usvalue = *(u16 *)&hwinfo[EEPROM_MAC_ADDR + i];
		*((u16 *) (&rtlefuse.dev_addr[i])) = usvalue;
	}

	ether_addr_copy(priv->mac_addr, rtlefuse.dev_addr);

	_rtl88eu_read_txpower_info(priv, hwinfo);

	rtlefuse.eeprom_version = hwinfo[EEPROM_VERSION_88E];
	if (rtlefuse.eeprom_version == 0xFF)
		rtlefuse.eeprom_version = 0;

	RT_TRACE(priv, COMP_ERR, DBG_WARNING,
			"eeprom_version = %d\n", rtlefuse.eeprom_version);

	rtlefuse.eeprom_channelplan = hwinfo[EEPROM_CHANNELPLAN];
	rtlefuse.channel_plan = rtlefuse.eeprom_channelplan;

	rtlefuse.crystalcap = hwinfo[EEPROM_XTAL_88E];
	if (rtlefuse.crystalcap == 0xFF)
		rtlefuse.crystalcap = EEPROM_DEFAULT_CRYSTALCAP_88E;
	rtlefuse.eeprom_oemid = hwinfo[EEPROM_CUSTOMERID_88E];
	RT_TRACE(priv, COMP_ERR, DBG_WARNING,
			"EEPROM Customer ID: 0x%2x\n", rtlefuse.eeprom_oemid);
	rtlefuse.antenna_div_cfg =
		(hwinfo[EEPROM_RF_BOARD_OPTION_88E] & 0x18) >> 3;
	if (hwinfo[EEPROM_RF_BOARD_OPTION_88E] == 0xFF)
		rtlefuse.antenna_div_cfg = 0;
	rtlefuse.antenna_div_type = hwinfo[EEPROM_RF_ANTENNA_OPT_88E];
	if (rtlefuse.antenna_div_type == 0xFF)
		rtlefuse.antenna_div_type = 0x01;
	if (rtlefuse.antenna_div_type == CG_TRX_HW_ANTDIV ||
			rtlefuse.antenna_div_type == CGCS_RX_HW_ANTDIV)
		rtlefuse.antenna_div_cfg = 1;

	printk("[TEST]antenna_div_type = %d\n", rtlefuse.antenna_div_type);

	rtlefuse.board_type = (hwinfo[EEPROM_RF_BOARD_OPTION_88E]
			& 0xE0) >> 5;
	rtlefuse.eeprom_thermalmeter =
		hwinfo[EEPROM_THERMAL_METER_88E];

	/* TODO */
#if 0
	if (rtlhal->oem_id == RT_CID_DEFAULT) {
		switch (rtlefuse.eeprom_oemid) {
			case EEPROM_CID_DEFAULT:
				if (rtlefuse.eeprom_did == 0x8179) {
					if (rtlefuse.eeprom_svid == 0x1025) {
						rtlhal->oem_id = RT_CID_819X_ACER;
					} else if ((rtlefuse.eeprom_svid == 0x10EC &&
								rtlefuse.eeprom_smid == 0x0179) ||
							(rtlefuse.eeprom_svid == 0x17AA &&
							 rtlefuse.eeprom_smid == 0x0179)) {
						rtlhal->oem_id = RT_CID_819X_LENOVO;
					} else if (rtlefuse.eeprom_svid == 0x103c &&
							rtlefuse.eeprom_smid == 0x197d) {
						rtlhal->oem_id = RT_CID_819X_HP;
					} else {
						rtlhal->oem_id = RT_CID_DEFAULT;
					}
				} else {
					rtlhal->oem_id = RT_CID_DEFAULT;
				}
				break;
			case EEPROM_CID_TOSHIBA:
				rtlhal->oem_id = RT_CID_TOSHIBA;
				break;
			case EEPROM_CID_QMI:
				rtlhal->oem_id = RT_CID_819X_QMI;
				break;
			case EEPROM_CID_WHQL:
			default:
				rtlhal->oem_id = RT_CID_DEFAULT;
				break;

		}
	}
#endif

	return 0;
}

static int rtl8188eu_load_firmware(struct rtl8xxxu_priv *priv)
{
	return rtl8xxxu_load_firmware(priv, "rtlwifi/rtl8188eufw.bin");
}

static void rtl8192eu_init_phy_bb(struct rtl8xxxu_priv *priv)
{
}

static void rtl8192e_enable_rf(struct rtl8xxxu_priv *priv)
{
}

int rtl88eu_rx_query_desc(struct rtl8xxxu_priv *priv,
			   struct sk_buff *skb,
			   struct ieee80211_rx_status *rx_status)
{
	//struct rx_fwinfo_88e *p_drvinfo;
	struct ieee80211_hdr *hdr;
	struct rtl_stats status = {0,};
	u8 *pdesc = skb->data;
	u32 phystatus = GET_RX_DESC_PHYST(pdesc);
	int rate_idx;
	u8 *data;
	int i;

	skb_pull(skb, 24);

	status.packet_report_type = (u8)GET_RX_STATUS_DESC_RPT_SEL(pdesc);
	if (status.packet_report_type == TX_REPORT2)
		status.length = (u16)GET_RX_RPT2_DESC_PKT_LEN(pdesc);
	else
		status.length = (u16)GET_RX_DESC_PKT_LEN(pdesc);
	status.rx_drvinfo_size = (u8)GET_RX_DESC_DRV_INFO_SIZE(pdesc) *
	    RX_DRV_INFO_SIZE_UNIT;
	status.rx_bufshift = (u8)(GET_RX_DESC_SHIFT(pdesc) & 0x03);
	status.icv = (u16)GET_RX_DESC_ICV(pdesc);
	status.crc = (u16)GET_RX_DESC_CRC32(pdesc);
	status.hwerror = (status.crc | status.icv);
	status.decrypted = !GET_RX_DESC_SWDEC(pdesc);
	status.rate = (u8)GET_RX_DESC_RXMCS(pdesc);
	status.shortpreamble = (u16)GET_RX_DESC_SPLCP(pdesc);
	status.isampdu = (bool) (GET_RX_DESC_PAGGR(pdesc) == 1);
	status.isfirst_ampdu = (bool)((GET_RX_DESC_PAGGR(pdesc) == 1) &&
				(GET_RX_DESC_FAGGR(pdesc) == 1));
	if (status.packet_report_type == NORMAL_RX)
		status.timestamp_low = GET_RX_DESC_TSFL(pdesc);
	status.rx_is40Mhzpacket = (bool) GET_RX_DESC_BW(pdesc);
	status.is_ht = (bool)GET_RX_DESC_RXHT(pdesc);

	//status.is_cck = RTL8188_RX_HAL_IS_CCK_RATE(status.rate);

	status.macid = GET_RX_DESC_MACID(pdesc);
	if (GET_RX_STATUS_DESC_MAGIC_MATCH(pdesc))
		status.wake_match = BIT(2);
	else if (GET_RX_STATUS_DESC_MAGIC_MATCH(pdesc))
		status.wake_match = BIT(1);
	else if (GET_RX_STATUS_DESC_UNICAST_MATCH(pdesc))
		status.wake_match = BIT(0);
	else
		status.wake_match = 0;
	//rx_status.freq = hw->conf.chandef.chan->center_freq;
	//rx_status.band = hw->conf.chandef.chan->band;

	hdr = (struct ieee80211_hdr *)(skb->data + status.rx_drvinfo_size
			+ status.rx_bufshift);
	if (status.crc) {
		rx_status->flag |= RX_FLAG_FAILED_FCS_CRC;
	}

	if (status.rx_is40Mhzpacket)
		rx_status->flag |= RX_FLAG_40MHZ;

	if (status.is_ht)
		rx_status->flag |= RX_FLAG_HT;

	rx_status->flag |= RX_FLAG_MACTIME_START;

	/* hw will set status.decrypted true, if it finds the
	 * frame is open data frame or mgmt frame.
	 * So hw will not decryption robust managment frame
	 * for IEEE80211w but still set status.decrypted
	 * true, so here we should set it back to undecrypted
	 * for IEEE80211w frame, and mac80211 sw will help
	 * to decrypt it
	 */
	if (status.decrypted) {
		if ((!_ieee80211_is_robust_mgmt_frame(hdr)) &&
		    (ieee80211_has_protected(hdr->frame_control)))
			rx_status->flag |= RX_FLAG_DECRYPTED;
		else
			rx_status->flag &= ~RX_FLAG_DECRYPTED;
	}
	/* rate_idx: index of data rate into band's
	 * supported rates or MCS index if HT rates
	 * are use (RX_FLAG_HT)
	 * Notice: this is diff with windows define
	 */
#if 1
#if 0
	rx_status->rate_idx = rtlwifi_rate_mapping(hw, status.is_ht,
						   false, status.rate);
#endif
	switch (status.rate) {
		case DESC_RATE1M:
			rate_idx = 0;
			break;
		case DESC_RATE2M:
			rate_idx = 1;
			break;
		case DESC_RATE5_5M:
			rate_idx = 2;
			break;
		case DESC_RATE11M:
			rate_idx = 3;
			break;
		case DESC_RATE6M:
			rate_idx = 4;
			break;
		case DESC_RATE9M:
			rate_idx = 5;
			break;
		case DESC_RATE12M:
			rate_idx = 6;
			break;
		case DESC_RATE18M:
			rate_idx = 7;
			break;
		case DESC_RATE24M:
			rate_idx = 8;
			break;
		case DESC_RATE36M:
			rate_idx = 9;
			break;
		case DESC_RATE48M:
			rate_idx = 10;
			break;
		case DESC_RATE54M:
			rate_idx = 11;
			break;
		default:
			rate_idx = 0;
			break;
	}

	rx_status->mactime = status.timestamp_low;
	rx_status->rate_idx = rate_idx;
#endif
#if 0
	if (phystatus == true) {
		p_drvinfo = (struct rx_fwinfo_88e *)(skb->data +
						     status.rx_bufshift);

		_rtl88ee_translate_rx_signal_stuff(hw,
						   skb, status, pdesc,
						   p_drvinfo);
	}
	rx_status->signal = status.recvsignalpower + 10;
#endif
	if (status.packet_report_type == TX_REPORT2) {
		status.macid_valid_entry[0] =
			 GET_RX_RPT2_DESC_MACID_VALID_1(pdesc);
		status.macid_valid_entry[1] =
			 GET_RX_RPT2_DESC_MACID_VALID_2(pdesc);
	}

	skb_pull(skb, (status.rx_drvinfo_size + status.rx_bufshift));
	return 0;
}

struct rtl8xxxu_fileops rtl8188eu_fops = {
	.init_device = rtl88eu_hw_init,
	.parse_efuse = rtl8188eu_parse_efuse,
	.load_firmware = rtl8188eu_load_firmware,
	.power_on = rtl8188eu_power_on,
	.power_off = rtl88eu_card_disable,
	.reset_8051 = rtl8xxxu_reset_8051,
	.llt_init = rtl8xxxu_init_llt_table,
	.init_phy_bb = rtl8xxxu_gen1_init_phy_bb,
	.init_phy_rf = rtl8188eu_init_phy_rf,
	.phy_iq_calibrate = rtl88e_phy_iq_calibrate,
	.config_channel = rtl8xxxu_gen1_config_channel,
	.parse_rx_desc = rtl88eu_rx_query_desc,
	.enable_rf = rtl8192e_enable_rf,
	.disable_rf = rtl8xxxu_gen1_disable_rf,
	.usb_quirks = rtl8xxxu_gen1_usb_quirks,
	.set_tx_power = rtl88e_phy_set_txpower_level,
	.update_rate_mask = rtl8xxxu_update_rate_mask,
	.report_connect = rtl8xxxu_gen2_report_connect,
	.writeN_block_size = 128,
	.tx_desc_size = sizeof(struct rtl8xxxu_txdesc32),
	.rx_desc_size = sizeof(struct rtl8xxxu_rxdesc24),
	
	.adda_1t_init = 0x0b1b25a0,
	.adda_1t_path_on = 0x0bdb25a0,
	.adda_2t_path_on_a = 0x04db25a4,
	.adda_2t_path_on_b = 0x0b1b25a4,
	.trxff_boundary = 0x23ff,
	.pbp_rx = PBP_PAGE_SIZE_128,
	.pbp_tx = PBP_PAGE_SIZE_128,
	.mactable = rtl8188eu_mac_init_table,
};

