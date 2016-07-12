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

//////////////////////////////////////////테스트//////////////////////////////////////
#define DM_DIG_THRESH_HIGH		40
#define DM_DIG_THRESH_LOW		35
#define DM_FALSEALARM_THRESH_LOW	400
#define DM_FALSEALARM_THRESH_HIGH	1000

#define DM_DIG_MAX			0x3e
#define DM_DIG_MIN			0x1e
#define DM_DIG_MAX_AP			0x32
#define DM_DIG_BACKOFF_MAX		12
#define DM_DIG_BACKOFF_MIN		-4
#define DM_DIG_BACKOFF_DEFAULT		10

enum cck_packet_detection_threshold {
	CCK_PD_STAGE_LOWRSSI = 0,
	CCK_PD_STAGE_HIGHRSSI = 1,
	CCK_FA_STAGE_LOW = 2,
	CCK_FA_STAGE_HIGH = 3,
	CCK_PD_STAGE_MAX = 4,
};

enum dm_dig_ext_port_alg_e {
	DIG_EXT_PORT_STAGE_0 = 0,
	DIG_EXT_PORT_STAGE_1 = 1,
	DIG_EXT_PORT_STAGE_2 = 2,
	DIG_EXT_PORT_STAGE_3 = 3,
	DIG_EXT_PORT_STAGE_MAX = 4,
};

enum dm_dig_connect_e {
	DIG_STA_DISCONNECT,
	DIG_STA_CONNECT,
	DIG_STA_BEFORE_CONNECT,
	DIG_MULTISTA_DISCONNECT,
	DIG_MULTISTA_CONNECT,
	DIG_AP_DISCONNECT,
	DIG_AP_CONNECT,
	DIG_AP_ADD_STATION,
	DIG_CONNECT_MAX
};
//////////////////////////////////////////테스트///////////////////////////////////////////////////////



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

u32 RTL8188EUPHY_REG_1TARRAY[] = {
		0x800, 0x80040000,
		0x804, 0x00000003,
		0x808, 0x0000FC00,
		0x80C, 0x0000000A,
		0x810, 0x10001331,
		0x814, 0x020C3D10,
		0x818, 0x02200385,
		0x81C, 0x00000000,
		0x820, 0x01000100,
		0x824, 0x00390204,
		0x828, 0x00000000,
		0x82C, 0x00000000,
		0x830, 0x00000000,
		0x834, 0x00000000,
		0x838, 0x00000000,
		0x83C, 0x00000000,
		0x840, 0x00010000,
		0x844, 0x00000000,
		0x848, 0x00000000,
		0x84C, 0x00000000,
		0x850, 0x00000000,
		0x854, 0x00000000,
		0x858, 0x569A11A9,
		0x85C, 0x01000014,
		0x860, 0x66F60110,
		0x864, 0x061F0649,
		0x868, 0x00000000,
		0x86C, 0x27272700,
		0x870, 0x07000760,
		0x874, 0x25004000,
		0x878, 0x00000808,
		0x87C, 0x00000000,
		0x880, 0xB0000C1C,
		0x884, 0x00000001,
		0x888, 0x00000000,
		0x88C, 0xCCC000C0,
		0x890, 0x00000800,
		0x894, 0xFFFFFFFE,
		0x898, 0x40302010,
		0x89C, 0x00706050,
		0x900, 0x00000000,
		0x904, 0x00000023,
		0x908, 0x00000000,
		0x90C, 0x81121111,
		0x910, 0x00000002,
		0x914, 0x00000201,
		0xA00, 0x00D047C8,
		0xA04, 0x80FF000C,
		0xA08, 0x8C838300,
		0xA0C, 0x2E7F120F,
		0xA10, 0x9500BB78,
		0xA14, 0x1114D028,
		0xA18, 0x00881117,
		0xA1C, 0x89140F00,
		0xA20, 0x1A1B0000,
		0xA24, 0x090E1317,
		0xA28, 0x00000204,
		0xA2C, 0x00D30000,
		0xA70, 0x101FBF00,
		0xA74, 0x00000007,
		0xA78, 0x00000900,
		0xA7C, 0x225B0606,
		0xA80, 0x218075B1,
		0xB2C, 0x80000000,
		0xC00, 0x48071D40,
		0xC04, 0x03A05611,
		0xC08, 0x000000E4,
		0xC0C, 0x6C6C6C6C,
		0xC10, 0x08800000,
		0xC14, 0x40000100,
		0xC18, 0x08800000,
		0xC1C, 0x40000100,
		0xC20, 0x00000000,
		0xC24, 0x00000000,
		0xC28, 0x00000000,
		0xC2C, 0x00000000,
		0xC30, 0x69E9AC47,
		0xC34, 0x469652AF,
		0xC38, 0x49795994,
		0xC3C, 0x0A97971C,
		0xC40, 0x1F7C403F,
		0xC44, 0x000100B7,
		0xC48, 0xEC020107,
		0xC4C, 0x007F037F,
		0xC50, 0x69553420,
		0xC54, 0x43BC0094,
		0xC58, 0x00013169,
		0xC5C, 0x00250492,
		0xC60, 0x00000000,
		0xC64, 0x7112848B,
		0xC68, 0x47C00BFF,
		0xC6C, 0x00000036,
		0xC70, 0x2C7F000D,
		0xC74, 0x020610DB,
		0xC78, 0x0000001F,
		0xC7C, 0x00B91612,
		0xC80, 0x390000E4,
		0xC84, 0x20F60000,
		0xC88, 0x40000100,
		0xC8C, 0x20200000,
		0xC90, 0x00091521,
		0xC94, 0x00000000,
		0xC98, 0x00121820,
		0xC9C, 0x00007F7F,
		0xCA0, 0x00000000,
		0xCA4, 0x000300A0,
		0xCA8, 0x00000000,
		0xCAC, 0x00000000,
		0xCB0, 0x00000000,
		0xCB4, 0x00000000,
		0xCB8, 0x00000000,
		0xCBC, 0x28000000,
		0xCC0, 0x00000000,
		0xCC4, 0x00000000,
		0xCC8, 0x00000000,
		0xCCC, 0x00000000,
		0xCD0, 0x00000000,
		0xCD4, 0x00000000,
		0xCD8, 0x64B22427,
		0xCDC, 0x00766932,
		0xCE0, 0x00222222,
		0xCE4, 0x00000000,
		0xCE8, 0x37644302,
		0xCEC, 0x2F97D40C,
		0xD00, 0x00000740,
		0xD04, 0x00020401,
		0xD08, 0x0000907F,
		0xD0C, 0x20010201,
		0xD10, 0xA0633333,
		0xD14, 0x3333BC43,
		0xD18, 0x7A8F5B6F,
		0xD2C, 0xCC979975,
		0xD30, 0x00000000,
		0xD34, 0x80608000,
		0xD38, 0x00000000,
		0xD3C, 0x00127353,
		0xD40, 0x00000000,
		0xD44, 0x00000000,
		0xD48, 0x00000000,
		0xD4C, 0x00000000,
		0xD50, 0x6437140A,
		0xD54, 0x00000000,
		0xD58, 0x00000282,
		0xD5C, 0x30032064,
		0xD60, 0x4653DE68,
		0xD64, 0x04518A3C,
		0xD68, 0x00002101,
		0xD6C, 0x2A201C16,
		0xD70, 0x1812362E,
		0xD74, 0x322C2220,
		0xD78, 0x000E3C24,
		0xE00, 0x2D2D2D2D,
		0xE04, 0x2D2D2D2D,
		0xE08, 0x0390272D,
		0xE10, 0x2D2D2D2D,
		0xE14, 0x2D2D2D2D,
		0xE18, 0x2D2D2D2D,
		0xE1C, 0x2D2D2D2D,
		0xE28, 0x00000000,
		0xE30, 0x1000DC1F,
		0xE34, 0x10008C1F,
		0xE38, 0x02140102,
		0xE3C, 0x681604C2,
		0xE40, 0x01007C00,
		0xE44, 0x01004800,
		0xE48, 0xFB000000,
		0xE4C, 0x000028D1,
		0xE50, 0x1000DC1F,
		0xE54, 0x10008C1F,
		0xE58, 0x02140102,
		0xE5C, 0x28160D05,
		0xE60, 0x00000008,
		0xE68, 0x001B25A4,
		0xE6C, 0x00C00014,
		0xE70, 0x00C00014,
		0xE74, 0x01000014,
		0xE78, 0x01000014,
		0xE7C, 0x01000014,
		0xE80, 0x01000014,
		0xE84, 0x00C00014,
		0xE88, 0x01000014,
		0xE8C, 0x00C00014,
		0xED0, 0x00C00014,
		0xED4, 0x00C00014,
		0xED8, 0x00C00014,
		0xEDC, 0x00000014,
		0xEE0, 0x00000014,
		0xEEC, 0x01C00014,
		0xF14, 0x00000003,
		0xF4C, 0x00000000,
		0xF00, 0x00000300,
};

u32 RTL8188EUPHY_REG_ARRAY_PG[] = {
		0xE00, 0xFFFFFFFF, 0x06070809,
		0xE04, 0xFFFFFFFF, 0x02020405,
		0xE08, 0x0000FF00, 0x00000006,
		0x86C, 0xFFFFFF00, 0x00020400,
		0xE10, 0xFFFFFFFF, 0x08090A0B,
		0xE14, 0xFFFFFFFF, 0x01030607,
		0xE18, 0xFFFFFFFF, 0x08090A0B,
		0xE1C, 0xFFFFFFFF, 0x01030607,
		0xE00, 0xFFFFFFFF, 0x00000000,
		0xE04, 0xFFFFFFFF, 0x00000000,
		0xE08, 0x0000FF00, 0x00000000,
		0x86C, 0xFFFFFF00, 0x00000000,
		0xE10, 0xFFFFFFFF, 0x00000000,
		0xE14, 0xFFFFFFFF, 0x00000000,
		0xE18, 0xFFFFFFFF, 0x00000000,
		0xE1C, 0xFFFFFFFF, 0x00000000,
		0xE00, 0xFFFFFFFF, 0x02020202,
		0xE04, 0xFFFFFFFF, 0x00020202,
		0xE08, 0x0000FF00, 0x00000000,
		0x86C, 0xFFFFFF00, 0x00000000,
		0xE10, 0xFFFFFFFF, 0x04040404,
		0xE14, 0xFFFFFFFF, 0x00020404,
		0xE18, 0xFFFFFFFF, 0x00000000,
		0xE1C, 0xFFFFFFFF, 0x00000000,
		0xE00, 0xFFFFFFFF, 0x02020202,
		0xE04, 0xFFFFFFFF, 0x00020202,
		0xE08, 0x0000FF00, 0x00000000,
		0x86C, 0xFFFFFF00, 0x00000000,
		0xE10, 0xFFFFFFFF, 0x04040404,
		0xE14, 0xFFFFFFFF, 0x00020404,
		0xE18, 0xFFFFFFFF, 0x00000000,
		0xE1C, 0xFFFFFFFF, 0x00000000,
		0xE00, 0xFFFFFFFF, 0x00000000,
		0xE04, 0xFFFFFFFF, 0x00000000,
		0xE08, 0x0000FF00, 0x00000000,
		0x86C, 0xFFFFFF00, 0x00000000,
		0xE10, 0xFFFFFFFF, 0x00000000,
		0xE14, 0xFFFFFFFF, 0x00000000,
		0xE18, 0xFFFFFFFF, 0x00000000,
		0xE1C, 0xFFFFFFFF, 0x00000000,
		0xE00, 0xFFFFFFFF, 0x02020202,
		0xE04, 0xFFFFFFFF, 0x00020202,
		0xE08, 0x0000FF00, 0x00000000,
		0x86C, 0xFFFFFF00, 0x00000000,
		0xE10, 0xFFFFFFFF, 0x04040404,
		0xE14, 0xFFFFFFFF, 0x00020404,
		0xE18, 0xFFFFFFFF, 0x00000000,
		0xE1C, 0xFFFFFFFF, 0x00000000,
		0xE00, 0xFFFFFFFF, 0x00000000,
		0xE04, 0xFFFFFFFF, 0x00000000,
		0xE08, 0x0000FF00, 0x00000000,
		0x86C, 0xFFFFFF00, 0x00000000,
		0xE10, 0xFFFFFFFF, 0x00000000,
		0xE14, 0xFFFFFFFF, 0x00000000,
		0xE18, 0xFFFFFFFF, 0x00000000,
		0xE1C, 0xFFFFFFFF, 0x00000000,
		0xE00, 0xFFFFFFFF, 0x00000000,
		0xE04, 0xFFFFFFFF, 0x00000000,
		0xE08, 0x0000FF00, 0x00000000,
		0x86C, 0xFFFFFF00, 0x00000000,
		0xE10, 0xFFFFFFFF, 0x00000000,
		0xE14, 0xFFFFFFFF, 0x00000000,
		0xE18, 0xFFFFFFFF, 0x00000000,
		0xE1C, 0xFFFFFFFF, 0x00000000,
		0xE00, 0xFFFFFFFF, 0x00000000,
		0xE04, 0xFFFFFFFF, 0x00000000,
		0xE08, 0x0000FF00, 0x00000000,
		0x86C, 0xFFFFFF00, 0x00000000,
		0xE10, 0xFFFFFFFF, 0x00000000,
		0xE14, 0xFFFFFFFF, 0x00000000,
		0xE18, 0xFFFFFFFF, 0x00000000,
		0xE1C, 0xFFFFFFFF, 0x00000000,
		0xE00, 0xFFFFFFFF, 0x00000000,
		0xE04, 0xFFFFFFFF, 0x00000000,
		0xE08, 0x0000FF00, 0x00000000,
		0x86C, 0xFFFFFF00, 0x00000000,
		0xE10, 0xFFFFFFFF, 0x00000000,
		0xE14, 0xFFFFFFFF, 0x00000000,
		0xE18, 0xFFFFFFFF, 0x00000000,
		0xE1C, 0xFFFFFFFF, 0x00000000,
		0xE00, 0xFFFFFFFF, 0x00000000,
		0xE04, 0xFFFFFFFF, 0x00000000,
		0xE08, 0x0000FF00, 0x00000000,
		0x86C, 0xFFFFFF00, 0x00000000,
		0xE10, 0xFFFFFFFF, 0x00000000,
		0xE14, 0xFFFFFFFF, 0x00000000,
		0xE18, 0xFFFFFFFF, 0x00000000,
		0xE1C, 0xFFFFFFFF, 0x00000000,

};

u32 RTL8188EU_RADIOA_1TARRAY[] = {
		0x000, 0x00030000,
		0x008, 0x00084000,
		0x018, 0x00000407,
		0x019, 0x00000012,
		0x01E, 0x00080009,
		0x01F, 0x00000880,
		0x02F, 0x0001A060,
		0x03F, 0x00000000,
		0x042, 0x000060C0,
		0x057, 0x000D0000,
		0x058, 0x000BE180,
		0x067, 0x00001552,
		0x083, 0x00000000,
		0x0B0, 0x000FF8FC,
		0x0B1, 0x00054400,
		0x0B2, 0x000CCC19,
		0x0B4, 0x00043003,
		0x0B6, 0x0004953E,
		0x0B7, 0x0001C718,
		0x0B8, 0x000060FF,
		0x0B9, 0x00080001,
		0x0BA, 0x00040000,
		0x0BB, 0x00000400,
		0x0BF, 0x000C0000,
		0x0C2, 0x00002400,
		0x0C3, 0x00000009,
		0x0C4, 0x00040C91,
		0x0C5, 0x00099999,
		0x0C6, 0x000000A3,
		0x0C7, 0x00088820,
		0x0C8, 0x00076C06,
		0x0C9, 0x00000000,
		0x0CA, 0x00080000,
		0x0DF, 0x00000180,
		0x0EF, 0x000001A0,
		0x051, 0x0006B27D,
		0xFF0F041F, 0xABCD,
		0x052, 0x0007E4DD,
		0xCDCDCDCD, 0xCDCD,
		0x052, 0x0007E49D,
		0xFF0F041F, 0xDEAD,
		0x053, 0x00000073,
		0x056, 0x00051FF3,
		0x035, 0x00000086,
		0x035, 0x00000186,
		0x035, 0x00000286,
		0x036, 0x00001C25,
		0x036, 0x00009C25,
		0x036, 0x00011C25,
		0x036, 0x00019C25,
		0x0B6, 0x00048538,
		0x018, 0x00000C07,
		0x05A, 0x0004BD00,
		0x019, 0x000739D0,
		0x034, 0x0000ADF3,
		0x034, 0x00009DF0,
		0x034, 0x00008DED,
		0x034, 0x00007DEA,
		0x034, 0x00006DE7,
		0x034, 0x000054EE,
		0x034, 0x000044EB,
		0x034, 0x000034E8,
		0x034, 0x0000246B,
		0x034, 0x00001468,
		0x034, 0x0000006D,
		0x000, 0x00030159,
		0x084, 0x00068200,
		0x086, 0x000000CE,
		0x087, 0x00048A00,
		0x08E, 0x00065540,
		0x08F, 0x00088000,
		0x0EF, 0x000020A0,
		0x03B, 0x000F02B0,
		0x03B, 0x000EF7B0,
		0x03B, 0x000D4FB0,
		0x03B, 0x000CF060,
		0x03B, 0x000B0090,
		0x03B, 0x000A0080,
		0x03B, 0x00090080,
		0x03B, 0x0008F780,
		0x03B, 0x000722B0,
		0x03B, 0x0006F7B0,
		0x03B, 0x00054FB0,
		0x03B, 0x0004F060,
		0x03B, 0x00030090,
		0x03B, 0x00020080,
		0x03B, 0x00010080,
		0x03B, 0x0000F780,
		0x0EF, 0x000000A0,
		0x000, 0x00010159,
		0x018, 0x0000F407,
		0xFFE, 0x00000000,
		0xFFE, 0x00000000,
		0x01F, 0x00080003,
		0xFFE, 0x00000000,
		0xFFE, 0x00000000,
		0x01E, 0x00000001,
		0x01F, 0x00080000,
		0x000, 0x00033E60,
};

u32 RTL8188EUMAC_1T_ARRAY[] = {
		0x026, 0x00000041,
		0x027, 0x00000035,
		0x428, 0x0000000A,
		0x429, 0x00000010,
		0x430, 0x00000000,
		0x431, 0x00000001,
		0x432, 0x00000002,
		0x433, 0x00000004,
		0x434, 0x00000005,
		0x435, 0x00000006,
		0x436, 0x00000007,
		0x437, 0x00000008,
		0x438, 0x00000000,
		0x439, 0x00000000,
		0x43A, 0x00000001,
		0x43B, 0x00000002,
		0x43C, 0x00000004,
		0x43D, 0x00000005,
		0x43E, 0x00000006,
		0x43F, 0x00000007,
		0x440, 0x0000005D,
		0x441, 0x00000001,
		0x442, 0x00000000,
		0x444, 0x00000015,
		0x445, 0x000000F0,
		0x446, 0x0000000F,
		0x447, 0x00000000,
		0x458, 0x00000041,
		0x459, 0x000000A8,
		0x45A, 0x00000072,
		0x45B, 0x000000B9,
		0x460, 0x00000066,
		0x461, 0x00000066,
		0x480, 0x00000008,
		0x4C8, 0x000000FF,
		0x4C9, 0x00000008,
		0x4CC, 0x000000FF,
		0x4CD, 0x000000FF,
		0x4CE, 0x00000001,
		0x4D3, 0x00000001,
		0x500, 0x00000026,
		0x501, 0x000000A2,
		0x502, 0x0000002F,
		0x503, 0x00000000,
		0x504, 0x00000028,
		0x505, 0x000000A3,
		0x506, 0x0000005E,
		0x507, 0x00000000,
		0x508, 0x0000002B,
		0x509, 0x000000A4,
		0x50A, 0x0000005E,
		0x50B, 0x00000000,
		0x50C, 0x0000004F,
		0x50D, 0x000000A4,
		0x50E, 0x00000000,
		0x50F, 0x00000000,
		0x512, 0x0000001C,
		0x514, 0x0000000A,
		0x516, 0x0000000A,
		0x525, 0x0000004F,
		0x550, 0x00000010,
		0x551, 0x00000010,
		0x559, 0x00000002,
		0x55D, 0x000000FF,
		0x605, 0x00000030,
		0x608, 0x0000000E,
		0x609, 0x0000002A,
		0x620, 0x000000FF,
		0x621, 0x000000FF,
		0x622, 0x000000FF,
		0x623, 0x000000FF,
		0x624, 0x000000FF,
		0x625, 0x000000FF,
		0x626, 0x000000FF,
		0x627, 0x000000FF,
		0x652, 0x00000020,
		0x63C, 0x0000000A,
		0x63D, 0x0000000A,
		0x63E, 0x0000000E,
		0x63F, 0x0000000E,
		0x640, 0x00000040,
		0x66E, 0x00000005,
		0x700, 0x00000021,
		0x701, 0x00000043,
		0x702, 0x00000065,
		0x703, 0x00000087,
		0x708, 0x00000021,
		0x709, 0x00000043,
		0x70A, 0x00000065,
		0x70B, 0x00000087,
};

u32 RTL8188EUAGCTAB_1TARRAY[] = {
		0xC78, 0xFB000001,
		0xC78, 0xFB010001,
		0xC78, 0xFB020001,
		0xC78, 0xFB030001,
		0xC78, 0xFB040001,
		0xC78, 0xFB050001,
		0xC78, 0xFA060001,
		0xC78, 0xF9070001,
		0xC78, 0xF8080001,
		0xC78, 0xF7090001,
		0xC78, 0xF60A0001,
		0xC78, 0xF50B0001,
		0xC78, 0xF40C0001,
		0xC78, 0xF30D0001,
		0xC78, 0xF20E0001,
		0xC78, 0xF10F0001,
		0xC78, 0xF0100001,
		0xC78, 0xEF110001,
		0xC78, 0xEE120001,
		0xC78, 0xED130001,
		0xC78, 0xEC140001,
		0xC78, 0xEB150001,
		0xC78, 0xEA160001,
		0xC78, 0xE9170001,
		0xC78, 0xE8180001,
		0xC78, 0xE7190001,
		0xC78, 0xE61A0001,
		0xC78, 0xE51B0001,
		0xC78, 0xE41C0001,
		0xC78, 0xE31D0001,
		0xC78, 0xE21E0001,
		0xC78, 0xE11F0001,
		0xC78, 0x8A200001,
		0xC78, 0x89210001,
		0xC78, 0x88220001,
		0xC78, 0x87230001,
		0xC78, 0x86240001,
		0xC78, 0x85250001,
		0xC78, 0x84260001,
		0xC78, 0x83270001,
		0xC78, 0x82280001,
		0xC78, 0x6B290001,
		0xC78, 0x6A2A0001,
		0xC78, 0x692B0001,
		0xC78, 0x682C0001,
		0xC78, 0x672D0001,
		0xC78, 0x662E0001,
		0xC78, 0x652F0001,
		0xC78, 0x64300001,
		0xC78, 0x63310001,
		0xC78, 0x62320001,
		0xC78, 0x61330001,
		0xC78, 0x46340001,
		0xC78, 0x45350001,
		0xC78, 0x44360001,
		0xC78, 0x43370001,
		0xC78, 0x42380001,
		0xC78, 0x41390001,
		0xC78, 0x403A0001,
		0xC78, 0x403B0001,
		0xC78, 0x403C0001,
		0xC78, 0x403D0001,
		0xC78, 0x403E0001,
		0xC78, 0x403F0001,
		0xC78, 0xFB400001,
		0xC78, 0xFB410001,
		0xC78, 0xFB420001,
		0xC78, 0xFB430001,
		0xC78, 0xFB440001,
		0xC78, 0xFB450001,
		0xC78, 0xFB460001,
		0xC78, 0xFB470001,
		0xC78, 0xFB480001,
		0xC78, 0xFA490001,
		0xC78, 0xF94A0001,
		0xC78, 0xF84B0001,
		0xC78, 0xF74C0001,
		0xC78, 0xF64D0001,
		0xC78, 0xF54E0001,
		0xC78, 0xF44F0001,
		0xC78, 0xF3500001,
		0xC78, 0xF2510001,
		0xC78, 0xF1520001,
		0xC78, 0xF0530001,
		0xC78, 0xEF540001,
		0xC78, 0xEE550001,
		0xC78, 0xED560001,
		0xC78, 0xEC570001,
		0xC78, 0xEB580001,
		0xC78, 0xEA590001,
		0xC78, 0xE95A0001,
		0xC78, 0xE85B0001,
		0xC78, 0xE75C0001,
		0xC78, 0xE65D0001,
		0xC78, 0xE55E0001,
		0xC78, 0xE45F0001,
		0xC78, 0xE3600001,
		0xC78, 0xE2610001,
		0xC78, 0xC3620001,
		0xC78, 0xC2630001,
		0xC78, 0xC1640001,
		0xC78, 0x8B650001,
		0xC78, 0x8A660001,
		0xC78, 0x89670001,
		0xC78, 0x88680001,
		0xC78, 0x87690001,
		0xC78, 0x866A0001,
		0xC78, 0x856B0001,
		0xC78, 0x846C0001,
		0xC78, 0x676D0001,
		0xC78, 0x666E0001,
		0xC78, 0x656F0001,
		0xC78, 0x64700001,
		0xC78, 0x63710001,
		0xC78, 0x62720001,
		0xC78, 0x61730001,
		0xC78, 0x60740001,
		0xC78, 0x46750001,
		0xC78, 0x45760001,
		0xC78, 0x44770001,
		0xC78, 0x43780001,
		0xC78, 0x42790001,
		0xC78, 0x417A0001,
		0xC78, 0x407B0001,
		0xC78, 0x407C0001,
		0xC78, 0x407D0001,
		0xC78, 0x407E0001,
		0xC78, 0x407F0001,

};


static struct rtl8xxxu_power_base rtl8188eu_power_base = {
	.reg_0e00 = 0x07090c0c,
	.reg_0e04 = 0x01020405,
	.reg_0e08 = 0x00000000,
	.reg_086c = 0x00000000,

	.reg_0e10 = 0x0b0c0c0e,
	.reg_0e14 = 0x01030506,
	.reg_0e18 = 0x0b0c0d0e,
	.reg_0e1c = 0x01030509,

	.reg_0830 = 0x07090c0c,
	.reg_0834 = 0x01020405,
	.reg_0838 = 0x00000000,
	.reg_086c_2 = 0x00000000,

	.reg_083c = 0x0b0c0d0e,
	.reg_0848 = 0x01030509,
	.reg_084c = 0x0b0c0d0e,
	.reg_0868 = 0x01030509,
};

static const u32 ofdmswing_table[OFDM_TABLE_SIZE] = {
	0x7f8001fe,		/* 0, +6.0dB */
	0x788001e2,		/* 1, +5.5dB */
	0x71c001c7,		/* 2, +5.0dB */
	0x6b8001ae,		/* 3, +4.5dB */
	0x65400195,		/* 4, +4.0dB */
	0x5fc0017f,		/* 5, +3.5dB */
	0x5a400169,		/* 6, +3.0dB */
	0x55400155,		/* 7, +2.5dB */
	0x50800142,		/* 8, +2.0dB */
	0x4c000130,		/* 9, +1.5dB */
	0x47c0011f,		/* 10, +1.0dB */
	0x43c0010f,		/* 11, +0.5dB */
	0x40000100,		/* 12, +0dB */
	0x3c8000f2,		/* 13, -0.5dB */
	0x390000e4,		/* 14, -1.0dB */
	0x35c000d7,		/* 15, -1.5dB */
	0x32c000cb,		/* 16, -2.0dB */
	0x300000c0,		/* 17, -2.5dB */
	0x2d4000b5,		/* 18, -3.0dB */
	0x2ac000ab,		/* 19, -3.5dB */
	0x288000a2,		/* 20, -4.0dB */
	0x26000098,		/* 21, -4.5dB */
	0x24000090,		/* 22, -5.0dB */
	0x22000088,		/* 23, -5.5dB */
	0x20000080,		/* 24, -6.0dB */
	0x1e400079,		/* 25, -6.5dB */
	0x1c800072,		/* 26, -7.0dB */
	0x1b00006c,		/* 27. -7.5dB */
	0x19800066,		/* 28, -8.0dB */
	0x18000060,		/* 29, -8.5dB */
	0x16c0005b,		/* 30, -9.0dB */
	0x15800056,		/* 31, -9.5dB */
	0x14400051,		/* 32, -10.0dB */
	0x1300004c,		/* 33, -10.5dB */
	0x12000048,		/* 34, -11.0dB */
	0x11000044,		/* 35, -11.5dB */
	0x10000040,		/* 36, -12.0dB */
	0x0f00003c,		/* 37, -12.5dB */
	0x0e400039,		/* 38, -13.0dB */
	0x0d800036,		/* 39, -13.5dB */
	0x0cc00033,		/* 40, -14.0dB */
	0x0c000030,		/* 41, -14.5dB */
	0x0b40002d,		/* 42, -15.0dB */
};

static const u8 cck_tbl_ch1_13[CCK_TABLE_SIZE][8] = {
	{0x36, 0x35, 0x2e, 0x25, 0x1c, 0x12, 0x09, 0x04},	/* 0, +0dB */
	{0x33, 0x32, 0x2b, 0x23, 0x1a, 0x11, 0x08, 0x04},	/* 1, -0.5dB */
	{0x30, 0x2f, 0x29, 0x21, 0x19, 0x10, 0x08, 0x03},	/* 2, -1.0dB */
	{0x2d, 0x2d, 0x27, 0x1f, 0x18, 0x0f, 0x08, 0x03},	/* 3, -1.5dB */
	{0x2b, 0x2a, 0x25, 0x1e, 0x16, 0x0e, 0x07, 0x03},	/* 4, -2.0dB */
	{0x28, 0x28, 0x22, 0x1c, 0x15, 0x0d, 0x07, 0x03},	/* 5, -2.5dB */
	{0x26, 0x25, 0x21, 0x1b, 0x14, 0x0d, 0x06, 0x03},	/* 6, -3.0dB */
	{0x24, 0x23, 0x1f, 0x19, 0x13, 0x0c, 0x06, 0x03},	/* 7, -3.5dB */
	{0x22, 0x21, 0x1d, 0x18, 0x11, 0x0b, 0x06, 0x02},	/* 8, -4.0dB */
	{0x20, 0x20, 0x1b, 0x16, 0x11, 0x08, 0x05, 0x02},	/* 9, -4.5dB */
	{0x1f, 0x1e, 0x1a, 0x15, 0x10, 0x0a, 0x05, 0x02},	/* 10, -5.0dB */
	{0x1d, 0x1c, 0x18, 0x14, 0x0f, 0x0a, 0x05, 0x02},	/* 11, -5.5dB */
	{0x1b, 0x1a, 0x17, 0x13, 0x0e, 0x09, 0x04, 0x02},	/* 12, -6.0dB */
	{0x1a, 0x19, 0x16, 0x12, 0x0d, 0x09, 0x04, 0x02},	/* 13, -6.5dB */
	{0x18, 0x17, 0x15, 0x11, 0x0c, 0x08, 0x04, 0x02},	/* 14, -7.0dB */
	{0x17, 0x16, 0x13, 0x10, 0x0c, 0x08, 0x04, 0x02},	/* 15, -7.5dB */
	{0x16, 0x15, 0x12, 0x0f, 0x0b, 0x07, 0x04, 0x01},	/* 16, -8.0dB */
	{0x14, 0x14, 0x11, 0x0e, 0x0b, 0x07, 0x03, 0x02},	/* 17, -8.5dB */
	{0x13, 0x13, 0x10, 0x0d, 0x0a, 0x06, 0x03, 0x01},	/* 18, -9.0dB */
	{0x12, 0x12, 0x0f, 0x0c, 0x09, 0x06, 0x03, 0x01},	/* 19, -9.5dB */
	{0x11, 0x11, 0x0f, 0x0c, 0x09, 0x06, 0x03, 0x01},	/* 20, -10.0dB*/
	{0x10, 0x10, 0x0e, 0x0b, 0x08, 0x05, 0x03, 0x01},	/* 21, -10.5dB*/
	{0x0f, 0x0f, 0x0d, 0x0b, 0x08, 0x05, 0x03, 0x01},	/* 22, -11.0dB*/
	{0x0e, 0x0e, 0x0c, 0x0a, 0x08, 0x05, 0x02, 0x01},	/* 23, -11.5dB*/
	{0x0d, 0x0d, 0x0c, 0x0a, 0x07, 0x05, 0x02, 0x01},	/* 24, -12.0dB*/
	{0x0d, 0x0c, 0x0b, 0x09, 0x07, 0x04, 0x02, 0x01},	/* 25, -12.5dB*/
	{0x0c, 0x0c, 0x0a, 0x09, 0x06, 0x04, 0x02, 0x01},	/* 26, -13.0dB*/
	{0x0b, 0x0b, 0x0a, 0x08, 0x06, 0x04, 0x02, 0x01},	/* 27, -13.5dB*/
	{0x0b, 0x0a, 0x09, 0x08, 0x06, 0x04, 0x02, 0x01},	/* 28, -14.0dB*/
	{0x0a, 0x0a, 0x09, 0x07, 0x05, 0x03, 0x02, 0x01},	/* 29, -14.5dB*/
	{0x0a, 0x09, 0x08, 0x07, 0x05, 0x03, 0x02, 0x01},	/* 30, -15.0dB*/
	{0x09, 0x09, 0x08, 0x06, 0x05, 0x03, 0x01, 0x01},	/* 31, -15.5dB*/
	{0x09, 0x08, 0x07, 0x06, 0x04, 0x03, 0x01, 0x01}	/* 32, -16.0dB*/
};

static const u8 cck_tbl_ch14[CCK_TABLE_SIZE][8] = {
	{0x36, 0x35, 0x2e, 0x1b, 0x00, 0x00, 0x00, 0x00},	/* 0, +0dB */
	{0x33, 0x32, 0x2b, 0x19, 0x00, 0x00, 0x00, 0x00},	/* 1, -0.5dB */
	{0x30, 0x2f, 0x29, 0x18, 0x00, 0x00, 0x00, 0x00},	/* 2, -1.0dB */
	{0x2d, 0x2d, 0x17, 0x17, 0x00, 0x00, 0x00, 0x00},	/* 3, -1.5dB */
	{0x2b, 0x2a, 0x25, 0x15, 0x00, 0x00, 0x00, 0x00},	/* 4, -2.0dB */
	{0x28, 0x28, 0x24, 0x14, 0x00, 0x00, 0x00, 0x00},	/* 5, -2.5dB */
	{0x26, 0x25, 0x21, 0x13, 0x00, 0x00, 0x00, 0x00},	/* 6, -3.0dB */
	{0x24, 0x23, 0x1f, 0x12, 0x00, 0x00, 0x00, 0x00},	/* 7, -3.5dB */
	{0x22, 0x21, 0x1d, 0x11, 0x00, 0x00, 0x00, 0x00},	/* 8, -4.0dB */
	{0x20, 0x20, 0x1b, 0x10, 0x00, 0x00, 0x00, 0x00},	/* 9, -4.5dB */
	{0x1f, 0x1e, 0x1a, 0x0f, 0x00, 0x00, 0x00, 0x00},	/* 10, -5.0dB */
	{0x1d, 0x1c, 0x18, 0x0e, 0x00, 0x00, 0x00, 0x00},	/* 11, -5.5dB */
	{0x1b, 0x1a, 0x17, 0x0e, 0x00, 0x00, 0x00, 0x00},	/* 12, -6.0dB */
	{0x1a, 0x19, 0x16, 0x0d, 0x00, 0x00, 0x00, 0x00},	/* 13, -6.5dB */
	{0x18, 0x17, 0x15, 0x0c, 0x00, 0x00, 0x00, 0x00},	/* 14, -7.0dB */
	{0x17, 0x16, 0x13, 0x0b, 0x00, 0x00, 0x00, 0x00},	/* 15, -7.5dB */
	{0x16, 0x15, 0x12, 0x0b, 0x00, 0x00, 0x00, 0x00},	/* 16, -8.0dB */
	{0x14, 0x14, 0x11, 0x0a, 0x00, 0x00, 0x00, 0x00},	/* 17, -8.5dB */
	{0x13, 0x13, 0x10, 0x0a, 0x00, 0x00, 0x00, 0x00},	/* 18, -9.0dB */
	{0x12, 0x12, 0x0f, 0x09, 0x00, 0x00, 0x00, 0x00},	/* 19, -9.5dB */
	{0x11, 0x11, 0x0f, 0x09, 0x00, 0x00, 0x00, 0x00},	/* 20, -10.0dB*/
	{0x10, 0x10, 0x0e, 0x08, 0x00, 0x00, 0x00, 0x00},	/* 21, -10.5dB*/
	{0x0f, 0x0f, 0x0d, 0x08, 0x00, 0x00, 0x00, 0x00},	/* 22, -11.0dB*/
	{0x0e, 0x0e, 0x0c, 0x07, 0x00, 0x00, 0x00, 0x00},	/* 23, -11.5dB*/
	{0x0d, 0x0d, 0x0c, 0x07, 0x00, 0x00, 0x00, 0x00},	/* 24, -12.0dB*/
	{0x0d, 0x0c, 0x0b, 0x06, 0x00, 0x00, 0x00, 0x00},	/* 25, -12.5dB*/
	{0x0c, 0x0c, 0x0a, 0x06, 0x00, 0x00, 0x00, 0x00},	/* 26, -13.0dB*/
	{0x0b, 0x0b, 0x0a, 0x06, 0x00, 0x00, 0x00, 0x00},	/* 27, -13.5dB*/
	{0x0b, 0x0a, 0x09, 0x05, 0x00, 0x00, 0x00, 0x00},	/* 28, -14.0dB*/
	{0x0a, 0x0a, 0x09, 0x05, 0x00, 0x00, 0x00, 0x00},	/* 29, -14.5dB*/
	{0x0a, 0x09, 0x08, 0x05, 0x00, 0x00, 0x00, 0x00},	/* 30, -15.0dB*/
	{0x09, 0x09, 0x08, 0x05, 0x00, 0x00, 0x00, 0x00},	/* 31, -15.5dB*/
	{0x09, 0x08, 0x07, 0x04, 0x00, 0x00, 0x00, 0x00}	/* 32, -16.0dB*/
};

/* 임시 */
void rtl88e_phy_lc_calibrate(struct rtl8xxxu_priv *priv, bool is2t);
void rtl88e_phy_set_txpower_level(struct rtl8xxxu_priv *priv, int channel, bool ht40);
void rtl88e_phy_iq_calibrate(struct rtl8xxxu_priv *priv);


int reg_bcn_ctrl_val;
spinlock_t rf_lock;
u8 last_hmeboxnum;

struct rtl_efuse rtlefuse;
struct rtl_phy rtlphy;
struct rtl_dm rtldm;
struct dig_t dm_dig;
struct false_alarm_statistics falsealm_cnt;
struct rate_adaptive p_ra;
struct ps_t dm_pstable;

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
		tmplong2 = rtl_get_bbreg(priv, pphyreg->rfhssi_para2, MASKDWORD);
	tmplong2 = (tmplong2 & (~BLSSIREADADDRESS)) |
	    (newoffset << 23) | BLSSIREADEDGE;
	rtl_set_bbreg(priv, RFPGA0_XA_HSSIPARAMETER2, MASKDWORD,
		      tmplong & (~BLSSIREADEDGE));
	mdelay(1);
	rtl_set_bbreg(priv, pphyreg->rfhssi_para2, MASKDWORD, tmplong2);
	mdelay(2);
	if (rfpath == RF90_PATH_A)
		rfpi_enable = (u8)rtl_get_bbreg(priv, RFPGA0_XA_HSSIPARAMETER1,
						BIT(8));
	else if (rfpath == RF90_PATH_B)
		rfpi_enable = (u8)rtl_get_bbreg(priv, RFPGA0_XB_HSSIPARAMETER1,
						BIT(8));
	if (rfpi_enable)
		retvalue = rtl_get_bbreg(priv, pphyreg->rf_rbpi,
					 BLSSIREADBACKDATA);
	else
		retvalue = rtl_get_bbreg(priv, pphyreg->rf_rb,
					 BLSSIREADBACKDATA);
	RT_TRACE(priv, COMP_RF, DBG_TRACE,
		 "RFR-%d Addr[0x%x]=0x%x\n",
		 rfpath, pphyreg->rf_rb, retvalue);
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
	rtl_set_bbreg(priv, pphyreg->rf3wire_offset, MASKDWORD, data_and_addr);
	RT_TRACE(priv, COMP_RF, DBG_TRACE,
		 "RFW-%d Addr[0x%x]=0x%x\n",
		 rfpath, pphyreg->rf3wire_offset, data_and_addr);
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

///////////////////////////DM//////////////////////////////////////////////////
#define	CAL_SWING_OFF(_off, _dir, _size, _del)				\
	do {								\
		for (_off = 0; _off < _size; _off++) {			\
			if (_del < thermal_threshold[_dir][_off]) {	\
				if (_off != 0)				\
					_off--;				\
				break;					\
			}						\
		}							\
		if (_off >= _size)					\
			_off = _size - 1;				\
	} while (0)

static void rtl88e_set_iqk_matrix(struct rtl8xxxu_priv *priv,
				  u8 ofdm_index, u8 rfpath,
				  long iqk_result_x, long iqk_result_y)
{
	long ele_a = 0, ele_d, ele_c = 0, value32;

	ele_d = (ofdmswing_table[ofdm_index] & 0xFFC00000)>>22;

	if (iqk_result_x != 0) {
		if ((iqk_result_x & 0x00000200) != 0)
			iqk_result_x = iqk_result_x | 0xFFFFFC00;
		ele_a = ((iqk_result_x * ele_d)>>8)&0x000003FF;

		if ((iqk_result_y & 0x00000200) != 0)
			iqk_result_y = iqk_result_y | 0xFFFFFC00;
		ele_c = ((iqk_result_y * ele_d)>>8)&0x000003FF;

		switch (rfpath) {
		case RF90_PATH_A:
			value32 = (ele_d << 22)|((ele_c & 0x3F)<<16) | ele_a;
			rtl_set_bbreg(priv, ROFDM0_XATXIQIMBALANCE,
				      MASKDWORD, value32);
			value32 = (ele_c & 0x000003C0) >> 6;
			rtl_set_bbreg(priv, ROFDM0_XCTXAFE, MASKH4BITS,
				      value32);
			value32 = ((iqk_result_x * ele_d) >> 7) & 0x01;
			rtl_set_bbreg(priv, ROFDM0_ECCATHRESHOLD, BIT(24),
				      value32);
			break;
		case RF90_PATH_B:
			value32 = (ele_d << 22)|((ele_c & 0x3F)<<16) | ele_a;
			rtl_set_bbreg(priv, ROFDM0_XBTXIQIMBALANCE, MASKDWORD,
				      value32);
			value32 = (ele_c & 0x000003C0) >> 6;
			rtl_set_bbreg(priv, ROFDM0_XDTXAFE, MASKH4BITS, value32);
			value32 = ((iqk_result_x * ele_d) >> 7) & 0x01;
			rtl_set_bbreg(priv, ROFDM0_ECCATHRESHOLD, BIT(28),
				      value32);
			break;
		default:
			break;
		}
	} else {
		switch (rfpath) {
		case RF90_PATH_A:
			rtl_set_bbreg(priv, ROFDM0_XATXIQIMBALANCE,
				      MASKDWORD, ofdmswing_table[ofdm_index]);
			rtl_set_bbreg(priv, ROFDM0_XCTXAFE,
				      MASKH4BITS, 0x00);
			rtl_set_bbreg(priv, ROFDM0_ECCATHRESHOLD,
				      BIT(24), 0x00);
			break;
		case RF90_PATH_B:
			rtl_set_bbreg(priv, ROFDM0_XBTXIQIMBALANCE,
				      MASKDWORD, ofdmswing_table[ofdm_index]);
			rtl_set_bbreg(priv, ROFDM0_XDTXAFE,
				      MASKH4BITS, 0x00);
			rtl_set_bbreg(priv, ROFDM0_ECCATHRESHOLD,
				      BIT(28), 0x00);
			break;
		default:
			break;
		}
	}
}

///////////////////////////////////////START_OF_DM///////////////////////////////////////////

void rtl88e_dm_txpower_track_adjust(struct rtl8xxxu_priv *priv,
	u8 type, u8 *pdirection, u32 *poutwrite_val)
{
	u8 pwr_val = 0;
	u8 cck_base = rtldm.swing_idx_cck_base;
	u8 cck_val = rtldm.swing_idx_cck;
	u8 ofdm_base = rtldm.swing_idx_ofdm_base[0];
	u8 ofdm_val = rtldm.swing_idx_ofdm[RF90_PATH_A];

	if (type == 0) {
		if (ofdm_val <= ofdm_base) {
			*pdirection = 1;
			pwr_val = ofdm_base - ofdm_val;
		} else {
			*pdirection = 2;
			pwr_val = ofdm_base - ofdm_val;
		}
	} else if (type == 1) {
		if (cck_val <= cck_base) {
			*pdirection = 1;
			pwr_val = cck_base - cck_val;
		} else {
			*pdirection = 2;
			pwr_val = cck_val - cck_base;
		}
	}

	if (pwr_val >= TXPWRTRACK_MAX_IDX && (*pdirection == 1))
		pwr_val = TXPWRTRACK_MAX_IDX;

	*poutwrite_val = pwr_val | (pwr_val << 8) | (pwr_val << 16) |
			 (pwr_val << 24);
}

static void dm_tx_pwr_track_set_pwr(struct rtl8xxxu_priv *priv,
				    enum pwr_track_control_method method,
				    u8 rfpath, u8 channel_mapped_index)
{
	if (method == TXAGC) {
		if (rtldm.swing_flag_ofdm ||
		    rtldm.swing_flag_cck) {
			rtl88e_phy_set_txpower_level(priv,
						     rtlphy.current_channel, false);
			rtldm.swing_flag_ofdm = false;
			rtldm.swing_flag_cck = false;
		}
	} else if (method == BBSWING) {
		if (!rtldm.cck_inch14) {
			rtl8xxxu_write8(priv, 0xa22,
				       cck_tbl_ch1_13[rtldm.swing_idx_cck][0]);
			rtl8xxxu_write8(priv, 0xa23,
				       cck_tbl_ch1_13[rtldm.swing_idx_cck][1]);
			rtl8xxxu_write8(priv, 0xa24,
				       cck_tbl_ch1_13[rtldm.swing_idx_cck][2]);
			rtl8xxxu_write8(priv, 0xa25,
				       cck_tbl_ch1_13[rtldm.swing_idx_cck][3]);
			rtl8xxxu_write8(priv, 0xa26,
				       cck_tbl_ch1_13[rtldm.swing_idx_cck][4]);
			rtl8xxxu_write8(priv, 0xa27,
				       cck_tbl_ch1_13[rtldm.swing_idx_cck][5]);
			rtl8xxxu_write8(priv, 0xa28,
				       cck_tbl_ch1_13[rtldm.swing_idx_cck][6]);
			rtl8xxxu_write8(priv, 0xa29,
				       cck_tbl_ch1_13[rtldm.swing_idx_cck][7]);
		} else {
			rtl8xxxu_write8(priv, 0xa22,
				       cck_tbl_ch14[rtldm.swing_idx_cck][0]);
			rtl8xxxu_write8(priv, 0xa23,
				       cck_tbl_ch14[rtldm.swing_idx_cck][1]);
			rtl8xxxu_write8(priv, 0xa24,
				       cck_tbl_ch14[rtldm.swing_idx_cck][2]);
			rtl8xxxu_write8(priv, 0xa25,
				       cck_tbl_ch14[rtldm.swing_idx_cck][3]);
			rtl8xxxu_write8(priv, 0xa26,
				       cck_tbl_ch14[rtldm.swing_idx_cck][4]);
			rtl8xxxu_write8(priv, 0xa27,
				       cck_tbl_ch14[rtldm.swing_idx_cck][5]);
			rtl8xxxu_write8(priv, 0xa28,
				       cck_tbl_ch14[rtldm.swing_idx_cck][6]);
			rtl8xxxu_write8(priv, 0xa29,
				       cck_tbl_ch14[rtldm.swing_idx_cck][7]);
		}

		if (rfpath == RF90_PATH_A) {
			rtl88e_set_iqk_matrix(priv, rtldm.swing_idx_ofdm[rfpath],
					      rfpath, rtlphy.iqk_matrix
					      [channel_mapped_index].
					      value[0][0],
					      rtlphy.iqk_matrix
					      [channel_mapped_index].
					      value[0][1]);
		} else if (rfpath == RF90_PATH_B) {
			rtl88e_set_iqk_matrix(priv, rtldm.swing_idx_ofdm[rfpath],
					      rfpath, rtlphy.iqk_matrix
					      [channel_mapped_index].
					      value[0][4],
					      rtlphy.iqk_matrix
					      [channel_mapped_index].
					      value[0][5]);
		}
	} else {
		return;
	}
}

static u8 rtl88e_dm_initial_gain_min_pwdb(struct rtl8xxxu_priv *priv)
{
	long rssi_val_min = 0;

	if ((dm_dig.curmultista_cstate == DIG_MULTISTA_CONNECT) &&
	    (dm_dig.cur_sta_cstate == DIG_STA_CONNECT)) {
		if (rtldm.entry_min_undec_sm_pwdb != 0)
			rssi_val_min =
			    (rtldm.entry_min_undec_sm_pwdb >
			     rtldm.undec_sm_pwdb) ?
			    rtldm.undec_sm_pwdb :
			    rtldm.entry_min_undec_sm_pwdb;
		else
			rssi_val_min = rtldm.undec_sm_pwdb;
	} else if (dm_dig.cur_sta_cstate == DIG_STA_CONNECT ||
		   dm_dig.cur_sta_cstate == DIG_STA_BEFORE_CONNECT) {
		rssi_val_min = rtldm.undec_sm_pwdb;
	} else if (dm_dig.curmultista_cstate ==
		DIG_MULTISTA_CONNECT) {
		rssi_val_min = rtldm.entry_min_undec_sm_pwdb;
	}

	return (u8)rssi_val_min;
}
#if 0
static void rtl88e_dm_false_alarm_counter_statistics(struct rtl8xxxu_priv *priv)
{
	u32 ret_value;

	rtl_set_bbreg(priv, ROFDM0_LSTF, BIT(31), 1);
	rtl_set_bbreg(priv, ROFDM1_LSTF, BIT(31), 1);

	ret_value = rtl_get_bbreg(priv, ROFDM0_FRAMESYNC, MASKDWORD);
	falsealm_cnt.cnt_fast_fsync_fail = (ret_value&0xffff);
	falsealm_cnt.cnt_sb_search_fail = ((ret_value&0xffff0000)>>16);

	ret_value = rtl_get_bbreg(priv, ROFDM_PHYCOUNTER1, MASKDWORD);
	falsealm_cnt.cnt_ofdm_cca = (ret_value&0xffff);
	falsealm_cnt.cnt_parity_fail = ((ret_value & 0xffff0000) >> 16);

	ret_value = rtl_get_bbreg(priv, ROFDM_PHYCOUNTER2, MASKDWORD);
	falsealm_cnt.cnt_rate_illegal = (ret_value & 0xffff);
	falsealm_cnt.cnt_crc8_fail = ((ret_value & 0xffff0000) >> 16);

	ret_value = rtl_get_bbreg(priv, ROFDM_PHYCOUNTER3, MASKDWORD);
	falsealm_cnt.cnt_mcs_fail = (ret_value & 0xffff);
	falsealm_cnt.cnt_ofdm_fail = falsealm_cnt.cnt_parity_fail +
		falsealm_cnt.cnt_rate_illegal +
		falsealm_cnt.cnt_crc8_fail +
		falsealm_cnt.cnt_mcs_fail +
		falsealm_cnt.cnt_fast_fsync_fail +
		falsealm_cnt.cnt_sb_search_fail;

	ret_value = rtl_get_bbreg(priv, REG_SC_CNT, MASKDWORD);
	falsealm_cnt.cnt_bw_lsc = (ret_value & 0xffff);
	falsealm_cnt.cnt_bw_usc = ((ret_value & 0xffff0000) >> 16);

	rtl_set_bbreg(priv, RCCK0_FALSEALARMREPORT, BIT(12), 1);
	rtl_set_bbreg(priv, RCCK0_FALSEALARMREPORT, BIT(14), 1);

	ret_value = rtl_get_bbreg(priv, RCCK0_FACOUNTERLOWER, MASKBYTE0);
	falsealm_cnt.cnt_cck_fail = ret_value;

	ret_value = rtl_get_bbreg(priv, RCCK0_FACOUNTERUPPER, MASKBYTE3);
	falsealm_cnt.cnt_cck_fail += (ret_value & 0xff) << 8;

	ret_value = rtl_get_bbreg(priv, RCCK0_CCA_CNT, MASKDWORD);
	falsealm_cnt.cnt_cck_cca = ((ret_value & 0xff) << 8) |
		((ret_value&0xFF00)>>8);

	falsealm_cnt.cnt_all = (falsealm_cnt.cnt_fast_fsync_fail +
				falsealm_cnt.cnt_sb_search_fail +
				falsealm_cnt.cnt_parity_fail +
				falsealm_cnt.cnt_rate_illegal +
				falsealm_cnt.cnt_crc8_fail +
				falsealm_cnt.cnt_mcs_fail +
				falsealm_cnt.cnt_cck_fail);
	falsealm_cnt.cnt_cca_all = falsealm_cnt.cnt_ofdm_cca +
		falsealm_cnt.cnt_cck_cca;

	rtl_set_bbreg(priv, ROFDM0_TRSWISOLATION, BIT(31), 1);
	rtl_set_bbreg(priv, ROFDM0_TRSWISOLATION, BIT(31), 0);
	rtl_set_bbreg(priv, ROFDM1_LSTF, BIT(27), 1);
	rtl_set_bbreg(priv, ROFDM1_LSTF, BIT(27), 0);
	rtl_set_bbreg(priv, ROFDM0_LSTF, BIT(31), 0);
	rtl_set_bbreg(priv, ROFDM1_LSTF, BIT(31), 0);
	rtl_set_bbreg(priv, RCCK0_FALSEALARMREPORT, BIT(13)|BIT(12), 0);
	rtl_set_bbreg(priv, RCCK0_FALSEALARMREPORT, BIT(13)|BIT(12), 2);
	rtl_set_bbreg(priv, RCCK0_FALSEALARMREPORT, BIT(15)|BIT(14), 0);
	rtl_set_bbreg(priv, RCCK0_FALSEALARMREPORT, BIT(15)|BIT(14), 2);

	RT_TRACE(priv, COMP_DIG, DBG_TRACE,
		 "cnt_parity_fail = %d, cnt_rate_illegal = %d, cnt_crc8_fail = %d, cnt_mcs_fail = %d\n",
		 falsealm_cnt.cnt_parity_fail,
		 falsealm_cnt.cnt_rate_illegal,
		 falsealm_cnt.cnt_crc8_fail, falsealm_cnt.cnt_mcs_fail);

	RT_TRACE(priv, COMP_DIG, DBG_TRACE,
		 "cnt_ofdm_fail = %x, cnt_cck_fail = %x, cnt_all = %x\n",
		 falsealm_cnt.cnt_ofdm_fail,
		 falsealm_cnt.cnt_cck_fail, falsealm_cnt.cnt_all);
}
#endif

static void rtl88e_dm_cck_packet_detection_thresh(struct rtl8xxxu_priv *priv)
{
	u8 cur_cck_cca_thresh;

	if (dm_dig.cur_sta_cstate == DIG_STA_CONNECT) {
		dm_dig.rssi_val_min = rtl88e_dm_initial_gain_min_pwdb(priv);
		if (dm_dig.rssi_val_min > 25) {
			cur_cck_cca_thresh = 0xcd;
		} else if ((dm_dig.rssi_val_min <= 25) &&
			   (dm_dig.rssi_val_min > 10)) {
			cur_cck_cca_thresh = 0x83;
		} else {
			if (falsealm_cnt.cnt_cck_fail > 1000)
				cur_cck_cca_thresh = 0x83;
			else
				cur_cck_cca_thresh = 0x40;
		}

	} else {
		if (falsealm_cnt.cnt_cck_fail > 1000)
			cur_cck_cca_thresh = 0x83;
		else
			cur_cck_cca_thresh = 0x40;
	}

	if (dm_dig.cur_cck_cca_thres != cur_cck_cca_thresh)
		rtl_set_bbreg(priv, RCCK0_CCA, MASKBYTE2, cur_cck_cca_thresh);

	dm_dig.cur_cck_cca_thres = cur_cck_cca_thresh;
	dm_dig.pre_cck_cca_thres = dm_dig.cur_cck_cca_thres;
	RT_TRACE(priv, COMP_DIG, DBG_TRACE,
		 "CCK cca thresh hold =%x\n", dm_dig.cur_cck_cca_thres);
}

#if 0
static void rtl88e_dm_dig(struct rtl8xxxu_priv *priv)
{
	u8 dig_dynamic_min, dig_maxofmin;
	bool bfirstconnect;
	u8 dm_dig_max, dm_dig_min;
	u8 current_igi = dm_dig.cur_igvalue;

	if (rtldm.dm_initialgain_enable == false)
		return;
	if (dm_dig.dig_enable_flag == false)
		return;
	if (mac->act_scanning == true)
		return;
	if (mac->link_state >= MAC80211_LINKED)
		dm_dig.cur_sta_cstate = DIG_STA_CONNECT;
	else
		dm_dig.cur_sta_cstate = DIG_STA_DISCONNECT;
	if (rtlpriv->mac80211.opmode == NL80211_IFTYPE_AP ||
	    rtlpriv->mac80211.opmode == NL80211_IFTYPE_ADHOC)
		dm_dig.cur_sta_cstate = DIG_STA_DISCONNECT;

	dm_dig_max = DM_DIG_MAX;
	dm_dig_min = DM_DIG_MIN;
	dig_maxofmin = DM_DIG_MAX_AP;
	dig_dynamic_min = dm_dig.dig_min_0;
	bfirstconnect = ((mac->link_state >= MAC80211_LINKED) ? true : false) &&
			 !dm_dig.media_connect_0;

	dm_dig.rssi_val_min =
		rtl88e_dm_initial_gain_min_pwdb(hw);

	if (mac->link_state >= MAC80211_LINKED) {
		if ((dm_dig.rssi_val_min + 20) > dm_dig_max)
			dm_dig.rx_gain_max = dm_dig_max;
		else if ((dm_dig.rssi_val_min + 20) < dm_dig_min)
			dm_dig.rx_gain_max = dm_dig_min;
		else
			dm_dig.rx_gain_max = dm_dig.rssi_val_min + 20;

		if (rtlefuse.antenna_div_type == CG_TRX_HW_ANTDIV) {
			dig_dynamic_min  = dm_dig.antdiv_rssi_max;
		} else {
			if (dm_dig.rssi_val_min < dm_dig_min)
				dig_dynamic_min = dm_dig_min;
			else if (dm_dig.rssi_val_min < dig_maxofmin)
				dig_dynamic_min = dig_maxofmin;
			else
				dig_dynamic_min = dm_dig.rssi_val_min;
		}
	} else {
		dm_dig.rx_gain_max = dm_dig_max;
		dig_dynamic_min = dm_dig_min;
		RT_TRACE(priv, COMP_DIG, DBG_LOUD, "no link\n");
	}

	if (falsealm_cnt.cnt_all > 10000) {
		dm_dig.large_fa_hit++;
		if (dm_dig.forbidden_igi < current_igi) {
			dm_dig.forbidden_igi = current_igi;
			dm_dig.large_fa_hit = 1;
		}

		if (dm_dig.large_fa_hit >= 3) {
			if ((dm_dig.forbidden_igi + 1) >
				dm_dig.rx_gain_max)
				dm_dig.rx_gain_min =
					dm_dig.rx_gain_max;
			else
				dm_dig.rx_gain_min =
					dm_dig.forbidden_igi + 1;
			dm_dig.recover_cnt = 3600;
		}
	} else {
		if (dm_dig.recover_cnt != 0) {
			dm_dig.recover_cnt--;
		} else {
			if (dm_dig.large_fa_hit == 0) {
				if ((dm_dig.forbidden_igi - 1) <
				    dig_dynamic_min) {
					dm_dig.forbidden_igi = dig_dynamic_min;
					dm_dig.rx_gain_min = dig_dynamic_min;
				} else {
					dm_dig.forbidden_igi--;
					dm_dig.rx_gain_min =
						dm_dig.forbidden_igi + 1;
				}
			} else if (dm_dig.large_fa_hit == 3) {
				dm_dig.large_fa_hit = 0;
			}
		}
	}

	if (dm_dig.cur_sta_cstate == DIG_STA_CONNECT) {
		if (bfirstconnect) {
			current_igi = dm_dig.rssi_val_min;
		} else {
			if (falsealm_cnt.cnt_all > DM_DIG_FA_TH2)
				current_igi += 2;
			else if (falsealm_cnt.cnt_all > DM_DIG_FA_TH1)
				current_igi++;
			else if (falsealm_cnt.cnt_all < DM_DIG_FA_TH0)
				current_igi--;
		}
	} else {
		if (falsealm_cnt.cnt_all > 10000)
			current_igi += 2;
		else if (falsealm_cnt.cnt_all > 8000)
			current_igi++;
		else if (falsealm_cnt.cnt_all < 500)
			current_igi--;
	}

	if (current_igi > DM_DIG_FA_UPPER)
		current_igi = DM_DIG_FA_UPPER;
	else if (current_igi < DM_DIG_FA_LOWER)
		current_igi = DM_DIG_FA_LOWER;

	if (falsealm_cnt.cnt_all > 10000)
		current_igi = DM_DIG_FA_UPPER;

	dm_dig.cur_igvalue = current_igi;
	rtl88e_dm_write_dig(hw);
	dm_dig.media_connect_0 =
		((mac->link_state >= MAC80211_LINKED) ? true : false);
	dm_dig.dig_min_0 = dig_dynamic_min;

	rtl88e_dm_cck_packet_detection_thresh(hw);
}
#endif

static void rtl88e_dm_init_dynamic_txpower(struct rtl8xxxu_priv *priv)
{

	rtldm.dynamic_txpower_enable = false;
	rtldm.last_dtp_lvl = TXHIGHPWRLEVEL_NORMAL;
	rtldm.dynamic_txhighpower_lvl = TXHIGHPWRLEVEL_NORMAL;
}

#if 0
static void rtl92c_dm_dynamic_txpower(struct rtl8xxxu_priv *priv)
{
	struct rtl_phy *rtlphy = rtl_phy(rtlpriv);
	struct rtl_mac *mac = rtl_mac(rtl_priv(hw));
	long undec_sm_pwdb;

	if (!rtldm.dynamic_txpower_enable)
		return;

	if (rtldm.dm_flag & HAL_DM_HIPWR_DISABLE) {
		rtldm.dynamic_txhighpower_lvl = TXHIGHPWRLEVEL_NORMAL;
		return;
	}

	if ((mac->link_state < MAC80211_LINKED) &&
	    (rtldm.entry_min_undec_sm_pwdb == 0)) {
		RT_TRACE(priv, COMP_POWER, DBG_TRACE,
			 "Not connected to any\n");

		rtldm.dynamic_txhighpower_lvl = TXHIGHPWRLEVEL_NORMAL;

		rtldm.last_dtp_lvl = TXHIGHPWRLEVEL_NORMAL;
		return;
	}

	if (mac->link_state >= MAC80211_LINKED) {
		if (mac->opmode == NL80211_IFTYPE_ADHOC) {
			undec_sm_pwdb =
			    rtldm.entry_min_undec_sm_pwdb;
			RT_TRACE(priv, COMP_POWER, DBG_LOUD,
				 "AP Client PWDB = 0x%lx\n",
				  undec_sm_pwdb);
		} else {
			undec_sm_pwdb =
			    rtldm.undec_sm_pwdb;
			RT_TRACE(priv, COMP_POWER, DBG_LOUD,
				 "STA Default Port PWDB = 0x%lx\n",
				  undec_sm_pwdb);
		}
	} else {
		undec_sm_pwdb =
		    rtldm.entry_min_undec_sm_pwdb;

		RT_TRACE(priv, COMP_POWER, DBG_LOUD,
			 "AP Ext Port PWDB = 0x%lx\n",
			  undec_sm_pwdb);
	}

	if (undec_sm_pwdb >= TX_POWER_NEAR_FIELD_THRESH_LVL2) {
		rtldm.dynamic_txhighpower_lvl = TXHIGHPWRLEVEL_LEVEL1;
		RT_TRACE(priv, COMP_POWER, DBG_LOUD,
			 "TXHIGHPWRLEVEL_LEVEL1 (TxPwr = 0x0)\n");
	} else if ((undec_sm_pwdb <
		    (TX_POWER_NEAR_FIELD_THRESH_LVL2 - 3)) &&
		   (undec_sm_pwdb >=
		    TX_POWER_NEAR_FIELD_THRESH_LVL1)) {
		rtldm.dynamic_txhighpower_lvl = TXHIGHPWRLEVEL_LEVEL1;
		RT_TRACE(priv, COMP_POWER, DBG_LOUD,
			 "TXHIGHPWRLEVEL_LEVEL1 (TxPwr = 0x10)\n");
	} else if (undec_sm_pwdb <
		   (TX_POWER_NEAR_FIELD_THRESH_LVL1 - 5)) {
		rtldm.dynamic_txhighpower_lvl = TXHIGHPWRLEVEL_NORMAL;
		RT_TRACE(priv, COMP_POWER, DBG_LOUD,
			 "TXHIGHPWRLEVEL_NORMAL\n");
	}

	if ((rtldm.dynamic_txhighpower_lvl !=
		rtldm.last_dtp_lvl)) {
		RT_TRACE(priv, COMP_POWER, DBG_LOUD,
			 "PHY_SetTxPowerLevel8192S() Channel = %d\n",
			  rtlphy.current_channel);
		rtl88e_phy_set_txpower_level(hw, rtlphy.current_channel);
	}

	rtldm.last_dtp_lvl = rtldm.dynamic_txhighpower_lvl;
}
#endif

void rtl88e_dm_write_dig(struct rtl8xxxu_priv *priv)
{
	RT_TRACE(priv, COMP_DIG, DBG_LOUD,
		 "cur_igvalue = 0x%x, pre_igvalue = 0x%x, backoff_val = %d\n",
		 dm_dig.cur_igvalue, dm_dig.pre_igvalue,
		 dm_dig.back_val);

	if (dm_dig.cur_igvalue > 0x3f)
		dm_dig.cur_igvalue = 0x3f;
	if (dm_dig.pre_igvalue != dm_dig.cur_igvalue) {
		rtl_set_bbreg(priv, ROFDM0_XAAGCCORE1, 0x7f,
			      dm_dig.cur_igvalue);

		dm_dig.pre_igvalue = dm_dig.cur_igvalue;
	}
}
#if 0
static void rtl88e_dm_pwdb_monitor(struct rtl8xxxu_priv *priv)
{
	struct rtl_hal *rtlhal = rtl_hal(rtl_priv(hw));
	struct rtl_sta_info *drv_priv;
	static u64 last_record_txok_cnt;
	static u64 last_record_rxok_cnt;
	long tmp_entry_max_pwdb = 0, tmp_entry_min_pwdb = 0xff;

	if (rtlhal->oem_id == RT_CID_819X_HP) {
		u64 cur_txok_cnt = 0;
		u64 cur_rxok_cnt = 0;
		cur_txok_cnt = rtlpriv->stats.txbytesunicast -
			last_record_txok_cnt;
		cur_rxok_cnt = rtlpriv->stats.rxbytesunicast -
			last_record_rxok_cnt;
		last_record_txok_cnt = cur_txok_cnt;
		last_record_rxok_cnt = cur_rxok_cnt;

		if (cur_rxok_cnt > (cur_txok_cnt * 6))
			rtl8xxxu_write32(priv, REG_ARFR0, 0x8f015);
		else
			rtl8xxxu_write32(priv, REG_ARFR0, 0xff015);
	}

	/* AP & ADHOC & MESH */
	spin_lock_bh(&rtlpriv->locks.entry_list_lock);
	list_for_each_entry(drv_priv, &rtlpriv->entry_list, list) {
		if (drv_priv->rssi_stat.undec_sm_pwdb <
			tmp_entry_min_pwdb)
			tmp_entry_min_pwdb = drv_priv->rssi_stat.undec_sm_pwdb;
		if (drv_priv->rssi_stat.undec_sm_pwdb >
			tmp_entry_max_pwdb)
			tmp_entry_max_pwdb = drv_priv->rssi_stat.undec_sm_pwdb;
	}
	spin_unlock_bh(&rtlpriv->locks.entry_list_lock);

	/* If associated entry is found */
	if (tmp_entry_max_pwdb != 0) {
		rtldm.entry_max_undec_sm_pwdb = tmp_entry_max_pwdb;
		RTPRINT(rtlpriv, FDM, DM_PWDB, "EntryMaxPWDB = 0x%lx(%ld)\n",
			tmp_entry_max_pwdb, tmp_entry_max_pwdb);
	} else {
		rtldm.entry_max_undec_sm_pwdb = 0;
	}
	/* If associated entry is found */
	if (tmp_entry_min_pwdb != 0xff) {
		rtldm.entry_min_undec_sm_pwdb = tmp_entry_min_pwdb;
		RTPRINT(rtlpriv, FDM, DM_PWDB, "EntryMinPWDB = 0x%lx(%ld)\n",
					tmp_entry_min_pwdb, tmp_entry_min_pwdb);
	} else {
		rtldm.entry_min_undec_sm_pwdb = 0;
	}
	/* Indicate Rx signal strength to FW. */
	if (rtldm.useramask) {
		u8 h2c_parameter[3] = { 0 };

		h2c_parameter[2] = (u8)(rtldm.undec_sm_pwdb & 0xFF);
		h2c_parameter[0] = 0x20;
	} else {
		rtl8xxxu_write8(priv, 0x4fe, rtldm.undec_sm_pwdb);
	}
}
#endif

void rtl88e_dm_init_edca_turbo(struct rtl8xxxu_priv *priv)
{

	rtldm.current_turbo_edca = false;
	rtldm.is_any_nonbepkts = false;
	rtldm.is_cur_rdlstate = false;
}

#if 0
static void rtl88e_dm_check_edca_turbo(struct rtl8xxxu_priv *priv)
{
	struct rtl_mac *mac = rtl_mac(rtl_priv(hw));
	static u64 last_txok_cnt;
	static u64 last_rxok_cnt;
	static u32 last_bt_edca_ul;
	static u32 last_bt_edca_dl;
	u64 cur_txok_cnt = 0;
	u64 cur_rxok_cnt = 0;
	u32 edca_be_ul = 0x5ea42b;
	u32 edca_be_dl = 0x5ea42b;
	bool bt_change_edca = false;

	if ((last_bt_edca_ul != rtlpriv->btcoexist.bt_edca_ul) ||
	    (last_bt_edca_dl != rtlpriv->btcoexist.bt_edca_dl)) {
		rtldm.current_turbo_edca = false;
		last_bt_edca_ul = rtlpriv->btcoexist.bt_edca_ul;
		last_bt_edca_dl = rtlpriv->btcoexist.bt_edca_dl;
	}

	if (rtlpriv->btcoexist.bt_edca_ul != 0) {
		edca_be_ul = rtlpriv->btcoexist.bt_edca_ul;
		bt_change_edca = true;
	}

	if (rtlpriv->btcoexist.bt_edca_dl != 0) {
		edca_be_ul = rtlpriv->btcoexist.bt_edca_dl;
		bt_change_edca = true;
	}

	if (mac->link_state != MAC80211_LINKED) {
		rtldm.current_turbo_edca = false;
		return;
	}
	if ((bt_change_edca) ||
	    ((!rtldm.is_any_nonbepkts) &&
	     (!rtldm.disable_framebursting))) {

		cur_txok_cnt = rtlpriv->stats.txbytesunicast - last_txok_cnt;
		cur_rxok_cnt = rtlpriv->stats.rxbytesunicast - last_rxok_cnt;

		if (cur_rxok_cnt > 4 * cur_txok_cnt) {
			if (!rtldm.is_cur_rdlstate ||
			    !rtldm.current_turbo_edca) {
				rtl8xxxu_write32(priv,
						REG_EDCA_BE_PARAM,
						edca_be_dl);
				rtldm.is_cur_rdlstate = true;
			}
		} else {
			if (rtldm.is_cur_rdlstate ||
			    !rtldm.current_turbo_edca) {
				rtl8xxxu_write32(priv,
						REG_EDCA_BE_PARAM,
						edca_be_ul);
				rtldm.is_cur_rdlstate = false;
			}
		}
		rtldm.current_turbo_edca = true;
	} else {
		if (rtldm.current_turbo_edca) {
			u8 tmp = AC0_BE;

			rtlpriv->cfg->ops->set_hw_reg(hw,
						      HW_VAR_AC_PARAM,
						      &tmp);
			rtldm.current_turbo_edca = false;
		}
	}

	rtldm.is_any_nonbepkts = false;
	last_txok_cnt = rtlpriv->stats.txbytesunicast;
	last_rxok_cnt = rtlpriv->stats.rxbytesunicast;
}
#endif


static void dm_txpower_track_cb_therm(struct rtl8xxxu_priv *priv)
{
	u8 thermalvalue = 0, delta, delta_lck, delta_iqk, offset;
	u8 thermalvalue_avg_count = 0;
	u32 thermalvalue_avg = 0;
	long  ele_d, temp_cck;
	char ofdm_index[2], cck_index = 0,
		ofdm_index_old[2] = {0, 0}, cck_index_old = 0;
	int i = 0;
	/*bool is2t = false;*/

	u8 ofdm_min_index = 6, rf = 1;
	/*u8 index_for_channel;*/
	enum _power_dec_inc {power_dec, power_inc};

	/*0.1 the following TWO tables decide the
	 *final index of OFDM/CCK swing table
	 */
	char delta_swing_table_idx[2][15]  = {
		{0, 0, 2, 3, 4, 4, 5, 6, 7, 7, 8, 9, 10, 10, 11},
		{0, 0, -1, -2, -3, -4, -4, -4, -4, -5, -7, -8, -9, -9, -10}
	};
	u8 thermal_threshold[2][15] = {
		{0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 27},
		{0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 25, 25, 25}
	};

	/*Initilization (7 steps in total) */
	rtldm.txpower_trackinginit = true;
	RT_TRACE(priv, COMP_POWER_TRACKING, DBG_LOUD,
		 "dm_txpower_track_cb_therm\n");

	thermalvalue = (u8)rtl_get_rfreg(priv, RF90_PATH_A, RF_T_METER,
					 0xfc00);
	if (!thermalvalue)
		return;
	RT_TRACE(priv, COMP_POWER_TRACKING, DBG_LOUD,
		 "Readback Thermal Meter = 0x%x pre thermal meter 0x%x eeprom_thermalmeter 0x%x\n",
		 thermalvalue, rtldm.thermalvalue,
		 rtlefuse.eeprom_thermalmeter);

	/*1. Query OFDM Default Setting: Path A*/
	ele_d = rtl_get_bbreg(priv, ROFDM0_XATXIQIMBALANCE, MASKDWORD) &
			      MASKOFDM_D;
	for (i = 0; i < OFDM_TABLE_LENGTH; i++) {
		if (ele_d == (ofdmswing_table[i] & MASKOFDM_D)) {
			ofdm_index_old[0] = (u8)i;
			rtldm.swing_idx_ofdm_base[RF90_PATH_A] = (u8)i;
			RT_TRACE(priv, COMP_POWER_TRACKING, DBG_LOUD,
				 "Initial pathA ele_d reg0x%x = 0x%lx, ofdm_index = 0x%x\n",
				 ROFDM0_XATXIQIMBALANCE,
				 ele_d, ofdm_index_old[0]);
			break;
		}
	}

	/*2.Query CCK default setting From 0xa24*/
	temp_cck = rtl_get_bbreg(priv, RCCK0_TXFILTER2, MASKDWORD) & MASKCCK;
	for (i = 0; i < CCK_TABLE_LENGTH; i++) {
		if (rtldm.cck_inch14) {
			if (memcmp(&temp_cck, &cck_tbl_ch14[i][2], 4) == 0) {
				cck_index_old = (u8)i;
				rtldm.swing_idx_cck_base = (u8)i;
				RT_TRACE(priv, COMP_POWER_TRACKING,
					 DBG_LOUD,
					 "Initial reg0x%x = 0x%lx, cck_index = 0x%x, ch 14 %d\n",
					 RCCK0_TXFILTER2, temp_cck,
					 cck_index_old,
					 rtldm.cck_inch14);
				break;
			}
		} else {
			if (memcmp(&temp_cck, &cck_tbl_ch1_13[i][2], 4) == 0) {
				cck_index_old = (u8)i;
				rtldm.swing_idx_cck_base = (u8)i;
				RT_TRACE(priv, COMP_POWER_TRACKING,
					 DBG_LOUD,
					 "Initial reg0x%x = 0x%lx, cck_index = 0x%x, ch14 %d\n",
					 RCCK0_TXFILTER2, temp_cck,
					 cck_index_old,
					 rtldm.cck_inch14);
				break;
			}
		}
	}

	/*3 Initialize ThermalValues of RFCalibrateInfo*/
	if (!rtldm.thermalvalue) {
		rtldm.thermalvalue = rtlefuse.eeprom_thermalmeter;
		rtldm.thermalvalue_lck = thermalvalue;
		rtldm.thermalvalue_iqk = thermalvalue;
		for (i = 0; i < rf; i++)
			rtldm.ofdm_index[i] = ofdm_index_old[i];
		rtldm.cck_index = cck_index_old;
	}

	/*4 Calculate average thermal meter*/
	rtldm.thermalvalue_avg[rtldm.thermalvalue_avg_index] = thermalvalue;
	rtldm.thermalvalue_avg_index++;
	if (rtldm.thermalvalue_avg_index == AVG_THERMAL_NUM_88E)
		rtldm.thermalvalue_avg_index = 0;

	for (i = 0; i < AVG_THERMAL_NUM_88E; i++) {
		if (rtldm.thermalvalue_avg[i]) {
			thermalvalue_avg += rtldm.thermalvalue_avg[i];
			thermalvalue_avg_count++;
		}
	}

	if (thermalvalue_avg_count)
		thermalvalue = (u8)(thermalvalue_avg / thermalvalue_avg_count);

	/* 5 Calculate delta, delta_LCK, delta_IQK.*/
#if 0
	if (rtlhal->reloadtxpowerindex) {
		delta = (thermalvalue > rtlefuse.eeprom_thermalmeter) ?
		    (thermalvalue - rtlefuse.eeprom_thermalmeter) :
		    (rtlefuse.eeprom_thermalmeter - thermalvalue);
		rtlhal->reloadtxpowerindex = false;
		rtldm.done_txpower = false;

#else
	if (0) {
#endif
	} else if (rtldm.done_txpower) {
		delta = (thermalvalue > rtldm.thermalvalue) ?
		    (thermalvalue - rtldm.thermalvalue) :
		    (rtldm.thermalvalue - thermalvalue);
	} else {
		delta = (thermalvalue > rtlefuse.eeprom_thermalmeter) ?
		    (thermalvalue - rtlefuse.eeprom_thermalmeter) :
		    (rtlefuse.eeprom_thermalmeter - thermalvalue);
	}
	delta_lck = (thermalvalue > rtldm.thermalvalue_lck) ?
	    (thermalvalue - rtldm.thermalvalue_lck) :
	    (rtldm.thermalvalue_lck - thermalvalue);
	delta_iqk = (thermalvalue > rtldm.thermalvalue_iqk) ?
	    (thermalvalue - rtldm.thermalvalue_iqk) :
	    (rtldm.thermalvalue_iqk - thermalvalue);

	RT_TRACE(priv, COMP_POWER_TRACKING, DBG_LOUD,
		 "Readback Thermal Meter = 0x%x pre thermal meter 0x%x eeprom_thermalmeter 0x%x delta 0x%x delta_lck 0x%x delta_iqk 0x%x\n",
		 thermalvalue, rtldm.thermalvalue,
		 rtlefuse.eeprom_thermalmeter, delta, delta_lck,
		 delta_iqk);
	/* 6 If necessary, do LCK.*/
	if (delta_lck >= 8) {
		rtldm.thermalvalue_lck = thermalvalue;
#if 0
		rtlpriv->cfg->ops->phy_lc_calibrate(priv, false);
#else
		rtl88e_phy_lc_calibrate(priv, false);
#endif
	}

	/* 7 If necessary, move the index of
	 * swing table to adjust Tx power.
	 */
	if (delta > 0 && rtldm.txpower_track_control) {
		delta = (thermalvalue > rtlefuse.eeprom_thermalmeter) ?
		    (thermalvalue - rtlefuse.eeprom_thermalmeter) :
		    (rtlefuse.eeprom_thermalmeter - thermalvalue);

		/* 7.1 Get the final CCK_index and OFDM_index for each
		 * swing table.
		 */
		if (thermalvalue > rtlefuse.eeprom_thermalmeter) {
			CAL_SWING_OFF(offset, power_inc, INDEX_MAPPING_NUM,
				      delta);
			for (i = 0; i < rf; i++)
				ofdm_index[i] =
				  rtldm.ofdm_index[i] +
				  delta_swing_table_idx[power_inc][offset];
			cck_index = rtldm.cck_index +
				delta_swing_table_idx[power_inc][offset];
		} else {
			CAL_SWING_OFF(offset, power_dec, INDEX_MAPPING_NUM,
				      delta);
			for (i = 0; i < rf; i++)
				ofdm_index[i] =
				  rtldm.ofdm_index[i] +
				  delta_swing_table_idx[power_dec][offset];
			cck_index = rtldm.cck_index +
				delta_swing_table_idx[power_dec][offset];
		}

		/* 7.2 Handle boundary conditions of index.*/
		for (i = 0; i < rf; i++) {
			if (ofdm_index[i] > OFDM_TABLE_SIZE-1)
				ofdm_index[i] = OFDM_TABLE_SIZE-1;
			else if (rtldm.ofdm_index[i] < ofdm_min_index)
				ofdm_index[i] = ofdm_min_index;
		}

		if (cck_index > CCK_TABLE_SIZE-1)
			cck_index = CCK_TABLE_SIZE-1;
		else if (cck_index < 0)
			cck_index = 0;

		/*7.3Configure the Swing Table to adjust Tx Power.*/
		if (rtldm.txpower_track_control) {
			rtldm.done_txpower = true;
			rtldm.swing_idx_ofdm[RF90_PATH_A] =
				(u8)ofdm_index[RF90_PATH_A];
			rtldm.swing_idx_cck = cck_index;
			if (rtldm.swing_idx_ofdm_cur !=
			    rtldm.swing_idx_ofdm[0]) {
				rtldm.swing_idx_ofdm_cur =
					 rtldm.swing_idx_ofdm[0];
				rtldm.swing_flag_ofdm = true;
			}

			if (rtldm.swing_idx_cck_cur != rtldm.swing_idx_cck) {
				rtldm.swing_idx_cck_cur = rtldm.swing_idx_cck;
				rtldm.swing_flag_cck = true;
			}

			dm_tx_pwr_track_set_pwr(priv, TXAGC, 0, 0);
		}
	}

	if (delta_iqk >= 8) {
		rtldm.thermalvalue_iqk = thermalvalue;
		rtl88e_phy_iq_calibrate(priv);
	}

	if (rtldm.txpower_track_control)
		rtldm.thermalvalue = thermalvalue;
	rtldm.txpowercount = 0;
	RT_TRACE(priv, COMP_POWER_TRACKING, DBG_LOUD, "end\n");
}

static void rtl88e_dm_init_txpower_tracking(struct rtl8xxxu_priv *priv)
{

	rtldm.txpower_tracking = true;
	rtldm.txpower_trackinginit = false;
	rtldm.txpowercount = 0;
	rtldm.txpower_track_control = true;

	rtldm.swing_idx_ofdm[RF90_PATH_A] = 12;
	rtldm.swing_idx_ofdm_cur = 12;
	rtldm.swing_flag_ofdm = false;
	RT_TRACE(priv, COMP_POWER_TRACKING, DBG_LOUD,
		 "rtldm.txpower_tracking = %d\n",
		 rtldm.txpower_tracking);
}

void rtl88e_dm_check_txpower_tracking(struct rtl8xxxu_priv *priv)
{
	static u8 tm_trigger;

	if (!rtldm.txpower_tracking)
		return;

	if (!tm_trigger) {
		rtl_set_rfreg(priv, RF90_PATH_A, RF_T_METER, BIT(17)|BIT(16),
			      0x03);
		RT_TRACE(priv, COMP_POWER_TRACKING, DBG_LOUD,
			 "Trigger 88E Thermal Meter!!\n");
		tm_trigger = 1;
		return;
	} else {
		RT_TRACE(priv, COMP_POWER_TRACKING, DBG_LOUD,
			 "Schedule TxPowerTracking !!\n");
				dm_txpower_track_cb_therm(priv);
		tm_trigger = 0;
	}
}

void rtl88e_dm_init_rate_adaptive_mask(struct rtl8xxxu_priv *priv)
{
	p_ra.ratr_state = DM_RATR_STA_INIT;
	p_ra.pre_ratr_state = DM_RATR_STA_INIT;

	if (rtldm.dm_type == DM_TYPE_BYDRIVER)
		rtldm.useramask = true;
	else
		rtldm.useramask = false;
}

#if 0
static void rtl88e_dm_refresh_rate_adaptive_mask(struct rtl8xxxu_priv *priv)
{
	struct rtl_hal *rtlhal = rtl_hal(rtl_priv(hw));
	struct rtl_mac *mac = rtl_mac(rtl_priv(hw));
	struct rate_adaptive *p_ra = &rtlpriv->ra;
	u32 low_rssithresh_for_ra, high_rssithresh_for_ra;
	struct ieee80211_sta *sta = NULL;

	if (is_hal_stop(rtlhal)) {
		RT_TRACE(priv, COMP_RATE, DBG_LOUD,
			 "driver is going to unload\n");
		return;
	}

	if (!rtldm.useramask) {
		RT_TRACE(priv, COMP_RATE, DBG_LOUD,
			 "driver does not control rate adaptive mask\n");
		return;
	}

	if (mac->link_state == MAC80211_LINKED &&
	    mac->opmode == NL80211_IFTYPE_STATION) {
		switch (p_ra.pre_ratr_state) {
		case DM_RATR_STA_HIGH:
			high_rssithresh_for_ra = 50;
			low_rssithresh_for_ra = 20;
			break;
		case DM_RATR_STA_MIDDLE:
			high_rssithresh_for_ra = 55;
			low_rssithresh_for_ra = 20;
			break;
		case DM_RATR_STA_LOW:
			high_rssithresh_for_ra = 50;
			low_rssithresh_for_ra = 25;
			break;
		default:
			high_rssithresh_for_ra = 50;
			low_rssithresh_for_ra = 20;
			break;
		}

		if (rtldm.undec_sm_pwdb >
		    (long)high_rssithresh_for_ra)
			p_ra.ratr_state = DM_RATR_STA_HIGH;
		else if (rtldm.undec_sm_pwdb >
			 (long)low_rssithresh_for_ra)
			p_ra.ratr_state = DM_RATR_STA_MIDDLE;
		else
			p_ra.ratr_state = DM_RATR_STA_LOW;

		if (p_ra.pre_ratr_state != p_ra.ratr_state) {
			RT_TRACE(priv, COMP_RATE, DBG_LOUD,
				 "RSSI = %ld\n",
				  rtldm.undec_sm_pwdb);
			RT_TRACE(priv, COMP_RATE, DBG_LOUD,
				 "RSSI_LEVEL = %d\n", p_ra.ratr_state);
			RT_TRACE(priv, COMP_RATE, DBG_LOUD,
				 "PreState = %d, CurState = %d\n",
				  p_ra.pre_ratr_state, p_ra.ratr_state);

			rcu_read_lock();
			sta = rtl_find_sta(hw, mac->bssid);
			if (sta)
				rtlpriv->cfg->ops->update_rate_tbl(hw, sta,
								   p_ra.ratr_state);
			rcu_read_unlock();

			p_ra.pre_ratr_state = p_ra.ratr_state;
		}
	}
}
#endif

static void rtl92c_dm_init_dynamic_bb_powersaving(struct rtl8xxxu_priv *priv)
{
	dm_pstable.pre_ccastate = CCA_MAX;
	dm_pstable.cur_ccasate = CCA_MAX;
	dm_pstable.pre_rfstate = RF_MAX;
	dm_pstable.cur_rfstate = RF_MAX;
	dm_pstable.rssi_val_min = 0;
}

static void rtl88e_dm_update_rx_idle_ant(struct rtl8xxxu_priv *priv,
					 u8 ant)
{
	struct fast_ant_training *pfat_table = &rtldm.fat_table;
	u32 default_ant, optional_ant;

	if (pfat_table->rx_idle_ant != ant) {
		RT_TRACE(priv, COMP_INIT, DBG_LOUD,
			 "need to update rx idle ant\n");
		if (ant == MAIN_ANT) {
			default_ant =
			  (pfat_table->rx_idle_ant == CG_TRX_HW_ANTDIV) ?
			  MAIN_ANT_CG_TRX : MAIN_ANT_CGCS_RX;
			optional_ant =
			  (pfat_table->rx_idle_ant == CG_TRX_HW_ANTDIV) ?
			  AUX_ANT_CG_TRX : AUX_ANT_CGCS_RX;
		} else {
			default_ant =
			   (pfat_table->rx_idle_ant == CG_TRX_HW_ANTDIV) ?
			   AUX_ANT_CG_TRX : AUX_ANT_CGCS_RX;
			optional_ant =
			   (pfat_table->rx_idle_ant == CG_TRX_HW_ANTDIV) ?
			   MAIN_ANT_CG_TRX : MAIN_ANT_CGCS_RX;
		}

		if (rtlefuse.antenna_div_type == CG_TRX_HW_ANTDIV) {
			rtl_set_bbreg(priv, DM_REG_RX_ANT_CTRL_11N,
				      BIT(5) | BIT(4) | BIT(3), default_ant);
			rtl_set_bbreg(priv, DM_REG_RX_ANT_CTRL_11N,
				      BIT(8) | BIT(7) | BIT(6), optional_ant);
			rtl_set_bbreg(priv, DM_REG_ANTSEL_CTRL_11N,
				      BIT(14) | BIT(13) | BIT(12),
				      default_ant);
			rtl_set_bbreg(priv, DM_REG_RESP_TX_11N,
				      BIT(6) | BIT(7), default_ant);
		} else if (rtlefuse.antenna_div_type == CGCS_RX_HW_ANTDIV) {
			rtl_set_bbreg(priv, DM_REG_RX_ANT_CTRL_11N,
				      BIT(5) | BIT(4) | BIT(3), default_ant);
			rtl_set_bbreg(priv, DM_REG_RX_ANT_CTRL_11N,
				      BIT(8) | BIT(7) | BIT(6), optional_ant);
		}
	}
	pfat_table->rx_idle_ant = ant;
	RT_TRACE(priv, COMP_INIT, DBG_LOUD, "RxIdleAnt %s\n",
		 (ant == MAIN_ANT) ? ("MAIN_ANT") : ("AUX_ANT"));
}

static void rtl88e_dm_update_tx_ant(struct rtl8xxxu_priv *priv,
				    u8 ant, u32 mac_id)
{
	struct fast_ant_training *pfat_table = &rtldm.fat_table;
	u8 target_ant;

	if (ant == MAIN_ANT)
		target_ant = MAIN_ANT_CG_TRX;
	else
		target_ant = AUX_ANT_CG_TRX;

	pfat_table->antsel_a[mac_id] = target_ant & BIT(0);
	pfat_table->antsel_b[mac_id] = (target_ant & BIT(1)) >> 1;
	pfat_table->antsel_c[mac_id] = (target_ant & BIT(2)) >> 2;
	RT_TRACE(priv, COMP_INIT, DBG_LOUD, "txfrominfo target ant %s\n",
		(ant == MAIN_ANT) ? ("MAIN_ANT") : ("AUX_ANT"));
	RT_TRACE(priv, COMP_INIT, DBG_LOUD, "antsel_tr_mux = 3'b%d%d%d\n",
		pfat_table->antsel_c[mac_id],
		pfat_table->antsel_b[mac_id],
		pfat_table->antsel_a[mac_id]);
}

static void rtl88e_dm_rx_hw_antena_div_init(struct rtl8xxxu_priv *priv)
{
	u32  value32;

	/*MAC Setting*/
	value32 = rtl_get_bbreg(priv, DM_REG_ANTSEL_PIN_11N, MASKDWORD);
	rtl_set_bbreg(priv, DM_REG_ANTSEL_PIN_11N,
		      MASKDWORD, value32 | (BIT(23) | BIT(25)));
	/*Pin Setting*/
	rtl_set_bbreg(priv, DM_REG_PIN_CTRL_11N, BIT(9) | BIT(8), 0);
	rtl_set_bbreg(priv, DM_REG_RX_ANT_CTRL_11N, BIT(10), 0);
	rtl_set_bbreg(priv, DM_REG_LNA_SWITCH_11N, BIT(22), 1);
	rtl_set_bbreg(priv, DM_REG_LNA_SWITCH_11N, BIT(31), 1);
	/*OFDM Setting*/
	rtl_set_bbreg(priv, DM_REG_ANTDIV_PARA1_11N, MASKDWORD, 0x000000a0);
	/*CCK Setting*/
	rtl_set_bbreg(priv, DM_REG_BB_PWR_SAV4_11N, BIT(7), 1);
	rtl_set_bbreg(priv, DM_REG_CCK_ANTDIV_PARA2_11N, BIT(4), 1);
	rtl88e_dm_update_rx_idle_ant(priv, MAIN_ANT);
	rtl_set_bbreg(priv, DM_REG_ANT_MAPPING1_11N, MASKLWORD, 0x0201);
}

static void rtl88e_dm_trx_hw_antenna_div_init(struct rtl8xxxu_priv *priv)
{
	u32  value32;

	/*MAC Setting*/
	value32 = rtl_get_bbreg(priv, DM_REG_ANTSEL_PIN_11N, MASKDWORD);
	rtl_set_bbreg(priv, DM_REG_ANTSEL_PIN_11N, MASKDWORD,
		      value32 | (BIT(23) | BIT(25)));
	/*Pin Setting*/
	rtl_set_bbreg(priv, DM_REG_PIN_CTRL_11N, BIT(9) | BIT(8), 0);
	rtl_set_bbreg(priv, DM_REG_RX_ANT_CTRL_11N, BIT(10), 0);
	rtl_set_bbreg(priv, DM_REG_LNA_SWITCH_11N, BIT(22), 0);
	rtl_set_bbreg(priv, DM_REG_LNA_SWITCH_11N, BIT(31), 1);
	/*OFDM Setting*/
	rtl_set_bbreg(priv, DM_REG_ANTDIV_PARA1_11N, MASKDWORD, 0x000000a0);
	/*CCK Setting*/
	rtl_set_bbreg(priv, DM_REG_BB_PWR_SAV4_11N, BIT(7), 1);
	rtl_set_bbreg(priv, DM_REG_CCK_ANTDIV_PARA2_11N, BIT(4), 1);
	/*TX Setting*/
	rtl_set_bbreg(priv, DM_REG_TX_ANT_CTRL_11N, BIT(21), 0);
	rtl88e_dm_update_rx_idle_ant(priv, MAIN_ANT);
	rtl_set_bbreg(priv, DM_REG_ANT_MAPPING1_11N, MASKLWORD, 0x0201);
}

static void rtl88e_dm_fast_training_init(struct rtl8xxxu_priv *priv)
{
	struct fast_ant_training *pfat_table = &rtldm.fat_table;
	u32 ant_combination = 2;
	u32 value32, i;

	for (i = 0; i < 6; i++) {
		pfat_table->bssid[i] = 0;
		pfat_table->ant_sum[i] = 0;
		pfat_table->ant_cnt[i] = 0;
		pfat_table->ant_ave[i] = 0;
	}
	pfat_table->train_idx = 0;
	pfat_table->fat_state = FAT_NORMAL_STATE;

	/*MAC Setting*/
	value32 = rtl_get_bbreg(priv, DM_REG_ANTSEL_PIN_11N, MASKDWORD);
	rtl_set_bbreg(priv, DM_REG_ANTSEL_PIN_11N,
		      MASKDWORD, value32 | (BIT(23) | BIT(25)));
	value32 = rtl_get_bbreg(priv, DM_REG_ANT_TRAIN_PARA2_11N, MASKDWORD);
	rtl_set_bbreg(priv, DM_REG_ANT_TRAIN_PARA2_11N,
		      MASKDWORD, value32 | (BIT(16) | BIT(17)));
	rtl_set_bbreg(priv, DM_REG_ANT_TRAIN_PARA2_11N,
		      MASKLWORD, 0);
	rtl_set_bbreg(priv, DM_REG_ANT_TRAIN_PARA1_11N,
		      MASKDWORD, 0);

	/*Pin Setting*/
	rtl_set_bbreg(priv, DM_REG_PIN_CTRL_11N, BIT(9) | BIT(8), 0);
	rtl_set_bbreg(priv, DM_REG_RX_ANT_CTRL_11N, BIT(10), 0);
	rtl_set_bbreg(priv, DM_REG_LNA_SWITCH_11N, BIT(22), 0);
	rtl_set_bbreg(priv, DM_REG_LNA_SWITCH_11N, BIT(31), 1);

	/*OFDM Setting*/
	rtl_set_bbreg(priv, DM_REG_ANTDIV_PARA1_11N, MASKDWORD, 0x000000a0);
	/*antenna mapping table*/
	rtl_set_bbreg(priv, DM_REG_ANT_MAPPING1_11N, MASKBYTE0, 1);
	rtl_set_bbreg(priv, DM_REG_ANT_MAPPING1_11N, MASKBYTE1, 2);

	/*TX Setting*/
	rtl_set_bbreg(priv, DM_REG_TX_ANT_CTRL_11N, BIT(21), 1);
	rtl_set_bbreg(priv, DM_REG_RX_ANT_CTRL_11N,
		      BIT(5) | BIT(4) | BIT(3), 0);
	rtl_set_bbreg(priv, DM_REG_RX_ANT_CTRL_11N,
		      BIT(8) | BIT(7) | BIT(6), 1);
	rtl_set_bbreg(priv, DM_REG_RX_ANT_CTRL_11N,
		      BIT(2) | BIT(1) | BIT(0), (ant_combination - 1));

	rtl_set_bbreg(priv, DM_REG_IGI_A_11N, BIT(7), 1);
}

static void rtl88e_dm_antenna_div_init(struct rtl8xxxu_priv *priv)
{
	if (rtlefuse.antenna_div_type == CGCS_RX_HW_ANTDIV)
		rtl88e_dm_rx_hw_antena_div_init(priv);
	else if (rtlefuse.antenna_div_type == CG_TRX_HW_ANTDIV)
		rtl88e_dm_trx_hw_antenna_div_init(priv);
	else if (rtlefuse.antenna_div_type == CG_TRX_SMART_ANTDIV)
		rtl88e_dm_fast_training_init(priv);

}

void rtl88e_dm_set_tx_ant_by_tx_info(struct rtl8xxxu_priv *priv,
				     u8 *pdesc, u32 mac_id)
{
	struct fast_ant_training *pfat_table = &rtldm.fat_table;

	if ((rtlefuse.antenna_div_type == CG_TRX_HW_ANTDIV) ||
	    (rtlefuse.antenna_div_type == CG_TRX_SMART_ANTDIV)) {
		SET_TX_DESC_ANTSEL_A(pdesc, pfat_table->antsel_a[mac_id]);
		SET_TX_DESC_ANTSEL_B(pdesc, pfat_table->antsel_b[mac_id]);
		SET_TX_DESC_ANTSEL_C(pdesc, pfat_table->antsel_c[mac_id]);
	}
}

void rtl88e_dm_ant_sel_statistics(struct rtl8xxxu_priv *priv,
				  u8 antsel_tr_mux, u32 mac_id,
				  u32 rx_pwdb_all)
{
	struct fast_ant_training *pfat_table = &rtldm.fat_table;

	if (rtlefuse.antenna_div_type == CG_TRX_HW_ANTDIV) {
		if (antsel_tr_mux == MAIN_ANT_CG_TRX) {
			pfat_table->main_ant_sum[mac_id] += rx_pwdb_all;
			pfat_table->main_ant_cnt[mac_id]++;
		} else {
			pfat_table->aux_ant_sum[mac_id] += rx_pwdb_all;
			pfat_table->aux_ant_cnt[mac_id]++;
		}
	} else if (rtlefuse.antenna_div_type == CGCS_RX_HW_ANTDIV) {
		if (antsel_tr_mux == MAIN_ANT_CGCS_RX) {
			pfat_table->main_ant_sum[mac_id] += rx_pwdb_all;
			pfat_table->main_ant_cnt[mac_id]++;
		} else {
			pfat_table->aux_ant_sum[mac_id] += rx_pwdb_all;
			pfat_table->aux_ant_cnt[mac_id]++;
		}
	}
}

static void rtl88e_dm_hw_ant_div(struct rtl8xxxu_priv *priv)
{
	struct rtl_sta_info *drv_priv;
	struct fast_ant_training *pfat_table = &rtldm.fat_table;
	u32 i, min_rssi = 0xff, ant_div_max_rssi = 0;
	u32 max_rssi = 0, local_min_rssi, local_max_rssi;
	u32 main_rssi, aux_rssi;
	u8 rx_idle_ant = 0, target_ant = 7;

	/*for sta its self*/
	i = 0;
	main_rssi = (pfat_table->main_ant_cnt[i] != 0) ?
		(pfat_table->main_ant_sum[i] / pfat_table->main_ant_cnt[i]) : 0;
	aux_rssi = (pfat_table->aux_ant_cnt[i] != 0) ?
		(pfat_table->aux_ant_sum[i] / pfat_table->aux_ant_cnt[i]) : 0;
	target_ant = (main_rssi == aux_rssi) ?
		pfat_table->rx_idle_ant : ((main_rssi >= aux_rssi) ?
		MAIN_ANT : AUX_ANT);
	RT_TRACE(priv, COMP_INIT, DBG_LOUD,
		"main_ant_sum %d main_ant_cnt %d\n",
		pfat_table->main_ant_sum[i],
		pfat_table->main_ant_cnt[i]);
	RT_TRACE(priv, COMP_INIT, DBG_LOUD,
		 "aux_ant_sum %d aux_ant_cnt %d\n",
		 pfat_table->aux_ant_sum[i], pfat_table->aux_ant_cnt[i]);
	RT_TRACE(priv, COMP_INIT, DBG_LOUD, "main_rssi %d aux_rssi%d\n",
		 main_rssi, aux_rssi);
	local_max_rssi = (main_rssi > aux_rssi) ? main_rssi : aux_rssi;
	if ((local_max_rssi > ant_div_max_rssi) && (local_max_rssi < 40))
		ant_div_max_rssi = local_max_rssi;
	if (local_max_rssi > max_rssi)
		max_rssi = local_max_rssi;

	if ((pfat_table->rx_idle_ant == MAIN_ANT) && (main_rssi == 0))
		main_rssi = aux_rssi;
	else if ((pfat_table->rx_idle_ant == AUX_ANT) && (aux_rssi == 0))
		aux_rssi = main_rssi;

	local_min_rssi = (main_rssi > aux_rssi) ? aux_rssi : main_rssi;
	if (local_min_rssi < min_rssi) {
		min_rssi = local_min_rssi;
		rx_idle_ant = target_ant;
	}
	if (rtlefuse.antenna_div_type == CG_TRX_HW_ANTDIV)
		rtl88e_dm_update_tx_ant(priv, target_ant, i);
#if 0
	/* STA이외의 모드는 아직 지원 X */
	if (rtlpriv->mac80211.opmode == NL80211_IFTYPE_AP ||
	    rtlpriv->mac80211.opmode == NL80211_IFTYPE_ADHOC) {
		spin_lock_bh(&rtlpriv->locks.entry_list_lock);
		list_for_each_entry(drv_priv, &rtlpriv->entry_list, list) {
			i++;
			main_rssi = (pfat_table->main_ant_cnt[i] != 0) ?
				(pfat_table->main_ant_sum[i] /
				pfat_table->main_ant_cnt[i]) : 0;
			aux_rssi = (pfat_table->aux_ant_cnt[i] != 0) ?
				(pfat_table->aux_ant_sum[i] /
				pfat_table->aux_ant_cnt[i]) : 0;
			target_ant = (main_rssi == aux_rssi) ?
				pfat_table->rx_idle_ant : ((main_rssi >=
				aux_rssi) ? MAIN_ANT : AUX_ANT);

			local_max_rssi = (main_rssi > aux_rssi) ?
					 main_rssi : aux_rssi;
			if ((local_max_rssi > ant_div_max_rssi) &&
			    (local_max_rssi < 40))
				ant_div_max_rssi = local_max_rssi;
			if (local_max_rssi > max_rssi)
				max_rssi = local_max_rssi;

			if ((pfat_table->rx_idle_ant == MAIN_ANT) &&
			    (main_rssi == 0))
				main_rssi = aux_rssi;
			else if ((pfat_table->rx_idle_ant == AUX_ANT) &&
				 (aux_rssi == 0))
				aux_rssi = main_rssi;

			local_min_rssi = (main_rssi > aux_rssi) ?
				aux_rssi : main_rssi;
			if (local_min_rssi < min_rssi) {
				min_rssi = local_min_rssi;
				rx_idle_ant = target_ant;
			}
			if (rtlefuse.antenna_div_type == CG_TRX_HW_ANTDIV)
				rtl88e_dm_update_tx_ant(hw, target_ant, i);
		}
		spin_unlock_bh(&rtlpriv->locks.entry_list_lock);
	}
#endif
	for (i = 0; i < ASSOCIATE_ENTRY_NUM; i++) {
		pfat_table->main_ant_sum[i] = 0;
		pfat_table->aux_ant_sum[i] = 0;
		pfat_table->main_ant_cnt[i] = 0;
		pfat_table->aux_ant_cnt[i] = 0;
	}

	rtl88e_dm_update_rx_idle_ant(priv, rx_idle_ant);

	dm_dig.antdiv_rssi_max = ant_div_max_rssi;
	dm_dig.rssi_max = max_rssi;
}

#if 0
static void rtl88e_set_next_mac_address_target(struct rtl8xxxu_priv *priv)
{
	struct rtl_sta_info *drv_priv;
	struct fast_ant_training *pfat_table = &rtldm.fat_table;
	u32 value32, i, j = 0;

	if (mac->link_state >= MAC80211_LINKED) {
		for (i = 0; i < ASSOCIATE_ENTRY_NUM; i++) {
			if ((pfat_table->train_idx + 1) == ASSOCIATE_ENTRY_NUM)
				pfat_table->train_idx = 0;
			else
				pfat_table->train_idx++;

			if (pfat_table->train_idx == 0) {
				value32 = (mac->mac_addr[5] << 8) |
					  mac->mac_addr[4];
				rtl_set_bbreg(priv, DM_REG_ANT_TRAIN_PARA2_11N,
					      MASKLWORD, value32);

				value32 = (mac->mac_addr[3] << 24) |
					  (mac->mac_addr[2] << 16) |
					  (mac->mac_addr[1] << 8) |
					  mac->mac_addr[0];
				rtl_set_bbreg(priv, DM_REG_ANT_TRAIN_PARA1_11N,
					      MASKDWORD, value32);
				break;
			}

			if (rtlpriv->mac80211.opmode !=
			    NL80211_IFTYPE_STATION) {
				spin_lock_bh(&rtlpriv->locks.entry_list_lock);
				list_for_each_entry(drv_priv,
						    &rtlpriv->entry_list, list) {
					j++;
					if (j != pfat_table->train_idx)
						continue;

					value32 = (drv_priv->mac_addr[5] << 8) |
						  drv_priv->mac_addr[4];
					rtl_set_bbreg(priv,
						      DM_REG_ANT_TRAIN_PARA2_11N,
						      MASKLWORD, value32);

					value32 = (drv_priv->mac_addr[3] << 24) |
						  (drv_priv->mac_addr[2] << 16) |
						  (drv_priv->mac_addr[1] << 8) |
						  drv_priv->mac_addr[0];
					rtl_set_bbreg(priv,
						      DM_REG_ANT_TRAIN_PARA1_11N,
						      MASKDWORD, value32);
					break;
				}
				spin_unlock_bh(&rtlpriv->locks.entry_list_lock);
				/*find entry, break*/
				if (j == pfat_table->train_idx)
					break;
			}
		}
	}
}
#endif

#if 0
static void rtl88e_dm_fast_ant_training(struct rtl8xxxu_priv *priv)
{
	struct rtl_dm *rtldm = rtl_dm(rtl_priv(hw));
	struct fast_ant_training *pfat_table = &rtldm.fat_table;
	u32 i, max_rssi = 0;
	u8 target_ant = 2;
	bool bpkt_filter_match = false;

	if (pfat_table->fat_state == FAT_TRAINING_STATE) {
		for (i = 0; i < 7; i++) {
			if (pfat_table->ant_cnt[i] == 0) {
				pfat_table->ant_ave[i] = 0;
			} else {
				pfat_table->ant_ave[i] =
					pfat_table->ant_sum[i] /
					pfat_table->ant_cnt[i];
				bpkt_filter_match = true;
			}

			if (pfat_table->ant_ave[i] > max_rssi) {
				max_rssi = pfat_table->ant_ave[i];
				target_ant = (u8) i;
			}
		}

		if (bpkt_filter_match == false) {
			rtl_set_bbreg(priv, DM_REG_TXAGC_A_1_MCS32_11N,
				      BIT(16), 0);
			rtl_set_bbreg(priv, DM_REG_IGI_A_11N, BIT(7), 0);
		} else {
			rtl_set_bbreg(priv, DM_REG_TXAGC_A_1_MCS32_11N,
				      BIT(16), 0);
			rtl_set_bbreg(priv, DM_REG_RX_ANT_CTRL_11N, BIT(8) |
				      BIT(7) | BIT(6), target_ant);
			rtl_set_bbreg(priv, DM_REG_TX_ANT_CTRL_11N,
				      BIT(21), 1);

			pfat_table->antsel_a[pfat_table->train_idx] =
				target_ant & BIT(0);
			pfat_table->antsel_b[pfat_table->train_idx] =
				(target_ant & BIT(1)) >> 1;
			pfat_table->antsel_c[pfat_table->train_idx] =
				(target_ant & BIT(2)) >> 2;

			if (target_ant == 0)
				rtl_set_bbreg(priv, DM_REG_IGI_A_11N, BIT(7), 0);
		}

		for (i = 0; i < 7; i++) {
			pfat_table->ant_sum[i] = 0;
			pfat_table->ant_cnt[i] = 0;
		}

		pfat_table->fat_state = FAT_NORMAL_STATE;
		return;
	}

	if (pfat_table->fat_state == FAT_NORMAL_STATE) {
		rtl88e_set_next_mac_address_target(hw);

		pfat_table->fat_state = FAT_TRAINING_STATE;
		rtl_set_bbreg(priv, DM_REG_TXAGC_A_1_MCS32_11N, BIT(16), 1);
		rtl_set_bbreg(priv, DM_REG_IGI_A_11N, BIT(7), 1);

		mod_timer(&rtlpriv->works.fast_antenna_training_timer,
			  jiffies + MSECS(RTL_WATCH_DOG_TIME));
	}
}
#endif

#if 0
void rtl88e_dm_fast_antenna_training_callback(unsigned long data)
{
	struct ieee80211_hw *hw = (struct ieee80211_hw *)data;

	rtl88e_dm_fast_ant_training(hw);
}
#endif

#if 0
static void rtl88e_dm_antenna_diversity(struct rtl8xxxu_priv *priv)
{
	struct rtl_mac *mac = rtl_mac(rtl_priv(hw));
	struct rtl_efuse *rtlefuse = rtl_efuse(rtl_priv(hw));
	struct rtl_dm *rtldm = rtl_dm(rtl_priv(hw));
	struct fast_ant_training *pfat_table = &rtldm.fat_table;

	if (mac->link_state < MAC80211_LINKED) {
		RT_TRACE(priv, COMP_DIG, DBG_LOUD, "No Link\n");
		if (pfat_table->becomelinked) {
			RT_TRACE(priv, COMP_DIG, DBG_LOUD,
				 "need to turn off HW AntDiv\n");
			rtl_set_bbreg(priv, DM_REG_IGI_A_11N, BIT(7), 0);
			rtl_set_bbreg(priv, DM_REG_CCK_ANTDIV_PARA1_11N,
				      BIT(15), 0);
			if (rtlefuse.antenna_div_type == CG_TRX_HW_ANTDIV)
				rtl_set_bbreg(priv, DM_REG_TX_ANT_CTRL_11N,
					      BIT(21), 0);
			pfat_table->becomelinked =
				(mac->link_state == MAC80211_LINKED) ?
				true : false;
		}
		return;
	} else {
		if (!pfat_table->becomelinked) {
			RT_TRACE(priv, COMP_DIG, DBG_LOUD,
				 "Need to turn on HW AntDiv\n");
			rtl_set_bbreg(priv, DM_REG_IGI_A_11N, BIT(7), 1);
			rtl_set_bbreg(priv, DM_REG_CCK_ANTDIV_PARA1_11N,
				      BIT(15), 1);
			if (rtlefuse.antenna_div_type == CG_TRX_HW_ANTDIV)
				rtl_set_bbreg(priv, DM_REG_TX_ANT_CTRL_11N,
					      BIT(21), 1);
			pfat_table->becomelinked =
				(mac->link_state >= MAC80211_LINKED) ?
				true : false;
		}
	}

	if ((rtlefuse.antenna_div_type == CG_TRX_HW_ANTDIV) ||
	    (rtlefuse.antenna_div_type == CGCS_RX_HW_ANTDIV))
		rtl88e_dm_hw_ant_div(hw);
	else if (rtlefuse.antenna_div_type == CG_TRX_SMART_ANTDIV)
		rtl88e_dm_fast_ant_training(hw);
}
#endif

void rtl_dm_diginit(struct rtl8xxxu_priv *priv, u32 cur_igvalue)
{
	dm_dig.dig_enable_flag = true;
	dm_dig.dig_ext_port_stage = DIG_EXT_PORT_STAGE_MAX;
	dm_dig.cur_igvalue = cur_igvalue;
	dm_dig.pre_igvalue = 0;
	dm_dig.cur_sta_cstate = DIG_STA_DISCONNECT;
	dm_dig.presta_cstate = DIG_STA_DISCONNECT;
	dm_dig.curmultista_cstate = DIG_MULTISTA_DISCONNECT;
	dm_dig.rssi_lowthresh = DM_DIG_THRESH_LOW;
	dm_dig.rssi_highthresh = DM_DIG_THRESH_HIGH;
	dm_dig.fa_lowthresh = DM_FALSEALARM_THRESH_LOW;
	dm_dig.fa_highthresh = DM_FALSEALARM_THRESH_HIGH;
	dm_dig.rx_gain_max = DM_DIG_MAX;
	dm_dig.rx_gain_min = DM_DIG_MIN;
	dm_dig.back_val = DM_DIG_BACKOFF_DEFAULT;
	dm_dig.back_range_max = DM_DIG_BACKOFF_MAX;
	dm_dig.back_range_min = DM_DIG_BACKOFF_MIN;
	dm_dig.pre_cck_cca_thres = 0xff;
	dm_dig.cur_cck_cca_thres = 0x83;
	dm_dig.forbidden_igi = DM_DIG_MIN;
	dm_dig.large_fa_hit = 0;
	dm_dig.recover_cnt = 0;
	dm_dig.dig_min_0 = 0x25;
	dm_dig.dig_min_1 = 0x25;
	dm_dig.media_connect_0 = false;
	dm_dig.media_connect_1 = false;
	rtldm.dm_initialgain_enable = true;
	dm_dig.bt30_cur_igi = 0x32;
	dm_dig.pre_cck_pd_state = CCK_PD_STAGE_MAX;
	dm_dig.cur_cck_pd_state = CCK_PD_STAGE_LOWRSSI;
}


void rtl88e_dm_init(struct rtl8xxxu_priv *priv)
{
	u32 cur_igvalue = rtl_get_bbreg(priv, ROFDM0_XAAGCCORE1, 0x7f);

	rtldm.dm_type = DM_TYPE_BYDRIVER;
	rtl_dm_diginit(priv, cur_igvalue);
	rtl88e_dm_init_dynamic_txpower(priv);
	rtl88e_dm_init_edca_turbo(priv);
	rtl88e_dm_init_rate_adaptive_mask(priv);
	rtl88e_dm_init_txpower_tracking(priv);
	rtl92c_dm_init_dynamic_bb_powersaving(priv);
	rtl88e_dm_antenna_div_init(priv);
}

#if 0
void rtl88e_dm_watchdog(struct rtl8xxxu_priv *priv)
{
	struct rtl_ps_ctl *ppsc = rtl_psc(rtl_priv(hw));
	bool fw_current_inpsmode = false;
	bool fw_ps_awake = true;

	rtlpriv->cfg->ops->get_hw_reg(hw, HW_VAR_FW_PSMODE_STATUS,
				      (u8 *)(&fw_current_inpsmode));
	rtlpriv->cfg->ops->get_hw_reg(hw, HW_VAR_FWLPS_RF_ON,
				      (u8 *)(&fw_ps_awake));
	if (ppsc->p2p_ps_info.p2p_ps_mode)
		fw_ps_awake = false;

	if ((ppsc->rfpwr_state == ERFON) &&
	    ((!fw_current_inpsmode) && fw_ps_awake) &&
	    (!ppsc->rfchange_inprogress)) {
		rtl88e_dm_pwdb_monitor(hw);
		rtl88e_dm_dig(hw);
		rtl88e_dm_false_alarm_counter_statistics(hw);
		rtl92c_dm_dynamic_txpower(hw);
		rtl88e_dm_check_txpower_tracking(hw);
		rtl88e_dm_refresh_rate_adaptive_mask(hw);
		rtl88e_dm_check_edca_turbo(hw);
		rtl88e_dm_antenna_diversity(hw);
	}
}
#endif

///////////////////////////END_OF_DM//////////////////////////////////////////////////

static void _rtl88eu_enable_fw_download(struct rtl8xxxu_priv *priv, bool enable)
{
	u8 tmp;

	if (enable) {
		tmp = rtl8xxxu_read8(priv, REG_MCUFWDL);
		rtl8xxxu_write8(priv, REG_MCUFWDL, tmp | 0x01);

		tmp = rtl8xxxu_read8(priv, REG_MCUFWDL + 2);
		rtl8xxxu_write8(priv, REG_MCUFWDL + 2, tmp & 0xf7);
	} else {
		tmp = rtl8xxxu_read8(priv, REG_MCUFWDL);
		rtl8xxxu_write8(priv, REG_MCUFWDL, tmp & 0xfe);

		rtl8xxxu_write8(priv, REG_MCUFWDL + 1, 0x00);
	}
}

static void _rtl88eu_fw_block_write(struct rtl8xxxu_priv *priv,
				   const u8 *buffer, u32 size)
{
	u32 blocksize = sizeof(u32);
	u8 *bufferptr = (u8 *)buffer;
	u32 *pu4BytePtr = (u32 *)buffer;
	u32 i, offset, blockcount, remainsize;

	blockcount = size / blocksize;
	remainsize = size % blocksize;

	for (i = 0; i < blockcount; i++) {
		offset = i * blocksize;
		rtl8xxxu_write32(priv, (FW_8192C_START_ADDRESS + offset),
				*(pu4BytePtr + i));
	}

	if (remainsize) {
		offset = blockcount * blocksize;
		bufferptr += offset;
		for (i = 0; i < remainsize; i++) {
			rtl8xxxu_write8(priv, (FW_8192C_START_ADDRESS +
						 offset + i), *(bufferptr + i));
		}
	}
}

static void _rtl88eu_fw_page_write(struct rtl8xxxu_priv *priv,
				  u32 page, const u8 *buffer, u32 size)
{
	u8 value8;
	u8 u8page = (u8) (page & 0x07);

	value8 = (rtl8xxxu_read8(priv, REG_MCUFWDL + 2) & 0xF8) | u8page;

	rtl8xxxu_write8(priv, (REG_MCUFWDL + 2), value8);
	_rtl88eu_fw_block_write(priv, buffer, size);
}

static void _rtl88eu_fill_dummy(u8 *pfwbuf, u32 *pfwlen)
{
	u32 fwlen = *pfwlen;
	u8 remain = (u8) (fwlen % 4);

	remain = (remain == 0) ? 0 : (4 - remain);

	while (remain > 0) {
		pfwbuf[fwlen] = 0;
		fwlen++;
		remain--;
	}

	*pfwlen = fwlen;
}

static void _rtl88eu_write_fw(struct rtl8xxxu_priv *priv,
			     enum version_8188e version, u8 *buffer, u32 size)
{
	u8 *bufferptr = (u8 *)buffer;
	u32 pagenums, remainsize;
	u32 page, offset;

	printk ("[TEST]FW size is %d bytes,\n", size);

	_rtl88eu_fill_dummy(bufferptr, &size);

	pagenums = size / FW_8192C_PAGE_SIZE;
	remainsize = size % FW_8192C_PAGE_SIZE;

	if (pagenums > 8) {
	}

	for (page = 0; page < pagenums; page++) {
		offset = page * FW_8192C_PAGE_SIZE;
		_rtl88eu_fw_page_write(priv, page, (bufferptr + offset),
				      FW_8192C_PAGE_SIZE);
	}

	if (remainsize) {
		offset = pagenums * FW_8192C_PAGE_SIZE;
		page = pagenums;
		_rtl88eu_fw_page_write(priv, page, (bufferptr + offset),
				      remainsize);
	}
}

void rtl88eu_firmware_selfreset(struct rtl8xxxu_priv *priv)
{
	u8 u1b_tmp;

	u1b_tmp = rtl8xxxu_read8(priv, REG_SYS_FUNC_EN+1);
	rtl8xxxu_write8(priv, REG_SYS_FUNC_EN+1, (u1b_tmp & (~BIT(2))));
	rtl8xxxu_write8(priv, REG_SYS_FUNC_EN+1, (u1b_tmp | BIT(2)));
	RT_TRACE(priv, COMP_INIT, DBG_LOUD, "8051 reset success\n");

}

static int _rtl88eu_fw_free_to_go(struct rtl8xxxu_priv *priv)
{
	int err = -EIO;
	u32 counter = 0;
	u32 value32;

	do {
		value32 = rtl8xxxu_read32(priv, REG_MCUFWDL);
		if (value32 & FWDL_CHKSUM_RPT)
			break;
	} while (counter++ < FW_8192C_POLLING_TIMEOUT_COUNT);

	if (counter >= FW_8192C_POLLING_TIMEOUT_COUNT) {
		RT_TRACE(priv, COMP_ERR, DBG_EMERG,
			 "chksum report faill ! REG_MCUFWDL:0x%08x .\n",
			  value32);
		goto exit;
	}

	RT_TRACE(priv, COMP_FW, DBG_TRACE,
		 "Checksum report OK ! REG_MCUFWDL:0x%08x .\n", value32);

	value32 = rtl8xxxu_read32(priv, REG_MCUFWDL);
	value32 |= MCUFWDL_RDY;
	value32 &= ~WINTINI_RDY;
	rtl8xxxu_write32(priv, REG_MCUFWDL, value32);

	rtl88eu_firmware_selfreset(priv);
	counter = 0;

	do {
		value32 = rtl8xxxu_read32(priv, REG_MCUFWDL);
		if (value32 & WINTINI_RDY) {
			RT_TRACE(priv, COMP_FW, DBG_TRACE,
				 "Polling FW ready success!! REG_MCUFWDL:0x%08x.\n",
				  value32);
			err = 0;
			goto exit;
		}

		udelay(FW_8192C_POLLING_DELAY);

	} while (counter++ < FW_8192C_POLLING_TIMEOUT_COUNT);

	RT_TRACE(priv, COMP_ERR, DBG_EMERG,
		 "Polling FW ready fail!! REG_MCUFWDL:0x%08x .\n", value32);

exit:
	return err;
}

int rtl88eu_download_fw(struct rtl8xxxu_priv *priv,
		       bool buse_wake_on_wlan_fw)
{
	u8 *pfwdata;
	u32 fwsize;
	int err;
	enum version_8188e version = 0x00;
	struct rtl92c_firmware_header *pfwheader;

	pfwheader = (struct rtl92c_firmware_header *)priv->fw_data->data;
	pfwdata = priv->fw_data->data;
	fwsize = 0x4000;
	RT_TRACE(priv, COMP_FW, DBG_DMESG,
		 "normal Firmware SIZE %d\n", fwsize);
	if (IS_FW_HEADER_EXIST(pfwheader)) {
		RT_TRACE(priv, COMP_FW, DBG_DMESG,
			 "Firmware Version(%d), Signature(%#x), Size(%d)\n",
			  pfwheader->version, pfwheader->signature,
			  (int)sizeof(struct rtl92c_firmware_header));

		pfwdata = pfwdata + sizeof(struct rtl92c_firmware_header);
		fwsize = fwsize - sizeof(struct rtl92c_firmware_header);
	}
	if (rtl8xxxu_read8(priv, REG_MCUFWDL) & BIT(7)) {
		rtl8xxxu_write8(priv, REG_MCUFWDL, 0);
		rtl88eu_firmware_selfreset(priv);
	}
	_rtl88eu_enable_fw_download(priv, true);
	_rtl88eu_write_fw(priv, version, pfwdata, fwsize);
	rtl8xxxu_write8(priv, REG_MCUFWDL,
		       rtl8xxxu_read8(priv, REG_MCUFWDL) | FWDL_CHKSUM_RPT);
	_rtl88eu_enable_fw_download(priv, false);

	err = _rtl88eu_fw_free_to_go(priv);
	if (err) {
		RT_TRACE(priv, COMP_ERR, DBG_EMERG,
			 "Firmware is not ready to run!\n");
	} else {
		RT_TRACE(priv, COMP_FW, DBG_LOUD,
			 "Firmware is ready to run!\n");
	}

	return 0;
}

#define RTL88eu_MAX_H2C_BOX_NUMS         4
#define RTL88eu_MAX_CMD_LEN              7
#define RTL88eu_MESSAGE_BOX_SIZE         4
#define RTL88eu_EX_MESSAGE_BOX_SIZE      4

static u8 _is_fw_read_cmd_down(struct rtl8xxxu_priv *priv, u8 msgbox_num)
{
	u8 read_down = false;
	int retry_cnts = 100;
	u8 valid;

	do {
		valid = rtl8xxxu_read8(priv, REG_HMETFR) & BIT(msgbox_num);
		if (0 == valid)
			read_down = true;
	} while((!read_down) && (retry_cnts--));

	return read_down;
}

void rtl88eu_fill_h2c_cmd(struct rtl8xxxu_priv *priv,
			  u8 element_id, u32 cmd_len, u8 *cmdbuffer)
{
	u8 bcmd_down = false;
	s32 retry_cnts = 100;
	u8 h2c_box_num;
	u32 msgbox_addr;
	u32 msgbox_ex_addr;
	u8 cmd_idx, ext_cmd_len;
	u32 h2c_cmd = 0;
	u32 h2c_cmd_ex = 0;
#if 0
	/* firmware는 되었다고 가정한다. */
	if (!rtlhal->fw_ready) {
		RT_ASSERT(false,
			  "return H2C cmd because of Fw download fail!!!\n");
		return;
	}
#endif

	if (!cmdbuffer) {
		return;
	}
	if (cmd_len > RTL88eu_MAX_CMD_LEN) {
		return;
	}

	do {
		h2c_box_num = last_hmeboxnum;

		if (!_is_fw_read_cmd_down(priv, h2c_box_num)) {
			RT_TRACE(priv, COMP_ERR, DBG_EMERG,
				 "return H2C cmd, fw read cmd failed...\n");
			return;
		}

		*(u8 *)(&h2c_cmd) = element_id;

		if (cmd_len <= 3) {
			memcpy((u8 *)(&h2c_cmd) +1, cmdbuffer, cmd_len);
		} else {
			memcpy((u8 *)(&h2c_cmd) +1, cmdbuffer, 3);
			ext_cmd_len = cmd_len - 3;
			memcpy((u8 *)(&h2c_cmd_ex), cmdbuffer + 3, ext_cmd_len);

			msgbox_ex_addr = REG_HMEBOX_EXT_0 +
				(h2c_box_num * RTL88eu_EX_MESSAGE_BOX_SIZE);

			for (cmd_idx = 0; cmd_idx < ext_cmd_len; cmd_idx++)
				rtl8xxxu_write8(priv, msgbox_ex_addr + cmd_idx,
					       *((u8 *)(&h2c_cmd_ex) +
						cmd_idx));
		}

		msgbox_addr = REG_HMEBOX_0 + (h2c_box_num *
					     RTL88eu_MESSAGE_BOX_SIZE);

		cmd_idx = 0;
		for (; cmd_idx < RTL88eu_MESSAGE_BOX_SIZE; cmd_idx++) {
			rtl8xxxu_write8(priv, msgbox_addr + cmd_idx,
				       *((u8 *)(&h2c_cmd) + cmd_idx));
		}
		bcmd_down = true;

		last_hmeboxnum = (h2c_box_num + 1) %
					  RTL88eu_MAX_H2C_BOX_NUMS;
	} while ((!bcmd_down) && (retry_cnts--));
}

#if 0
void rtl88eu_set_fw_pwrmode_cmd(struct rtl8xxxu_priv *priv, u8 mode)
{
	u8 u1_h2c_set_pwrmode[H2C_88E_PWEMODE_LENGTH] = { 0 };
	u8 rlbm = 0;

	RT_TRACE(priv, COMP_POWER, DBG_LOUD, "FW LPS mode = %d\n", mode);

	switch (mode) {
	case PS_MODE_ACTIVE:
		u1_h2c_set_pwrmode[0] = 0;
		break;
	case PS_MODE_MIN:
		u1_h2c_set_pwrmode[0] = 1;
		break;
	case PS_MODE_MAX:
		u1_h2c_set_pwrmode[0] = 1;
		break;
	case PS_MODE_DTIM:
		u1_h2c_set_pwrmode[0] = 1;
		break;
	case PS_MODE_UAPSD_WMM:
		u1_h2c_set_pwrmode[0] = 2;
		break;
	default:
		u1_h2c_set_pwrmode[0] = 0;
		break;
	}

	u1_h2c_set_pwrmode[1] = (((ppsc->smart_ps << 4) & 0xf0) | (rlbm & 0x0f));
	u1_h2c_set_pwrmode[2] = 1;
	u1_h2c_set_pwrmode[3] = 0;
	
	if (mode)
		u1_h2c_set_pwrmode[4] = 0x00;
	else
		u1_h2c_set_pwrmode[4] = 0x0c;
	
	RT_PRINT_DATA(rtlpriv, COMP_CMD, DBG_DMESG,
		      "rtl88eu_set_fw_pwrmode(): u1_h2c_set_pwrmode\n",
		      u1_h2c_set_pwrmode, H2C_88E_PWEMODE_LENGTH);
	rtl88eu_fill_h2c_cmd(priv, H2C_88E_SETPWRMODE,
					H2C_88E_PWEMODE_LENGTH, u1_h2c_set_pwrmode);

}
#endif


void rtl88e_phy_rf6052_set_bandwidth(struct rtl8xxxu_priv *priv, u8 bandwidth)
{
	u32 val32;
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

	rtl88e_dm_txpower_track_adjust(priv, 1, &direction, &pwrtrac_value);
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
	rtl88e_dm_txpower_track_adjust(priv, 1, &direction, &pwrtrac_value);

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

static void _rtl8188e_config_rf_reg(struct rtl8xxxu_priv *priv, u32 addr,
				    u32 data, enum radio_path rfpath,
				    u32 regaddr)
{
	if (addr == 0xffe) {
		mdelay(50);
	} else if (addr == 0xfd) {
		mdelay(5);
	} else if (addr == 0xfc) {
		mdelay(1);
	} else if (addr == 0xfb) {
		udelay(50);
	} else if (addr == 0xfa) {
		udelay(5);
	} else if (addr == 0xf9) {
		udelay(1);
	} else {
		rtl_set_rfreg(priv, rfpath, regaddr,
			      RFREG_OFFSET_MASK,
			      data);
		udelay(1);
	}
}

static void _rtl8188e_config_rf_radio_a(struct rtl8xxxu_priv *priv,
					u32 addr, u32 data)
{
	u32 content = 0x1000; /*RF Content: radio_a_txt*/
	u32 maskforphyset = (u32)(content & 0xE000);

	_rtl8188e_config_rf_reg(priv, addr, data, RF90_PATH_A,
		addr | maskforphyset);
}



static void process_path_a(struct rtl8xxxu_priv *priv,
			   u16  radioa_arraylen,
			   u32 *radioa_array_table)
{
	u32 v1, v2;
	int i;

	for (i = 0; i < radioa_arraylen; i = i + 2) {
		v1 = radioa_array_table[i];
		v2 = radioa_array_table[i+1];
			_rtl8188e_config_rf_radio_a(priv, v1, v2);
	}
}



bool rtl88e_phy_config_rf_with_headerfile(struct rtl8xxxu_priv *priv,
					  enum radio_path rfpath)
{
	bool rtstatus = true;
	u32 *radioa_array_table;
	u16 radioa_arraylen;

	radioa_arraylen = RTL8188EU_RADIOA_1TARRAYLEN;
	radioa_array_table = RTL8188EU_RADIOA_1TARRAY;
	RT_TRACE(priv, COMP_INIT, DBG_LOUD,
		 "Radio_A:RTL8188EU_RADIOA_1TARRAY %d\n", radioa_arraylen);
	RT_TRACE(priv, COMP_INIT, DBG_LOUD, "Radio No %x\n", rfpath);
	rtstatus = true;
	switch (rfpath) {
	case RF90_PATH_A:
		process_path_a(priv, radioa_arraylen, radioa_array_table);
		break;
	case RF90_PATH_B:
	case RF90_PATH_C:
	case RF90_PATH_D:
		break;
	}
	return true;
}



static bool _rtl88e_phy_rf6052_config_parafile(struct rtl8xxxu_priv *priv)
{
	u32 u4_regvalue = 0;
	u8 rfpath;
	bool rtstatus = true;
	struct bb_reg_def *pphyreg;

	pphyreg = &rtlphy.phyreg_def[0];

	u4_regvalue = rtl_get_bbreg(priv, pphyreg->rfintfs,
			BRFSI_RFENV);

	rtl_set_bbreg(priv, pphyreg->rfintfe, BRFSI_RFENV << 16, 0x1);
	udelay(1);

	rtl_set_bbreg(priv, pphyreg->rfintfo, BRFSI_RFENV, 0x1);
	udelay(1);

	rtl_set_bbreg(priv, pphyreg->rfhssi_para2,
			B3WIREADDREAALENGTH, 0x0);
	udelay(1);

	rtl_set_bbreg(priv, pphyreg->rfhssi_para2, B3WIREDATALENGTH, 0x0);
	udelay(1);

	rtstatus = rtl88e_phy_config_rf_with_headerfile(priv,
			(enum radio_path)RF90_PATH_A);

	rtl_set_bbreg(priv, pphyreg->rfintfs,
			BRFSI_RFENV, u4_regvalue);
	if (rtstatus != true) {
		RT_TRACE(priv, COMP_INIT, DBG_TRACE,
				"Radio[%d] Fail!!", rfpath);
		return false;

}

RT_TRACE(priv, COMP_INIT, DBG_TRACE, "\n");
return rtstatus;
}

bool rtl88e_phy_rf6052_config(struct rtl8xxxu_priv *priv)
{
	return _rtl88e_phy_rf6052_config_parafile(priv);
}

bool rtl88e_phy_rf_config(struct rtl8xxxu_priv *priv)
{
	return rtl88e_phy_rf6052_config(priv);
}

static bool _rtl88e_phy_config_mac_with_headerfile(struct rtl8xxxu_priv *priv)
{
	u32 i;
	u32 arraylength;
	u32 *ptrarray;

	RT_TRACE(priv, COMP_INIT, DBG_TRACE, "Read Rtl8188EMACPHY_Array\n");
	arraylength = RTL8188EUMAC_1T_ARRAYLEN;
	ptrarray = RTL8188EUMAC_1T_ARRAY;
	RT_TRACE(priv, COMP_INIT, DBG_LOUD,
		 "Img:RTL8188EUMAC_1T_ARRAY LEN %d\n", arraylength);
	for (i = 0; i < arraylength; i = i + 2)
		rtl8xxxu_write8(priv, ptrarray[i], (u8)ptrarray[i + 1]);
	return true;
}

static void _rtl88e_phy_init_bb_rf_register_definition(struct rtl8xxxu_priv *priv)
{

	rtlphy.phyreg_def[0].rfintfs = RFPGA0_XAB_RFINTERFACESW;
	rtlphy.phyreg_def[0].rfintfi = RFPGA0_XAB_RFINTERFACERB;
	rtlphy.phyreg_def[0].rfintfo = RFPGA0_XA_RFINTERFACEOE;
	rtlphy.phyreg_def[0].rfintfe = RFPGA0_XA_RFINTERFACEOE;
	rtlphy.phyreg_def[0].rf3wire_offset = RFPGA0_XA_LSSIPARAMETER;
	rtlphy.phyreg_def[0].rflssi_select = RFPGA0_XAB_RFPARAMETER;
	rtlphy.phyreg_def[0].rftxgain_stage = RFPGA0_TXGAINSTAGE;
	rtlphy.phyreg_def[0].rfhssi_para1 = RFPGA0_XA_HSSIPARAMETER1;
	rtlphy.phyreg_def[0].rfhssi_para2 = RFPGA0_XA_HSSIPARAMETER2;
	rtlphy.phyreg_def[0].rfsw_ctrl = RFPGA0_XAB_SWITCHCONTROL;
	rtlphy.phyreg_def[0].rfagc_control1 = ROFDM0_XAAGCCORE1;
	rtlphy.phyreg_def[0].rfagc_control2 = ROFDM0_XAAGCCORE2;
	rtlphy.phyreg_def[0].rfrxiq_imbal = ROFDM0_XARXIQIMBALANCE;
	rtlphy.phyreg_def[0].rfrx_afe = ROFDM0_XARXAFE;
	rtlphy.phyreg_def[0].rftxiq_imbal = ROFDM0_XATXIQIMBALANCE;
	rtlphy.phyreg_def[0].rftx_afe = ROFDM0_XATXAFE;
	rtlphy.phyreg_def[0].rf_rb = RFPGA0_XA_LSSIREADBACK;
	rtlphy.phyreg_def[0].rf_rbpi = TRANSCEIVEA_HSPI_READBACK;
}

static void _rtl8188e_config_bb_reg(struct rtl8xxxu_priv *priv,
				    u32 addr, u32 data)
{
	if (addr == 0xfe) {
		mdelay(50);
	} else if (addr == 0xfd) {
		mdelay(5);
	} else if (addr == 0xfc) {
		mdelay(1);
	} else if (addr == 0xfb) {
		udelay(50);
	} else if (addr == 0xfa) {
		udelay(5);
	} else if (addr == 0xf9) {
		udelay(1);
	} else {
		rtl_set_bbreg(priv, addr, MASKDWORD, data);
		udelay(1);
	}
}

static bool _rtl88e_check_condition(struct rtl8xxxu_priv *priv,
				    const u32  condition)
{
	u32 _board = rtlefuse.board_type; /*need efuse define*/
	u32 _interface = INTF_USB;
	u32 _platform = 0x08;/*SupportPlatform */
	u32 cond = condition;

	if (condition == 0xCDCDCDCD)
		return true;

	cond = condition & 0xFF;
	if ((_board & cond) == 0 && cond != 0x1F)
		return false;

	cond = condition & 0xFF00;
	cond = cond >> 8;
	if ((_interface & cond) == 0 && cond != 0x07)
		return false;

	cond = condition & 0xFF0000;
	cond = cond >> 16;
	if ((_platform & cond) == 0 && cond != 0x0F)
		return false;
	return true;
}

#define READ_NEXT_PAIR(v1, v2, i)			\
	do {						\
		i += 2; v1 = array_table[i];		\
		v2 = array_table[i+1];			\
	} while (0)

static void handle_branch1(struct rtl8xxxu_priv *priv, u16 arraylen,
			   u32 *array_table)
{
	u32 v1;
	u32 v2;
	int i;

	for (i = 0; i < arraylen; i = i + 2) {
		v1 = array_table[i];
		v2 = array_table[i+1];
		_rtl8188e_config_bb_reg(priv, v1, v2);
	}
}

static void handle_branch2(struct rtl8xxxu_priv *priv, u16 arraylen,
			   u32 *array_table)
{
	u32 v1;
	u32 v2;
	int i;

	for (i = 0; i < arraylen; i = i + 2) {
		v1 = array_table[i];
		v2 = array_table[i+1];
		rtl_set_bbreg(priv, array_table[i], MASKDWORD,
				      array_table[i + 1]);
		udelay(1);
		RT_TRACE(priv, COMP_INIT, DBG_TRACE,
			 "The agctab_array_table[0] is %x Rtl818EEPHY_REGArray[1] is %x\n",
			 array_table[i], array_table[i + 1]);
	}
}

static bool phy_config_bb_with_headerfile(struct rtl8xxxu_priv *priv,
					  u8 configtype)
{
	u32 *array_table;
	u16 arraylen;

	if (configtype == BASEBAND_CONFIG_PHY_REG) {
		arraylen = RTL8188EUPHY_REG_1TARRAYLEN;
		array_table = RTL8188EUPHY_REG_1TARRAY;
		handle_branch1(priv, arraylen, array_table);
	} else if (configtype == BASEBAND_CONFIG_AGC_TAB) {
		arraylen = RTL8188EUAGCTAB_1TARRAYLEN;
		array_table = RTL8188EUAGCTAB_1TARRAY;
		handle_branch2(priv, arraylen, array_table);
	}
	return true;
}

static void store_pwrindex_rate_offset(struct rtl8xxxu_priv *priv,
				       u32 regaddr, u32 bitmask,
				       u32 data)
{
	int count = rtlphy.pwrgroup_cnt;

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
	if (regaddr == RTXAGC_B_RATE18_06)
		rtlphy.mcs_txpwrlevel_origoffset[count][8] = data;
	if (regaddr == RTXAGC_B_RATE54_24)
		rtlphy.mcs_txpwrlevel_origoffset[count][9] = data;
	if (regaddr == RTXAGC_B_CCK1_55_MCS32)
		rtlphy.mcs_txpwrlevel_origoffset[count][14] = data;
	if (regaddr == RTXAGC_B_CCK11_A_CCK2_11 && bitmask == 0x000000ff)
		rtlphy.mcs_txpwrlevel_origoffset[count][15] = data;
	if (regaddr == RTXAGC_B_MCS03_MCS00)
		rtlphy.mcs_txpwrlevel_origoffset[count][10] = data;
	if (regaddr == RTXAGC_B_MCS07_MCS04)
		rtlphy.mcs_txpwrlevel_origoffset[count][11] = data;
	if (regaddr == RTXAGC_B_MCS11_MCS08)
		rtlphy.mcs_txpwrlevel_origoffset[count][12] = data;
	if (regaddr == RTXAGC_B_MCS15_MCS12)
		rtlphy.mcs_txpwrlevel_origoffset[count][13] = data;
}

static bool phy_config_bb_with_pghdr(struct rtl8xxxu_priv *priv, u8 configtype)
{
	int i;
	u32 *phy_reg_page;
	u16 phy_reg_page_len;
	u32 v1 = 0, v2 = 0, v3 = 0;

	phy_reg_page_len = RTL8188EUPHY_REG_ARRAY_PGLEN;
	phy_reg_page = RTL8188EUPHY_REG_ARRAY_PG;

	if (configtype == BASEBAND_CONFIG_PHY_REG) {
		for (i = 0; i < phy_reg_page_len; i = i + 3) {
			v1 = phy_reg_page[i];
			v2 = phy_reg_page[i+1];
			v3 = phy_reg_page[i+2];

				if (phy_reg_page[i] == 0xfe)
					mdelay(50);
				else if (phy_reg_page[i] == 0xfd)
					mdelay(5);
				else if (phy_reg_page[i] == 0xfc)
					mdelay(1);
				else if (phy_reg_page[i] == 0xfb)
					udelay(50);
				else if (phy_reg_page[i] == 0xfa)
					udelay(5);
				else if (phy_reg_page[i] == 0xf9)
					udelay(1);

				store_pwrindex_rate_offset(priv, phy_reg_page[i],
							   phy_reg_page[i + 1],
							   phy_reg_page[i + 2]);
		}
	} else {
		RT_TRACE(priv, COMP_SEND, DBG_TRACE,
			 "configtype != BaseBand_Config_PHY_REG\n");
	}
	return true;
}

#define READ_NEXT_RF_PAIR(v1, v2, i) \
do { \
	i += 2; \
	v1 = radioa_array_table[i]; \
	v2 = radioa_array_table[i+1]; \
} while (0)

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
	u8 rf_path = 0;

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

static u8 _rtl88e_phy_path_b_iqk(struct rtl8xxxu_priv *priv)
{
	u32 reg_eac, reg_eb4, reg_ebc, reg_ec4, reg_ecc;
	u8 result = 0x00;

	rtl_set_bbreg(priv, 0xe60, MASKDWORD, 0x00000002);
	rtl_set_bbreg(priv, 0xe60, MASKDWORD, 0x00000000);
	mdelay(IQK_DELAY_TIME);
	reg_eac = rtl_get_bbreg(priv, 0xeac, MASKDWORD);
	reg_eb4 = rtl_get_bbreg(priv, 0xeb4, MASKDWORD);
	reg_ebc = rtl_get_bbreg(priv, 0xebc, MASKDWORD);
	reg_ec4 = rtl_get_bbreg(priv, 0xec4, MASKDWORD);
	reg_ecc = rtl_get_bbreg(priv, 0xecc, MASKDWORD);

	if (!(reg_eac & BIT(31)) &&
	    (((reg_eb4 & 0x03FF0000) >> 16) != 0x142) &&
	    (((reg_ebc & 0x03FF0000) >> 16) != 0x42))
		result |= 0x01;
	else
		return result;
	if (!(reg_eac & BIT(30)) &&
	    (((reg_ec4 & 0x03FF0000) >> 16) != 0x132) &&
	    (((reg_ecc & 0x03FF0000) >> 16) != 0x36))
		result |= 0x02;
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

static void _rtl88e_phy_path_a_standby(struct rtl8xxxu_priv *priv)
{
	rtl_set_bbreg(priv, 0xe28, MASKDWORD, 0x0);
	rtl_set_bbreg(priv, 0x840, MASKDWORD, 0x00010000);
	rtl_set_bbreg(priv, 0xe28, MASKDWORD, 0x80800000);
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
	u8 patha_ok, pathb_ok;
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
#if 0
	/* 8188eu는 2t아님... */
	if (is2t) {
		_rtl88e_phy_path_a_standby(hw);
		_rtl88e_phy_path_adda_on(hw, adda_reg, false, is2t);
		for (i = 0; i < retrycount; i++) {
			pathb_ok = _rtl88e_phy_path_b_iqk(hw);
			if (pathb_ok == 0x03) {
				result[t][4] = (rtl_get_bbreg(priv,
							      0xeb4,
							      MASKDWORD) &
						0x3FF0000) >> 16;
				result[t][5] =
				    (rtl_get_bbreg(priv, 0xebc, MASKDWORD) &
				     0x3FF0000) >> 16;
				result[t][6] =
				    (rtl_get_bbreg(priv, 0xec4, MASKDWORD) &
				     0x3FF0000) >> 16;
				result[t][7] =
				    (rtl_get_bbreg(priv, 0xecc, MASKDWORD) &
				     0x3FF0000) >> 16;
				break;
			} else if (i == (retrycount - 1) && pathb_ok == 0x01) {
				result[t][4] = (rtl_get_bbreg(priv,
							      0xeb4,
							      MASKDWORD) &
						0x3FF0000) >> 16;
			}
			result[t][5] = (rtl_get_bbreg(priv, 0xebc, MASKDWORD) &
					0x3FF0000) >> 16;
		}
	}
#endif

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
#if 1
		/* 알아보고 구현할것 */
		if (rtlefuse.antenna_div_type == CGCS_RX_HW_ANTDIV)
			rtl_set_bbreg(priv, RCONFIG_RAM64x16, BIT(31), 0);
#endif
	} else {
		rtl_set_bbreg(priv, RFPGA0_XA_RFINTERFACEOE,
				BIT(14) | BIT(13) | BIT(12), 1);
		rtl_set_bbreg(priv, RFPGA0_XB_RFINTERFACEOE,
				BIT(5) | BIT(4) | BIT(3), 1);
#if 1
		/* 알아보고 구현할것. */
		if (rtlefuse.antenna_div_type == CGCS_RX_HW_ANTDIV)
			rtl_set_bbreg(priv, RCONFIG_RAM64x16, BIT(31), 1);
#endif
	}
}

#if 0
static void rtl88e_phy_set_io(struct rtl8xxxu_priv *priv)
{
	RT_TRACE(priv, COMP_CMD, DBG_TRACE,
		 "--->Cmd(%#x), set_io_inprogress(%d)\n",
		  rtlphy.current_io_type, rtlphy.set_io_inprogress);
	switch (rtlphy.current_io_type) {
	case IO_CMD_RESUME_DM_BY_SCAN:
		dm_dig.cur_igvalue = rtlphy.initgain_backup.xaagccore1;
		/*rtl92c_dm_write_dig(hw);*/
		rtl88e_phy_set_txpower_level(hw, rtlphy.current_channel);
		rtl_set_bbreg(priv, RCCK0_CCA, 0xff0000, 0x83);
		break;
	case IO_CMD_PAUSE_BAND0_DM_BY_SCAN:
		rtlphy.initgain_backup.xaagccore1 = dm_dig.cur_igvalue;
		dm_dig.cur_igvalue = 0x17;
		rtl_set_bbreg(priv, RCCK0_CCA, 0xff0000, 0x40);
		break;
	default:
		RT_TRACE(priv, COMP_ERR, DBG_LOUD,
			 "switch case not process\n");
		break;
	}
	rtlphy.set_io_inprogress = false;
	RT_TRACE(priv, COMP_CMD, DBG_TRACE,
		 "(%#x)\n", rtlphy.current_io_type);
}
#endif

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

#if 0
static bool _rtl88ee_phy_set_rf_power_state(struct rtl8xxxu_priv *priv,
					    enum rf_pwrstate rfpwr_state)
{
	bool bresult = true;
	u8 i, queue_id;
	struct rtl8192_tx_ring *ring = NULL;

	switch (rfpwr_state) {
	case ERFON:
		if ((ppsc->rfpwr_state == ERFOFF) &&
		    RT_IN_PS_LEVEL(ppsc, RT_RF_OFF_LEVL_HALT_NIC)) {
			bool rtstatus;
			u32 initializecount = 0;

			do {
				initializecount++;
				RT_TRACE(priv, COMP_RF, DBG_DMESG,
					 "IPS Set eRf nic enable\n");
				rtstatus = rtl_ps_enable_nic(hw);
			} while (!rtstatus &&
				 (initializecount < 10));
			RT_CLEAR_PS_LEVEL(ppsc,
					  RT_RF_OFF_LEVL_HALT_NIC);
		} else {
			RT_TRACE(priv, COMP_RF, DBG_DMESG,
				 "Set ERFON sleeped:%d ms\n",
				  jiffies_to_msecs(jiffies -
						   ppsc->
						   last_sleep_jiffies));
			ppsc->last_awake_jiffies = jiffies;
			rtl88ee_phy_set_rf_on(hw);
		}
		if (mac->link_state == MAC80211_LINKED) {
			rtlpriv->cfg->ops->led_control(hw,
						       LED_CTL_LINK);
		} else {
			rtlpriv->cfg->ops->led_control(hw,
						       LED_CTL_NO_LINK);
		}
		break;
	case ERFOFF:
		for (queue_id = 0, i = 0;
		     queue_id < RTL_PCI_MAX_TX_QUEUE_COUNT;) {
			ring = &pcipriv->dev.tx_ring[queue_id];
			if (queue_id == BEACON_QUEUE ||
			    skb_queue_len(&ring->queue) == 0) {
				queue_id++;
				continue;
			} else {
				RT_TRACE(priv, COMP_ERR, DBG_WARNING,
					 "eRf Off/Sleep: %d times TcbBusyQueue[%d] =%d before doze!\n",
					 (i + 1), queue_id,
					 skb_queue_len(&ring->queue));

				udelay(10);
				i++;
			}
			if (i >= MAX_DOZE_WAITING_TIMES_9x) {
				RT_TRACE(priv, COMP_ERR, DBG_WARNING,
					 "\n ERFSLEEP: %d times TcbBusyQueue[%d] = %d !\n",
					  MAX_DOZE_WAITING_TIMES_9x,
					  queue_id,
					  skb_queue_len(&ring->queue));
				break;
			}
		}

		if (ppsc->reg_rfps_level & RT_RF_OFF_LEVL_HALT_NIC) {
			RT_TRACE(priv, COMP_RF, DBG_DMESG,
				 "IPS Set eRf nic disable\n");
			rtl_ps_disable_nic(hw);
			RT_SET_PS_LEVEL(ppsc, RT_RF_OFF_LEVL_HALT_NIC);
		} else {
			if (ppsc->rfoff_reason == RF_CHANGE_BY_IPS) {
				rtlpriv->cfg->ops->led_control(hw,
							       LED_CTL_NO_LINK);
			} else {
				rtlpriv->cfg->ops->led_control(hw,
							       LED_CTL_POWER_OFF);
			}
		}
		break;
	case ERFSLEEP:{
			if (ppsc->rfpwr_state == ERFOFF)
				break;
			for (queue_id = 0, i = 0;
			     queue_id < RTL_PCI_MAX_TX_QUEUE_COUNT;) {
				ring = &pcipriv->dev.tx_ring[queue_id];
				if (skb_queue_len(&ring->queue) == 0) {
					queue_id++;
					continue;
				} else {
					RT_TRACE(priv, COMP_ERR, DBG_WARNING,
						 "eRf Off/Sleep: %d times TcbBusyQueue[%d] =%d before doze!\n",
						 (i + 1), queue_id,
						 skb_queue_len(&ring->queue));

					udelay(10);
					i++;
				}
				if (i >= MAX_DOZE_WAITING_TIMES_9x) {
					RT_TRACE(priv, COMP_ERR, DBG_WARNING,
						 "\n ERFSLEEP: %d times TcbBusyQueue[%d] = %d !\n",
						 MAX_DOZE_WAITING_TIMES_9x,
						 queue_id,
						 skb_queue_len(&ring->queue));
					break;
				}
			}
			RT_TRACE(priv, COMP_RF, DBG_DMESG,
				 "Set ERFSLEEP awaked:%d ms\n",
				  jiffies_to_msecs(jiffies -
				  ppsc->last_awake_jiffies));
			ppsc->last_sleep_jiffies = jiffies;
			_rtl88ee_phy_set_rf_sleep(hw);
			break;
		}
	default:
		RT_TRACE(priv, COMP_ERR, DBG_LOUD,
			 "switch case not process\n");
		bresult = false;
		break;
	}
	if (bresult)
		ppsc->rfpwr_state = rfpwr_state;
	return bresult;
}
#endif

static bool _rtl88e_phy_bb8188e_config_parafile(struct rtl8xxxu_priv *priv)
{
	bool rtstatus;

	rtstatus = phy_config_bb_with_headerfile(priv, BASEBAND_CONFIG_PHY_REG);
	if (!rtstatus) {
		RT_TRACE(priv, COMP_ERR, DBG_EMERG, "Write BB Reg Fail!!");
		return false;
	}

	rtstatus = phy_config_bb_with_pghdr(priv, BASEBAND_CONFIG_PHY_REG);
	if (!rtstatus) {
		RT_TRACE(priv, COMP_ERR, DBG_EMERG, "BB_PG Reg Fail!!");
		return false;
	}
	rtstatus =
	  phy_config_bb_with_headerfile(priv, BASEBAND_CONFIG_AGC_TAB);
	if (!rtstatus) {
		RT_TRACE(priv, COMP_ERR, DBG_EMERG, "AGC Table Fail\n");
		return false;
	}
	/* 사용되어지진 않음. */
	rtlphy.cck_high_power =
	  (bool)(rtl_get_bbreg(priv, RFPGA0_XA_HSSIPARAMETER2, 0x200));

	return true;
}

#if 0
bool rtl88e_phy_set_rf_power_state(struct rtl8xxxu_priv *priv,
				   enum rf_pwrstate rfpwr_state)
{
	bool bresult = false;

	if (rfpwr_state == ppsc->rfpwr_state)
		return bresult;
	bresult = _rtl88ee_phy_set_rf_power_state(hw, rfpwr_state);
	return bresult;
}
#endif

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

u8 rtl88e_phy_sw_chnl(struct rtl8xxxu_priv *priv)
{

	if (rtlphy.sw_chnl_inprogress)
		return 0;
	if (rtlphy.set_bwmode_inprogress)
		return 0;
#if 0
	RT_ASSERT((priv.current_channel <= 14),
		  "WIRELESS_MODE_G but channel>14");
#endif
	rtlphy.sw_chnl_inprogress = true;
	rtlphy.sw_chnl_stage = 0;
	rtlphy.sw_chnl_step = 0;
#if 0
	if (!(is_hal_stop(rtlhal)))) {
#else
	if (1) {
#endif
		rtl88e_phy_sw_chnl_callback(priv);
		RT_TRACE(priv, COMP_CHAN, DBG_LOUD,
			 "sw_chnl_inprogress false schdule workitem current channel %d\n",
			 rtlphy.current_channel);
		rtlphy.sw_chnl_inprogress = false;
	} else {
		RT_TRACE(priv, COMP_CHAN, DBG_LOUD,
			 "sw_chnl_inprogress false driver sleep or unload\n");
		rtlphy.sw_chnl_inprogress = false;
	}
	return 1;
}

bool rtl88e_phy_mac_config(struct rtl8xxxu_priv *priv)
{
	bool rtstatus = _rtl88e_phy_config_mac_with_headerfile(priv);

	rtl8xxxu_write8(priv, 0x04CA, 0x0B);
	return rtstatus;
}

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
#if 0
bool rtl88e_phy_set_io_cmd(struct rtl8xxxu_priv *priv, enum io_type iotype)
{
	bool postprocessing = false;

	RT_TRACE(priv, COMP_CMD, DBG_TRACE,
		 "-->IO Cmd(%#x), set_io_inprogress(%d)\n",
		  iotype, rtlphy.set_io_inprogress);
	do {
		switch (iotype) {
		case IO_CMD_RESUME_DM_BY_SCAN:
			RT_TRACE(priv, COMP_CMD, DBG_TRACE,
				 "[IO CMD] Resume DM after scan.\n");
			postprocessing = true;
			break;
		case IO_CMD_PAUSE_BAND0_DM_BY_SCAN:
			RT_TRACE(priv, COMP_CMD, DBG_TRACE,
				 "[IO CMD] Pause DM before scan.\n");
			postprocessing = true;
			break;
		default:
			RT_TRACE(priv, COMP_ERR, DBG_LOUD,
				 "switch case not process\n");
			break;
		}
	} while (false);
	if (postprocessing && !rtlphy.set_io_inprogress) {
		rtlphy.set_io_inprogress = true;
		rtlphy.current_io_type = iotype;
	} else {
		return false;
	}
	rtl88e_phy_set_io(hw);
	RT_TRACE(priv, COMP_CMD, DBG_TRACE, "IO Type(%#x)\n", iotype);
	return true;
}
#endif

bool rtl88e_phy_bb_config(struct rtl8xxxu_priv *priv)
{
	bool rtstatus = true;
	u16 regval;
	u32 tmp;
	_rtl88e_phy_init_bb_rf_register_definition(priv);
	regval = rtl8xxxu_read16(priv, REG_SYS_FUNC_EN);
	rtl8xxxu_write16(priv, REG_SYS_FUNC_EN,
		       regval | BIT(13) | BIT(0) | BIT(1));

	rtl8xxxu_write8(priv, REG_RF_CTRL, RF_EN | RF_RSTB | RF_SDMRSTB);
	rtl8xxxu_write8(priv, REG_SYS_FUNC_EN,
		       FEN_USBA | FEN_USBD | FEN_BB_GLB_RSTN | FEN_BBRSTB);
	tmp = rtl8xxxu_read32(priv, 0x4c);
	rtl8xxxu_write32(priv, 0x4c, tmp | BIT(23));
	rtstatus = _rtl88e_phy_bb8188e_config_parafile(priv);
	return rtstatus;
}

#if 0
void rtl88e_phy_get_hw_reg_originalvalue(struct rtl8xxxu_priv *priv)
{

	rtlphy.default_initialgain[0] =
	    (u8)rtl_get_bbreg(priv, ROFDM0_XAAGCCORE1, MASKBYTE0);
	rtlphy.default_initialgain[1] =
	    (u8)rtl_get_bbreg(priv, ROFDM0_XBAGCCORE1, MASKBYTE0);
	rtlphy.default_initialgain[2] =
	    (u8)rtl_get_bbreg(priv, ROFDM0_XCAGCCORE1, MASKBYTE0);
	rtlphy.default_initialgain[3] =
	    (u8)rtl_get_bbreg(priv, ROFDM0_XDAGCCORE1, MASKBYTE0);

	RT_TRACE(priv, COMP_INIT, DBG_TRACE,
		 "Default initial gain (c50=0x%x, c58=0x%x, c60=0x%x, c68=0x%x\n",
		 rtlphy.default_initialgain[0],
		 rtlphy.default_initialgain[1],
		 rtlphy.default_initialgain[2],
		 rtlphy.default_initialgain[3]);

	rtlphy.framesync = (u8)rtl_get_bbreg(priv, ROFDM0_RXDETECTOR3,
					      MASKBYTE0);
	rtlphy.framesync_c34 = rtl_get_bbreg(priv, ROFDM0_RXDETECTOR2,
					      MASKDWORD);

	RT_TRACE(priv, COMP_INIT, DBG_TRACE,
		 "Default framesync (0x%x) = 0x%x\n",
		 ROFDM0_RXDETECTOR3, rtlphy.framesync);
}

static long _rtl88e_phy_txpwr_idx_to_dbm(struct rtl8xxxu_priv *priv,
					 enum wireless_mode wirelessmode,
					 u8 txpwridx)
{
	long offset;
	long pwrout_dbm;

	switch (wirelessmode) {
	case WIRELESS_MODE_B:
		offset = -7;
		break;
	case WIRELESS_MODE_G:
	case WIRELESS_MODE_N_24G:
		offset = -8;
		break;
	default:
		offset = -8;
		break;
	}
	pwrout_dbm = txpwridx / 2 + offset;
	return pwrout_dbm;
}

void rtl88e_phy_get_txpower_level(struct rtl8xxxu_priv *priv, long *powerlevel)
{
	u8 txpwr_level;
	long txpwr_dbm;

	txpwr_level = rtlphy.cur_cck_txpwridx;
	txpwr_dbm = _rtl88e_phy_txpwr_idx_to_dbm(hw,
						 WIRELESS_MODE_B, txpwr_level);
	txpwr_level = rtlphy.cur_ofdm24g_txpwridx;
	if (_rtl88e_phy_txpwr_idx_to_dbm(hw,
					 WIRELESS_MODE_G,
					 txpwr_level) > txpwr_dbm)
		txpwr_dbm =
		    _rtl88e_phy_txpwr_idx_to_dbm(hw, WIRELESS_MODE_G,
						 txpwr_level);
	txpwr_level = rtlphy.cur_ofdm24g_txpwridx;
	if (_rtl88e_phy_txpwr_idx_to_dbm(hw,
					 WIRELESS_MODE_N_24G,
					 txpwr_level) > txpwr_dbm)
		txpwr_dbm =
		    _rtl88e_phy_txpwr_idx_to_dbm(hw, WIRELESS_MODE_N_24G,
						 txpwr_level);
	*powerlevel = txpwr_dbm;
}

#endif


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

void rtl88eu_iol_mode_enable(struct rtl8xxxu_priv *priv, u8 enable)
{
	u8 reg_0xf0 = 0;

	if (enable) {
		/* Enable initial offload */
		reg_0xf0 = rtl8xxxu_read8(priv, REG_SYS_CFG);
		rtl8xxxu_write8(priv, REG_SYS_CFG, reg_0xf0 | SW_OFFLOAD_EN);

#if 0
		if (!rtlhal->fw_ready)
			_rtl88eu_reset_8051(priv);
#else
		//여기 확인하라
			_rtl88eu_reset_8051(priv);
#endif
	} else {
		/* disable initial offload */
		reg_0xf0 = rtl8xxxu_read8(priv, REG_SYS_CFG);
		rtl8xxxu_write8(priv, REG_SYS_CFG, reg_0xf0 & ~SW_OFFLOAD_EN);
	}
}

s32 rtl88eu_iol_execute(struct rtl8xxxu_priv *priv, u8 control)
{
	s32 status = false;
	u8 reg_0x88 = 0;
	u32 start = 0, passing_time = 0;

	control = control & 0x0f;
	reg_0x88 = rtl8xxxu_read8(priv, REG_HMBOX_EXT_0);
	rtl8xxxu_write8(priv, REG_HMBOX_EXT_0, reg_0x88 | control);

	start = jiffies;
	while ((reg_0x88 = rtl8xxxu_read8(priv, REG_HMBOX_EXT_0)) & control &&
	       (passing_time = rtl_get_passing_time_ms(start)) < 1000) {
		;
	}

	reg_0x88 = rtl8xxxu_read8(priv, REG_HMBOX_EXT_0);
	status = (reg_0x88 & control) ? false : true;
	if (reg_0x88 & control<<4)
		status = false;
	return status;
}

static s32 _rtl88eu_iol_init_llt_table(struct rtl8xxxu_priv *priv, u8 boundary)
{
	s32 rst = true;

	rtl88eu_iol_mode_enable(priv, 1);
	rtl8xxxu_write8(priv, REG_TDECTRL+1, boundary);
	rst = rtl88eu_iol_execute(priv, CMD_INIT_LLT);
	rtl88eu_iol_mode_enable(priv, 0);
	return rst;
}

bool rtl88eu_iol_applied(struct rtl8xxxu_priv *priv)
{
	/* TODO*/
	return true;
}

s32 rtl88eu_iol_efuse_patch(struct rtl8xxxu_priv *priv)
{
	s32 result = true;

	if (rtl88eu_iol_applied(priv)) {
		rtl88eu_iol_mode_enable(priv, 1);
		result = rtl88eu_iol_execute(priv, CMD_READ_EFUSE_MAP);
		if (result == true)
			result = rtl88eu_iol_execute(priv, CMD_EFUSE_PATCH);

		rtl88eu_iol_mode_enable(priv, 0);
	}
	return result;
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
#if 0
	RT_TRACE(priv, COMP_ERR, DBG_LOUD,
		 "Chip RF Type: %s\n", (rtlphy.rf_type == RF_2T2R) ?
		 "RF_2T2R" : "RF_1T1R");
#endif
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

s32 rtl88eu_init_llt_table(struct rtl8xxxu_priv *priv, u8 boundary)
{
	s32 status = false;
	u32 i;
	u32 last_entry_of_tx_pkt_buf = LAST_ENTRY_OF_TX_PKT_BUFFER;

	if (rtl88eu_iol_applied(priv)) {
		status = _rtl88eu_iol_init_llt_table(priv, boundary);
	} else {
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

#if 0
		for (i = 0; i < 14; i++) {
			RTPRINT(priv, FINIT, INIT_TXPOWER,
				"RF(%d)-Ch(%d) [CCK / HT40_1S ] = [0x%x / 0x%x ]\n",
				rf_path, i,
				rtlefuse.txpwrlevel_cck[rf_path][i],
				rtlefuse.txpwrlevel_ht40_1s[rf_path][i]);
		}
#endif
	}
	
	if (!rtlefuse.autoload_failflag) {
		rtlefuse.eeprom_regulatory =
			(hwinfo[EEPROM_RF_BOARD_OPTION_88E] & 0x7);
		if (hwinfo[EEPROM_RF_BOARD_OPTION_88E] == 0xFF)
			rtlefuse.eeprom_regulatory =
				(EEPROM_DEFAULT_BOARD_OPTION & 0x7);
	} else {
		rtlefuse.eeprom_regulatory = 0;
	}
	RT_TRACE(priv, COMP_ERR, DBG_WARNING,
		 "eeprom_regulatory = 0x%x\n", rtlefuse.eeprom_regulatory);
}

static void efuse_power_switch(struct rtl8xxxu_priv *priv, u8 write, u8 pwrstate)
{
	u8 tempval;
	u16 tmpV16;

	rtl8xxxu_write8(priv, REG_EFUSE_ACCESS, 0x69);
	tmpV16 = rtl8xxxu_read16(priv, REG_SYS_FUNC_EN);
	
	if (!(tmpV16 & FEN_ELDR)) {
		tmpV16 |= FEN_ELDR; 
		rtl8xxxu_write16(priv, REG_SYS_FUNC_EN, tmpV16);
	}

	tmpV16 = rtl8xxxu_read16(priv, REG_SYS_CLKR);
	if ((!(tmpV16 & LOADER_CLK_EN)) ||
			(!(tmpV16 & ANA8M))) {
		tmpV16 |= (LOADER_CLK_EN | ANA8M);
		rtl8xxxu_write16(priv, REG_SYS_CLKR, tmpV16);
	}

	if (pwrstate) {
		if (write) {
			tempval = rtl8xxxu_read8(priv, REG_EFUSE_TEST + 3);
			tempval &= 0x0F;
			tempval |= (VOLTAGE_V25 << 4);
			rtl8xxxu_write8(priv, REG_EFUSE_TEST + 3,
					(tempval | 0x80));
		}
	} else {
		rtl8xxxu_write8(priv,
				REG_EFUSE_ACCESS, 0);
		if (write) {
			tempval = rtl8xxxu_read8(priv, REG_EFUSE_TEST + 3);
			rtl8xxxu_write8(priv, REG_EFUSE_TEST + 3,
				       (tempval & 0x7F));
		}
	}
}

void read_efuse_byte(struct rtl8xxxu_priv *priv, u16 _offset, u8 *pbuf)
{
	u32 value32;
	u8 readbyte;
	u16 retry;

	rtl8xxxu_write8(priv, REG_EFUSE_CTRL + 1,
		       (_offset & 0xff));
	readbyte = rtl8xxxu_read8(priv, REG_EFUSE_CTRL + 2);
	rtl8xxxu_write8(priv, REG_EFUSE_CTRL + 2,
		       ((_offset >> 8) & 0x03) | (readbyte & 0xfc));

	readbyte = rtl8xxxu_read8(priv, REG_EFUSE_CTRL + 3);
	rtl8xxxu_write8(priv, REG_EFUSE_CTRL + 3,
		       (readbyte & 0x7f));

	retry = 0;
	value32 = rtl8xxxu_read32(priv, REG_EFUSE_CTRL);
	while (!(((value32 >> 24) & 0xff) & 0x80) && (retry < 10000)) {
		value32 = rtl8xxxu_read32(priv, REG_EFUSE_CTRL);
		retry++;
	}

	udelay(50);
	value32 = rtl8xxxu_read32(priv, REG_EFUSE_CTRL);

	*pbuf = (u8) (value32 & 0xff);
}

void read_efuse(struct rtl8xxxu_priv *priv, u16 _offset, u16 _size_byte, u8 *pbuf)
{
	u8 *efuse_tbl;
	u8 rtemp8[1];
	u16 efuse_addr = 0;
	u8 offset, wren;
	u8 u1temp = 0;
	u16 i;
	u16 j;
	const u16 efuse_max_section = EFUSE_MAX_SECTION;
	const u32 efuse_len = EFUSE_REAL_CONTENT_LEN;
	u16 **efuse_word;
	u16 efuse_utilized = 0;
	u8 efuse_usage;

	if ((_offset + _size_byte) > HWSET_MAX_SIZE) {
		RT_TRACE(priv, COMP_EFUSE, DBG_LOUD,
			 "read_efuse(): Invalid offset(%#x) with read bytes(%#x)!!\n",
			 _offset, _size_byte);
		return;
	}

	/* allocate memory for efuse_tbl and efuse_word */
	efuse_tbl = kzalloc(HWSET_MAX_SIZE * sizeof(u8), GFP_ATOMIC);
	if (!efuse_tbl)
		return;
	efuse_word = kzalloc(EFUSE_MAX_WORD_UNIT * sizeof(u16 *), GFP_ATOMIC);
	if (!efuse_word)
		goto out;
	for (i = 0; i < EFUSE_MAX_WORD_UNIT; i++) {
		efuse_word[i] = kzalloc(efuse_max_section * sizeof(u16),
					GFP_ATOMIC);
		if (!efuse_word[i])
			goto done;
	}

	for (i = 0; i < efuse_max_section; i++)
		for (j = 0; j < EFUSE_MAX_WORD_UNIT; j++)
			efuse_word[j][i] = 0xFFFF;

	read_efuse_byte(priv, efuse_addr, rtemp8);
	if (*rtemp8 != 0xFF) {
		efuse_utilized++;
		efuse_addr++;
	}

	while ((*rtemp8 != 0xFF) && (efuse_addr < efuse_len)) {
		/*  Check PG header for section num.  */
		if ((*rtemp8 & 0x1F) == 0x0F) {/* extended header */
			u1temp = ((*rtemp8 & 0xE0) >> 5);
			read_efuse_byte(priv, efuse_addr, rtemp8);

			if ((*rtemp8 & 0x0F) == 0x0F) {
				efuse_addr++;
				read_efuse_byte(priv, efuse_addr, rtemp8);

				if (*rtemp8 != 0xFF &&
				    (efuse_addr < efuse_len)) {
					efuse_addr++;
				}
				continue;
			} else {
				offset = ((*rtemp8 & 0xF0) >> 1) | u1temp;
				wren = (*rtemp8 & 0x0F);
				efuse_addr++;
			}
		} else {
			offset = ((*rtemp8 >> 4) & 0x0f);
			wren = (*rtemp8 & 0x0f);
		}

		if (offset < efuse_max_section) {
			for (i = 0; i < EFUSE_MAX_WORD_UNIT; i++) {
				if (!(wren & 0x01)) {
					read_efuse_byte(priv, efuse_addr, rtemp8);
					efuse_addr++;
					efuse_utilized++;
					efuse_word[i][offset] =
							 (*rtemp8 & 0xff);

					if (efuse_addr >= efuse_len)
						break;

					read_efuse_byte(priv, efuse_addr, rtemp8);
					efuse_addr++;
					efuse_utilized++;
					efuse_word[i][offset] |=
					    (((u16)*rtemp8 << 8) & 0xff00);

					if (efuse_addr >= efuse_len)
						break;
				}

				wren >>= 1;
			}
		}

		read_efuse_byte(priv, efuse_addr, rtemp8);
		if (*rtemp8 != 0xFF && (efuse_addr < efuse_len)) {
			efuse_utilized++;
			efuse_addr++;
		}
	}

	for (i = 0; i < efuse_max_section; i++) {
		for (j = 0; j < EFUSE_MAX_WORD_UNIT; j++) {
			efuse_tbl[(i * 8) + (j * 2)] =
			    (efuse_word[j][i] & 0xff);
			efuse_tbl[(i * 8) + ((j * 2) + 1)] =
			    ((efuse_word[j][i] >> 8) & 0xff);
		}
	}

	for (i = 0; i < _size_byte; i++)
		pbuf[i] = efuse_tbl[_offset + i];
done:
	for (i = 0; i < EFUSE_MAX_WORD_UNIT; i++)
		kfree(efuse_word[i]);
	kfree(efuse_word);
out:
	kfree(efuse_tbl);
}



static void efuse_read_all_map(struct rtl8xxxu_priv *priv, u8 *efuse)
{
	efuse_power_switch(priv, false, true);
	read_efuse(priv, 0, HWSET_MAX_SIZE, efuse);
	efuse_power_switch(priv, false, false);
}

void rtl_efuse_shadow_map_update(struct rtl8xxxu_priv *priv)
{
	efuse_read_all_map(priv, &rtlefuse.efuse_map[EFUSE_INIT_MAP][0]);
	memcpy(&rtlefuse.efuse_map[EFUSE_MODIFY_MAP][0], &rtlefuse.efuse_map[EFUSE_INIT_MAP][0],
			HWSET_MAX_SIZE);
}


static void _rtl88eu_read_adapter_info(struct rtl8xxxu_priv *priv, int epromtype)
{
	u16 i, usvalue;
	u8 hwinfo[HWSET_MAX_SIZE] = {0};
	u16 eeprom_id;

	if (epromtype == EEPROM_BOOT_EFUSE) {
		rtl_efuse_shadow_map_update(priv);
		memcpy((void *)hwinfo,
		       (void *)&rtlefuse.efuse_map[EFUSE_INIT_MAP][0],
		       HWSET_MAX_SIZE);
	} else if (epromtype == EEPROM_93C46) {
		RT_TRACE(priv, COMP_ERR, DBG_EMERG,
			 "RTL819X Not boot from eeprom, check it !!");
		return;
	} else {
		RT_TRACE(priv, COMP_ERR, DBG_EMERG,
			 "boot from neither eeprom nor efuse, check it !!");
		return;
	}
	
	eeprom_id = le16_to_cpu(*((__le16 *)hwinfo));
	if (eeprom_id != 0x8129) {
		RT_TRACE(priv, COMP_ERR, DBG_WARNING,
			 "EEPROM ID(%#x) is invalid!!\n", eeprom_id);
	} else {
		;
	}


	for (i = 0; i < 6; i += 2) {
		usvalue = *(u16 *)&hwinfo[EEPROM_MAC_ADDR + i];
		*((u16 *) (&rtlefuse.dev_addr[i])) = usvalue;
	}

	ether_addr_copy(priv->mac_addr, rtlefuse.dev_addr);

	/* Hal_ReadPowerSavingMode88E(hw, eeprom->efuse_eeprom_data); */
	_rtl88eu_read_txpower_info(priv, hwinfo);

	if (!rtlefuse.autoload_failflag) {
		rtlefuse.eeprom_version = hwinfo[EEPROM_VERSION_88E];
		if (rtlefuse.eeprom_version == 0xFF)
			rtlefuse.eeprom_version = 0;
	} else {
		rtlefuse.eeprom_version = 1;
	}
	
	rtlefuse.txpwr_fromeprom = true; /* TODO */
	RT_TRACE(priv, COMP_ERR, DBG_WARNING,
		 "eeprom_version = %d\n", rtlefuse.eeprom_version);

	rtlefuse.eeprom_channelplan = hwinfo[EEPROM_CHANNELPLAN];
	rtlefuse.channel_plan = rtlefuse.eeprom_channelplan;
	
	if (!rtlefuse.autoload_failflag) {
		rtlefuse.crystalcap = hwinfo[EEPROM_XTAL_88E];
		if (rtlefuse.crystalcap == 0xFF)
			rtlefuse.crystalcap = EEPROM_DEFAULT_CRYSTALCAP_88E;
	} else {
		rtlefuse.crystalcap = EEPROM_DEFAULT_CRYSTALCAP_88E;
	}
	if (!rtlefuse.autoload_failflag) {
		rtlefuse.eeprom_oemid = hwinfo[EEPROM_CUSTOMERID_88E];
	} else {
		rtlefuse.eeprom_oemid = 0;
	}
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

	if (!rtlefuse.autoload_failflag)
		rtlefuse.board_type = (hwinfo[EEPROM_RF_BOARD_OPTION_88E]
					& 0xE0) >> 5;
	else
		rtlefuse.board_type = 0;

	if (!rtlefuse.autoload_failflag)
		rtlefuse.eeprom_thermalmeter =
			hwinfo[EEPROM_THERMAL_METER_88E];
	else
		rtlefuse.eeprom_thermalmeter =
			EEPROM_DEFAULT_THERMALMETER;

	if (rtlefuse.eeprom_thermalmeter == 0xff ||
	    rtlefuse.autoload_failflag) {
		rtlefuse.apk_thermalmeterignore = true;
		rtlefuse.eeprom_thermalmeter = EEPROM_DEFAULT_THERMALMETER;
	}
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

}

#if 0
static void _rtl88eu_config_normal_chip_out_ep(struct rtl8xxxu_priv *priv)
{
	switch (rtlusb->out_ep_nums) {
	case	3:
		rtlusb->out_queue_sel = TX_SELE_HQ | TX_SELE_LQ | TX_SELE_NQ;
		break;
	case	2:
		rtlusb->out_queue_sel = TX_SELE_HQ | TX_SELE_NQ;
		break;
	case	1:
		rtlusb->out_queue_sel = TX_SELE_HQ;
		break;
	default:
		break;
	}
	RT_TRACE(priv, COMP_ERR, DBG_WARNING,
		 "%s out_ep_queue_sel(0x%02x), out_ep_number(%d)\n",
		 __func__, rtlusb->out_queue_sel, rtlusb->out_ep_nums);
}

static void _rtl88eu_one_out_ep_mapping(struct rtl8xxxu_priv *priv,
					struct rtl_ep_map *ep_map)
{
	ep_map->ep_mapping[RTL_TXQ_BE] = 2;
	ep_map->ep_mapping[RTL_TXQ_BK] = 2;
	ep_map->ep_mapping[RTL_TXQ_VI] = 2;
	ep_map->ep_mapping[RTL_TXQ_VO] = 2;
	ep_map->ep_mapping[RTL_TXQ_MGT] = 2;
	ep_map->ep_mapping[RTL_TXQ_BCN] = 2;
	ep_map->ep_mapping[RTL_TXQ_HI] = 2;
}

static void _rtl88eu_two_out_ep_mapping(struct rtl8xxxu_priv *priv,
					struct rtl_ep_map *ep_map)
{
	if (rtlusb->wmm_enable) { /* for WMM */
		RT_TRACE(priv, COMP_INIT, DBG_DMESG,
			 "USB Chip-B & WMM Setting.....\n");
		ep_map->ep_mapping[RTL_TXQ_BE] = 3;
		ep_map->ep_mapping[RTL_TXQ_BK] = 2;
		ep_map->ep_mapping[RTL_TXQ_VI] = 2;
		ep_map->ep_mapping[RTL_TXQ_VO] = 3;
		ep_map->ep_mapping[RTL_TXQ_MGT] = 2;
		ep_map->ep_mapping[RTL_TXQ_BCN] = 2;
		ep_map->ep_mapping[RTL_TXQ_HI] = 2;
	} else { /* typical setting */
		RT_TRACE(priv, COMP_INIT, DBG_DMESG,
			 "USB typical Setting.....\n");
		ep_map->ep_mapping[RTL_TXQ_BE] = 3;
		ep_map->ep_mapping[RTL_TXQ_BK] = 3;
		ep_map->ep_mapping[RTL_TXQ_VI] = 2;
		ep_map->ep_mapping[RTL_TXQ_VO] = 2;
		ep_map->ep_mapping[RTL_TXQ_MGT] = 2;
		ep_map->ep_mapping[RTL_TXQ_BCN] = 2;
		ep_map->ep_mapping[RTL_TXQ_HI] = 2;
	}
}

static void _rtl88eu_three_out_ep_mapping(struct rtl8xxxu_priv *priv,
					  struct rtl_ep_map *ep_map)
{
	if (rtlusb->wmm_enable) { /* for WMM */
		RT_TRACE(priv, COMP_INIT, DBG_DMESG,
			 "USB 3EP Setting for WMM.....\n");
		ep_map->ep_mapping[RTL_TXQ_BE] = 5;
		ep_map->ep_mapping[RTL_TXQ_BK] = 3;
		ep_map->ep_mapping[RTL_TXQ_VI] = 3;
		ep_map->ep_mapping[RTL_TXQ_VO] = 2;
		ep_map->ep_mapping[RTL_TXQ_MGT] = 2;
		ep_map->ep_mapping[RTL_TXQ_BCN] = 2;
		ep_map->ep_mapping[RTL_TXQ_HI] = 2;
	} else { /* typical setting */
		RT_TRACE(priv, COMP_INIT, DBG_DMESG,
			 "USB 3EP Setting for typical.....\n");
		ep_map->ep_mapping[RTL_TXQ_BE] = 5;
		ep_map->ep_mapping[RTL_TXQ_BK] = 5;
		ep_map->ep_mapping[RTL_TXQ_VI] = 3;
		ep_map->ep_mapping[RTL_TXQ_VO] = 2;
		ep_map->ep_mapping[RTL_TXQ_MGT] = 2;
		ep_map->ep_mapping[RTL_TXQ_BCN] = 2;
		ep_map->ep_mapping[RTL_TXQ_HI] = 2;
	}
}

static int _rtl88eu_out_ep_mapping(struct rtl8xxxu_priv *priv)
{
	int err = 0;

	switch (rtlusb->out_ep_nums) {
	case 2:
		_rtl88eu_two_out_ep_mapping(hw, ep_map);
		break;
	case 3:
		/* TODO */
		_rtl88eu_three_out_ep_mapping(hw, ep_map);
		err = -EINVAL;
		break;
	case 1:
		_rtl88eu_one_out_ep_mapping(hw, ep_map);
		break;
	default:
		err = -EINVAL;
		break;
	}
	return err;
}

int rtl88eu_endpoint_mapping(struct rtl8xxxu_priv *priv)
{
	bool result = false;

	_rtl88eu_config_normal_chip_out_ep(hw);
	
	/* Normal chip with one IN and one OUT doesn't have interrupt IN EP. */
	if (1 == rtlusb->out_ep_nums) {
		if (1 != rtlusb->in_ep_nums)
			return result;
	}
	/* All config other than above support one Bulk IN and one
	 * Interrupt IN. */
	result = _rtl88eu_out_ep_mapping(hw);

	return result;
}
#endif

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
			 GET_PWR_CFG_BASE(cfg_cmd), GET_PWR_CFG_CMD(cfg_cmd),
			 GET_PWR_CFG_MASK(cfg_cmd), GET_PWR_CFG_VALUE(cfg_cmd));

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

static u32 _rtl88eu_init_power_on(struct rtl8xxxu_priv *priv)
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
	/* TODO */
#if 0
	haldata->bMacPwrCtrlOn = true;
#endif

	return true;
}

void rtl88eu_set_qos(struct rtl8xxxu_priv *priv, int aci)
{

	/* 임시 */
	rtl88e_dm_init_edca_turbo(priv);
	switch (aci) {
	case AC1_BK:
		rtl8xxxu_write32(priv, REG_EDCA_BK_PARAM, 0xa44f);
		break;
	case AC0_BE:
		break;
	case AC2_VI:
		rtl8xxxu_write32(priv, REG_EDCA_VI_PARAM, 0x005ea324);
		break;
	case AC3_VO:
		rtl8xxxu_write32(priv, REG_EDCA_VO_PARAM, 0x002fa226);
		break;
	default:
		//RT_ASSERT(false, "invalid aci: %d !\n", aci);
		break;
	}
}

void rtl88eu_enable_interrupt(struct rtl8xxxu_priv *priv)
{
}

void rtl88eu_disable_interrupt(struct rtl8xxxu_priv *priv)
{
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

static void _rtl88eu_init_page_boundary(struct rtl8xxxu_priv *priv)
{
	u16 rxff_bndy = 0x2400 - 1;

	rtl8xxxu_write16(priv, (REG_TRXFF_BNDY + 2), rxff_bndy);
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

#if 0
static void _rtl88eu_init_normal_chip_one_out_ep_priority(
							struct rtl8xxxu_priv *priv)
{
	u16 value = 0;

	switch (rtlusb->out_queue_sel) {
	case TX_SELE_HQ:
		value = QUEUE_HIGH;
		break;
	case TX_SELE_LQ:
		value = QUEUE_LOW;
		break;
	case TX_SELE_NQ:
		value = QUEUE_NORMAL;
		break;
	default:
		break;
	}
	_rtl88eu_init_normal_chip_reg_priority(priv, value, value, value, value,
					       value, value);
}
#endif 

static void _rtl88eu_init_normal_chip_two_out_ep_priority(
							struct rtl8xxxu_priv *priv)
{
	u16 be_q, bk_q, vi_q, vo_q, mgt_q, hi_q;
#if 0
	u16 value_hi = 0;
	u16 value_low = 0;
#else
	u16 value_hi = 0;
	u16 value_low = 0;

#endif
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

#if 0
static void _rtl88eu_init_normal_chip_three_out_ep_priority(
							struct rtl8xxxu_priv *priv)
{
	u16 be_q, bk_q, vi_q, vo_q, mgt_q, hi_q;

	if (!rtlusb->wmm_enable) {/* typical setting */
		be_q = QUEUE_LOW;
		bk_q = QUEUE_LOW;
		vi_q = QUEUE_NORMAL;
		vo_q = QUEUE_HIGH;
		mgt_q = QUEUE_HIGH;
		hi_q = QUEUE_HIGH;
	} else {/* for WMM */
		be_q = QUEUE_LOW;
		bk_q = QUEUE_NORMAL;
		vi_q = QUEUE_NORMAL;
		vo_q = QUEUE_HIGH;
		mgt_q = QUEUE_HIGH;
		hi_q = QUEUE_HIGH;
	}
	_rtl88eu_init_normal_chip_reg_priority(priv, be_q, bk_q, vi_q, vo_q,
					       mgt_q, hi_q);
}
#endif

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
	_rtl88eu_init_normal_chip_two_out_ep_priority(priv);
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

#if 0
void rtl88eu_set_check_bssid(struct rtl8xxxu_priv *priv, bool check_bssid)
{
	u32 reg_rcr;

	rtlpriv->cfg->ops->get_hw_reg(hw, HW_VAR_RCR, (u8 *)(&reg_rcr));

	if (check_bssid) {
		u8 tmp;
		reg_rcr |= (RCR_CBSSID_DATA | RCR_CBSSID_BCN);
		tmp = BIT(4);
		rtlpriv->cfg->ops->set_hw_reg(hw, HW_VAR_RCR,
					      (u8 *) (&reg_rcr));
		_rtl88eu_set_bcn_ctrl_reg(hw, 0, tmp);
	} else {
		u8 tmp;
		reg_rcr &= ~(RCR_CBSSID_DATA | RCR_CBSSID_BCN);
		tmp = BIT(4);
		reg_rcr &= (~(RCR_CBSSID_DATA | RCR_CBSSID_BCN));
		rtlpriv->cfg->ops->set_hw_reg(hw,
					      HW_VAR_RCR, (u8 *) (&reg_rcr));
		_rtl88eu_set_bcn_ctrl_reg(hw, tmp, 0);
	}
}

int rtl88eu_set_network_type(struct rtl8xxxu_priv *priv, enum nl80211_iftype type)
{

	if (_rtl88eu_set_media_status(hw, type))
		return -EOPNOTSUPP;

	if (rtlpriv->mac80211.link_state == MAC80211_LINKED) {
		if (type != NL80211_IFTYPE_AP)
			rtlpriv->cfg->ops->set_chk_bssid(hw, true);
	} else {
		rtlpriv->cfg->ops->set_chk_bssid(hw, false);
	}

	return 0;
}

static void _InitNetworkType(struct rtl8xxxu_priv *priv)
{
	u32 value32;

	value32 = rtl8xxxu_read32(priv, REG_CR);
	/*  TODO: use the other function to set network type */
	value32 = (value32 & ~MASK_NETTYPE) | _NETTYPE(NT_LINK_AP);

	rtl8xxxu_write32(priv, REG_CR, value32);
}
#endif

static void _rtl88eu_init_transfer_page_size(struct rtl8xxxu_priv *priv)
{
	u8 value8;

	value8 = _PSRX(PBP_128) | _PSTX(PBP_128);
	rtl8xxxu_write8(priv, REG_PBP, value8);
}

static void _rtl88eu_init_driver_info_size(struct rtl8xxxu_priv *priv,
					   u8 drv_info_size)
{

	rtl8xxxu_write8(priv, REG_RX_DRVINFO_SZ, drv_info_size);
}

static void _rtl88eu_init_wmac_setting(struct rtl8xxxu_priv *priv)
{
	u32 val32;

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

#if 0
static void _InitRDGSetting(struct rtl8xxxu_priv *priv)
{
	rtl8xxxu_write8(priv, REG_RD_CTRL, 0xFF);
	rtl8xxxu_write16(priv, REG_RD_NAV_NXT, 0x200);
	rtl8xxxu_write8(priv, REG_RD_RESP_PKT_TH, 0x05);
}
#endif

static void _rtl88eu_init_retry_function(struct rtl8xxxu_priv *priv)
{
	u8 value8;

	value8 = rtl8xxxu_read8(priv, REG_FWHW_TXQ_CTRL);
	value8 |= EN_AMPDU_RTY_NEW;
	rtl8xxxu_write8(priv, REG_FWHW_TXQ_CTRL, value8);

	/* Set ACK timeout */
	rtl8xxxu_write8(priv, REG_ACKTO, 0x40);
}

#if 1
static void _rtl88eu_agg_setting_tx_update(struct rtl8xxxu_priv *priv)
{
	u32 value32;
#if 0
	if (rtlusb->wmm_enable)
		rtlusb->usb_tx_agg_mode = false;

	if (rtlusb->usb_tx_agg_mode) {
#else
		if (1) {
#endif
		value32 = rtl8xxxu_read32(priv, REG_TDECTRL);
		value32 = value32 & ~(BLK_DESC_NUM_MASK << BLK_DESC_NUM_SHIFT);
		value32 |= ((6 & BLK_DESC_NUM_MASK) << BLK_DESC_NUM_SHIFT);

		rtl8xxxu_write32(priv, REG_TDECTRL, value32);
	}
}

static void _rtl88eu_agg_setting_rx_update( struct rtl8xxxu_priv *priv)
{
	u8 value_dma;
	u8 value_usb;

	value_dma = rtl8xxxu_read8(priv, REG_TRXDMA_CTRL);
	value_usb = rtl8xxxu_read8(priv, REG_USB_SPECIAL_OPTION);

	/* TODO */
#if 0
	rtlusb->usb_rx_agg_mode = USB_RX_AGG_DMA;

	switch (rtlusb->usb_rx_agg_mode) {
	case USB_RX_AGG_DMA:
		value_dma |= RXDMA_AGG_EN;
		value_usb &= ~USB_AGG_EN;
		break;
	case USB_RX_AGG_USB:
		value_dma &= ~RXDMA_AGG_EN;
		value_usb |= USB_AGG_EN;
		break;
	case USB_RX_AGG_DMA_USB:
		value_dma |= RXDMA_AGG_EN;
		value_usb |= USB_AGG_EN;
		break;
	case USB_RX_AGG_DISABLE:
	default:
		value_dma &= ~RXDMA_AGG_EN;
		value_usb &= ~USB_AGG_EN;
		break;
	}

	rtl8xxxu_write8(priv, REG_TRXDMA_CTRL, value_dma);
	rtl8xxxu_write8(priv, REG_USB_SPECIAL_OPTION, value_usb);

	switch (rtlusb->usb_rx_agg_mode) {
	case USB_RX_AGG_DMA:
		rtl8xxxu_write8(priv, REG_RXDMA_AGG_PG_TH, 48);
		rtl8xxxu_write8(priv, REG_RXDMA_AGG_PG_TH+1, 4);
		break;
	case USB_RX_AGG_USB:
		rtl8xxxu_write8(priv, REG_USB_AGG_TH, 8);
		rtl8xxxu_write8(priv, REG_USB_AGG_TO, 6);
		break;
	case USB_RX_AGG_DMA_USB:
		rtl8xxxu_write8(priv, REG_RXDMA_AGG_PG_TH, 48);
		rtl8xxxu_write8(priv, REG_RXDMA_AGG_PG_TH+1, (4 & 0x1F));
		rtl8xxxu_write8(priv, REG_USB_AGG_TH, 8);
		rtl8xxxu_write8(priv, REG_USB_AGG_TO, 6);
		break;
	case USB_RX_AGG_DISABLE:
	default:
		break;
	}
#else
	value_dma |= RXDMA_AGG_EN;
	value_usb &= ~USB_AGG_EN;
	rtl8xxxu_write8(priv, REG_RXDMA_AGG_PG_TH, 48);
	rtl8xxxu_write8(priv, REG_RXDMA_AGG_PG_TH+1, 4);

#endif

}
#endif

#if 1
static void _rtl88eu_init_agg_setting(struct rtl8xxxu_priv *priv)
{

	/* TODO */
#if 1
	_rtl88eu_agg_setting_tx_update(priv);
	_rtl88eu_agg_setting_rx_update(priv);
	/* haldata->UsbRxHighSpeedMode = false; */
#endif
}
#endif

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

static void _rtl88eu_bb_turn_on_block(struct rtl8xxxu_priv *priv)
{
	rtl_set_bbreg(priv, RFPGA0_RFMOD, BCCKEN, 0x1);
	rtl_set_bbreg(priv, RFPGA0_RFMOD, BOFDMEN, 0x1);
}

static void _rtl88eu_init_ant_selection(struct rtl8xxxu_priv *priv)
{

#if 0
	if (rtlefuse.antenna_div_cfg == 0)
		return;
#endif

	rtl8xxxu_write32(priv, REG_LEDCFG0,
			rtl8xxxu_read32(priv, REG_LEDCFG0)|BIT(23));
	rtl_set_bbreg(priv, RFPGA0_XAB_RFPARAMETER, BIT(13), 0x01);
}

#if 0
enum rf_pwrstate rtl88eu_rf_on_off_detect(struct rtl8xxxu_priv *priv)
{
	u8 val8;
	enum rf_pwrstate rfpowerstate = ERFOFF;

	if (ppsc->pwrdown_mode) {
		val8 = rtl8xxxu_read8(priv, REG_HSISR);
		RT_TRACE(priv, COMP_ERR, DBG_WARNING,
			 "pwrdown, 0x5c(BIT(7))=%02x\n", val8);
		rfpowerstate = (val8 & BIT(7)) ? ERFOFF : ERFON;
	} else {
		rtl8xxxu_write8(priv, REG_MAC_PINMUX_CFG,
			       rtl8xxxu_read8(priv, REG_MAC_PINMUX_CFG) &
					     ~(BIT(3)));
		val8 = rtl8xxxu_read8(priv, REG_GPIO_IO_SEL);
		RT_TRACE(priv, COMP_ERR, DBG_WARNING,"GPIO_IN=%02x\n", val8);
		rfpowerstate = (val8 & BIT(3)) ? ERFON: ERFOFF;
	}
	return rfpowerstate;
}
#endif

static int _rtl88eu_init_mac(struct rtl8xxxu_priv *priv)
{
	int err = 0;
	u32 boundary = 0;
	
	err = _rtl88eu_init_power_on(priv);
	if (err == false) {
		RT_TRACE(priv, COMP_ERR, DBG_EMERG,
			 "Failed to init power on!\n");
		return err;
	}
#if 1
	boundary = TX_PAGE_BOUNDARY_88E;
#else
	boundary = WMM_NORMAL_TX_PAGE_BOUNDARY_88E;
#endif
	if (false == rtl88eu_init_llt_table(priv, boundary)) {
	
		RT_TRACE(priv, COMP_ERR, DBG_EMERG,
			 "Failed to init LLT table\n");
		return -EINVAL;
	}

	_rtl88eu_init_queue_reserved_page(priv);
	_rtl88eu_init_txbuffer_boundary(priv, 0);
	_rtl88eu_init_page_boundary(priv);
	_rtl88eu_init_transfer_page_size(priv);
	_rtl88eu_init_queue_priority(priv);
	_rtl88eu_init_driver_info_size(priv, 4);
	_rtl88eu_init_interrupt(priv);
	_rtl88eu_init_wmac_setting(priv);
	_rtl88eu_init_adaptive_ctrl(priv);
	_rtl88eu_init_edca(priv);
	_rtl88eu_init_retry_function(priv);
	/* TODO */
#if 0
	_rtl88eu_init_agg_setting(priv);
#endif
	//여기 꼭 enable 해야함
	//rtl88e_phy_set_bw_mode(priv, NL80211_CHAN_HT20);
	_rtl88eu_init_beacon_parameters(priv);
	_rtl88eu_init_txbuffer_boundary(priv, boundary);
	return 0;
}

static void rtl_fw_do_work(const struct firmware *firmware, void *context,
			   bool is_wow)
{
	struct ieee80211_hw *hw = context;
	struct rtl8xxxu_priv *priv = hw->priv;
	int err;

	RT_TRACE(priv, COMP_ERR, DBG_LOUD,
		 "Firmware callback routine entered!\n");
	complete(&priv->firmware_loading_complete);
	if (!firmware) {
	}
#if 0
	if (firmware->size > rtlpriv->max_fw_size) {
		RT_TRACE(priv, COMP_ERR, DBG_EMERG,
			 "Firmware is too big!\n");
		release_firmware(firmware);
		return;
	}
#endif
	memcpy(priv->fw_data, firmware->data,
			firmware->size);
	priv->fw_size = firmware->size;
	release_firmware(firmware);
}

void rtl_cam_reset_all_entry(struct rtl8xxxu_priv *priv)
{
	u32 ul_command;

	ul_command = BIT(31) | BIT(30);
	rtl8xxxu_write32(priv, REG_CAMCMD, ul_command);
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

void rtl88eu_read_eeprom_info(struct rtl8xxxu_priv *priv)
{
	u8 eeValue;
	int epromtype;

	/* check system boot selection */
	eeValue = rtl8xxxu_read8(priv, REG_9346CR);
	
	if (eeValue & BIT(4)) {
		RT_TRACE(priv, COMP_ERR, DBG_DMESG, "Boot from EEPROM\n");
		epromtype = EEPROM_93C46;
	} else {
		RT_TRACE(priv, COMP_ERR, DBG_DMESG, "Boot from EFUSE\n");
		epromtype = EEPROM_BOOT_EFUSE;
	}

	_rtl88eu_read_adapter_info(priv, epromtype);
}

#if 0
void rtl88eu_get_hw_reg(struct rtl8xxxu_priv *priv, u8 variable, u8 *val)
{
	switch (variable) {
	case HW_VAR_RCR:
		*((u32 *) (val)) = rtl8xxxu_read32(priv, REG_RCR);
		break;
	case HW_VAR_RF_STATE:
		*((enum rf_pwrstate *)(val)) = ppsc->rfpwr_state;
		break;
	case HW_VAR_FWLPS_RF_ON:{
		enum rf_pwrstate rfstate;
		u32 val_rcr;

		rtlpriv->cfg->ops->get_hw_reg(hw, HW_VAR_RF_STATE,
					      (u8 *)(&rfstate));
		if (rfstate == ERFOFF) {
			*((bool *)(val)) = true;
		} else {
			val_rcr = rtl8xxxu_read32(priv, REG_RCR);
			val_rcr &= 0x00070000;
			if (val_rcr)
				*((bool *)(val)) = false;
			else
				*((bool *)(val)) = true;
		}
		break; }
	case HW_VAR_FW_PSMODE_STATUS:
		*((bool *)(val)) = ppsc->fw_current_inpsmode;
		break;
	case HW_VAR_CORRECT_TSF:{
		u64 tsf;
		u32 *ptsf_low = (u32 *)&tsf;
		u32 *ptsf_high = ((u32 *)&tsf) + 1;

		*ptsf_high = rtl8xxxu_read32(priv, (REG_TSFTR + 4));
		*ptsf_low = rtl8xxxu_read32(priv, REG_TSFTR);

		*((u64 *)(val)) = tsf;
		break; }
	default:
		RT_TRACE(priv, COMP_ERR, DBG_EMERG,
			 "switch case not process %x\n", variable);
		break;
	}
}

void rtl88eu_set_hw_reg(struct rtl8xxxu_priv *priv, u8 variable, u8 *val)
{
	u8 idx;

	switch (variable) {
	case HW_VAR_ETHER_ADDR:
		for (idx = 0; idx < ETH_ALEN; idx++) {
			rtl8xxxu_write8(priv, (REG_MACID + idx),
				       val[idx]);
		}
		break;
	case HW_VAR_BASIC_RATE:{
		u16 b_rate_cfg = ((u16 *)val)[0];
		u8 rate_index = 0;
		b_rate_cfg = b_rate_cfg & 0x15f;
		b_rate_cfg |= 0x01;
		rtl8xxxu_write8(priv, REG_RRSR, b_rate_cfg & 0xff);
		rtl8xxxu_write8(priv, REG_RRSR + 1,
			       (b_rate_cfg >> 8) & 0xff);
		while (b_rate_cfg > 0x1) {
			b_rate_cfg = (b_rate_cfg >> 1);
			rate_index++;
		}
		rtl8xxxu_write8(priv, REG_INIRTS_RATE_SEL,
			       rate_index);
		break;
		}
	case HW_VAR_BSSID:
		for (idx = 0; idx < ETH_ALEN; idx++) {
			rtl8xxxu_write8(priv, (REG_BSSID + idx),
				       val[idx]);
		}
		break;
	case HW_VAR_SIFS:
		rtl8xxxu_write8(priv, REG_SIFS_CTX + 1, val[0]);
		rtl8xxxu_write8(priv, REG_SIFS_TRX + 1, val[1]);

		rtl8xxxu_write8(priv, REG_SPEC_SIFS + 1, val[0]);
		rtl8xxxu_write8(priv, REG_MAC_SPEC_SIFS + 1, val[0]);

		if (!mac->ht_enable)
			rtl8xxxu_write16(priv, REG_RESP_SIFS_OFDM,
				       0x0e0e);
		else
			rtl8xxxu_write16(priv, REG_RESP_SIFS_OFDM,
				       *((u16 *)val));
		break;
	case HW_VAR_SLOT_TIME:{
		u8 e_aci;

		RT_TRACE(priv, COMP_MLME, DBG_LOUD,
			 "HW_VAR_SLOT_TIME %x\n", val[0]);

		rtl8xxxu_write8(priv, REG_SLOT, val[0]);

		for (e_aci = 0; e_aci < AC_MAX; e_aci++) {
			rtlpriv->cfg->ops->set_hw_reg(hw, HW_VAR_AC_PARAM,
						      &e_aci);
		}
		break;
		}
	case HW_VAR_ACK_PREAMBLE:{
		u8 reg_tmp;
		u8 short_preamble = (bool)*val;
		reg_tmp = rtl8xxxu_read8(priv, REG_TRXPTCL_CTL+2);
		if (short_preamble) {
			reg_tmp |= 0x02;
			rtl8xxxu_write8(priv, REG_TRXPTCL_CTL +
				       2, reg_tmp);
		} else {
			reg_tmp |= 0xFD;
			rtl8xxxu_write8(priv, REG_TRXPTCL_CTL +
				       2, reg_tmp);
		}
		break; }
	case HW_VAR_WPA_CONFIG:
		rtl8xxxu_write8(priv, REG_SECCFG, *val);
		break;
	case HW_VAR_AMPDU_MIN_SPACE:{
		u8 min_spacing_to_set;
		u8 sec_min_space;

		min_spacing_to_set = *val;
		if (min_spacing_to_set <= 7) {
			sec_min_space = 0;

			if (min_spacing_to_set < sec_min_space)
				min_spacing_to_set = sec_min_space;

			mac->min_space_cfg = ((mac->min_space_cfg &
					       0xf8) |
					      min_spacing_to_set);

			*val = min_spacing_to_set;

			RT_TRACE(priv, COMP_MLME, DBG_LOUD,
				 "Set HW_VAR_AMPDU_MIN_SPACE: %#x\n",
				  mac->min_space_cfg);

			rtl8xxxu_write8(priv, REG_AMPDU_MIN_SPACE,
				       mac->min_space_cfg);
		}
		break; }
	case HW_VAR_SHORTGI_DENSITY:{
		u8 density_to_set;

		density_to_set = *val;
		mac->min_space_cfg |= (density_to_set << 3);

		RT_TRACE(priv, COMP_MLME, DBG_LOUD,
			 "Set HW_VAR_SHORTGI_DENSITY: %#x\n",
			  mac->min_space_cfg);

		rtl8xxxu_write8(priv, REG_AMPDU_MIN_SPACE,
			       mac->min_space_cfg);
		break;
		}
	case HW_VAR_AMPDU_FACTOR:{
		u8 regtoset_normal[4] = { 0x41, 0xa8, 0x72, 0xb9 };
		u8 factor_toset;
		u8 *p_regtoset = NULL;
		u8 index = 0;

		p_regtoset = regtoset_normal;

		factor_toset = *val;
		if (factor_toset <= 3) {
			factor_toset = (1 << (factor_toset + 2));
			if (factor_toset > 0xf)
				factor_toset = 0xf;

			for (index = 0; index < 4; index++) {
				if ((p_regtoset[index] & 0xf0) >
				    (factor_toset << 4))
					p_regtoset[index] =
					    (p_regtoset[index] & 0x0f) |
					    (factor_toset << 4);

				if ((p_regtoset[index] & 0x0f) >
				    factor_toset)
					p_regtoset[index] =
					    (p_regtoset[index] & 0xf0) |
					    (factor_toset);

				rtl8xxxu_write8(priv,
					       (REG_AGGLEN_LMT + index),
					       p_regtoset[index]);

			}

			RT_TRACE(priv, COMP_MLME, DBG_LOUD,
				 "Set HW_VAR_AMPDU_FACTOR: %#x\n",
				  factor_toset);
		}
		break; }
	case HW_VAR_AC_PARAM:{
		u8 e_aci = *val;
		rtl88e_dm_init_edca_turbo(hw);

		if (rtlusb->acm_method != EACMWAY2_SW)
			rtlpriv->cfg->ops->set_hw_reg(hw,
						      HW_VAR_ACM_CTRL,
						      &e_aci);
		break; }
	case HW_VAR_ACM_CTRL:{
		u8 e_aci = *val;
		union aci_aifsn *p_aci_aifsn =
		    (union aci_aifsn *)(&(mac->ac[0].aifs));
		u8 acm = p_aci_aifsn->f.acm;
		u8 acm_ctrl = rtl8xxxu_read8(priv, REG_ACMHWCTRL);

		acm_ctrl = acm_ctrl |
			   ((rtlusb->acm_method == 2) ? 0x0 : 0x1);

		if (acm) {
			switch (e_aci) {
			case AC0_BE:
				acm_ctrl |= ACMHW_BEQEN;
				break;
			case AC2_VI:
				acm_ctrl |= ACMHW_VIQEN;
				break;
			case AC3_VO:
				acm_ctrl |= ACMHW_VOQEN;
				break;
			default:
				RT_TRACE(priv, COMP_ERR, DBG_WARNING,
					 "HW_VAR_ACM_CTRL acm set failed: eACI is %d\n",
					 acm);
				break;
			}
		} else {
			switch (e_aci) {
			case AC0_BE:
				acm_ctrl &= (~ACMHW_BEQEN);
				break;
			case AC2_VI:
				acm_ctrl &= (~ACMHW_VIQEN);
				break;
			case AC3_VO:
				acm_ctrl &= (~ACMHW_VOQEN);
				break;
			default:
				RT_TRACE(priv, COMP_ERR, DBG_EMERG,
					 "switch case not process\n");
				break;
			}
		}

		RT_TRACE(priv, COMP_QOS, DBG_TRACE,
			 "SetHwReg8190usb(): [HW_VAR_ACM_CTRL] Write 0x%X\n",
			 acm_ctrl);
		rtl8xxxu_write8(priv, REG_ACMHWCTRL, acm_ctrl);
		break; }
	case HW_VAR_RCR:
		rtl8xxxu_write32(priv, REG_RCR, ((u32 *)(val))[0]);
		mac->rx_conf = ((u32 *)(val))[0];
		break;
	case HW_VAR_RETRY_LIMIT:{
		u8 retry_limit = *val;

		rtl8xxxu_write16(priv, REG_RL,
			       retry_limit << RETRY_LIMIT_SHORT_SHIFT |
			       retry_limit << RETRY_LIMIT_LONG_SHIFT);
		break; }
	case HW_VAR_DUAL_TSF_RST:
		rtl8xxxu_write8(priv, REG_DUAL_TSF_RST, (BIT(0) | BIT(1)));
		break;
	case HW_VAR_EFUSE_BYTES:
		rtlefuse.efuse_usedbytes = *((u16 *)val);
		break;
	case HW_VAR_EFUSE_USAGE:
		rtlefuse.efuse_usedpercentage = *val;
		break;
	case HW_VAR_IO_CMD:
		rtl88e_phy_set_io_cmd(hw, (*(enum io_type *)val));
		break;
	case HW_VAR_SET_RPWM:
		break;
	case HW_VAR_H2C_FW_PWRMODE:
		rtl88eu_set_fw_pwrmode_cmd(hw, *val);
		break;
	case HW_VAR_FW_PSMODE_STATUS:
		ppsc->fw_current_inpsmode = *((bool *)val);
		break;
	case HW_VAR_H2C_FW_JOINBSSRPT:
		rtl88eu_set_fw_joinbss_report_cmd(hw, (*(u8 *)val));
		break;
	case HW_VAR_H2C_FW_P2P_PS_OFFLOAD:
		break;
	case HW_VAR_AID:{
		u16 u2btmp;

		u2btmp = rtl8xxxu_read16(priv, REG_BCN_PSR_RPT);
		u2btmp &= 0xC000;
		rtl8xxxu_write16(priv, REG_BCN_PSR_RPT, (u2btmp |
			       mac->assoc_id));
		break; }
	case HW_VAR_CORRECT_TSF:{
		u8 btype_ibss = *val;

		if (btype_ibss)
			_rtl88eu_stop_tx_beacon(priv);

		_rtl88eu_set_bcn_ctrl_reg(hw, 0, BIT(3));

		rtl8xxxu_write32(priv, REG_TSFTR,
				(u32)(mac->tsf & 0xffffffff));
		rtl8xxxu_write32(priv, REG_TSFTR + 4,
				(u32)((mac->tsf >> 32) & 0xffffffff));

		_rtl88eu_set_bcn_ctrl_reg(hw, BIT(3), 0);

		if (btype_ibss)
			_rtl88eu_resume_tx_beacon(priv);
		break; }
	case HW_VAR_KEEP_ALIVE: {
		u8 array[2];

		array[0] = 0xff;
		array[1] = *((u8 *)val);
		rtl88eu_fill_h2c_cmd(priv, H2C_88E_KEEP_ALIVE_CTRL,
				    2, array);
		break; }
	default:
		RT_TRACE(priv, COMP_ERR, DBG_EMERG,
			 "switch case not process %x\n", variable);
		break;
	}
}

#endif

void rtl88eu_update_interrupt_mask(struct rtl8xxxu_priv *priv,
				   u32 add_msr, u32 rm_msr)
{
}

void rtl88eu_set_beacon_interval(struct rtl8xxxu_priv *priv)
{
#if 0
	u16 bcn_interval = mac->beacon_interval;
#else
	u16 bcn_interval = 100;
#endif

	RT_TRACE(priv, COMP_BEACON, DBG_DMESG, "beacon_interval:%d\n",
		 bcn_interval);
	rtl8xxxu_write16(priv, REG_BCN_INTERVAL, bcn_interval);
}

void rtl88eu_set_beacon_related_registers(struct rtl8xxxu_priv *priv)
{
	u32 value32;
	u32 bcn_ctrl_reg = REG_BCN_CTRL;
	/* reset TSF, enable update TSF, correcting TSF On Beacon */

	/* BCN interval */
#if 0
	rtl8xxxu_write16(priv, REG_BCN_INTERVAL, mac->beacon_interval);
#else
	rtl8xxxu_write16(priv, REG_BCN_INTERVAL, 100);
#endif
	rtl8xxxu_write8(priv, REG_ATIMWND, 0x02);/* 2ms */

	_rtl88eu_init_beacon_parameters(priv);

	rtl8xxxu_write8(priv, REG_SLOT, 0x09);

	value32 = rtl8xxxu_read32(priv, REG_TCR);
	value32 &= ~TSFRST;
	rtl8xxxu_write32(priv,  REG_TCR, value32);

	value32 |= TSFRST;
	rtl8xxxu_write32(priv, REG_TCR, value32);

	/* NOTE: Fix test chip's bug (about contention windows's randomness) */
	rtl8xxxu_write8(priv,  REG_RXTSF_OFFSET_CCK, 0x50);
	rtl8xxxu_write8(priv, REG_RXTSF_OFFSET_OFDM, 0x50);

	_rtl88eu_beacon_function_enable(priv);

	_rtl88eu_resume_tx_beacon(priv);

	rtl8xxxu_write8(priv, bcn_ctrl_reg,
		       rtl8xxxu_read8(priv, bcn_ctrl_reg) | BIT(1));
}

static void rtl88eu_update_hal_rate_mask(struct rtl8xxxu_priv *priv,
					 u32 ramask, int sgi)
{
	struct rtl_sta_info *sta_entry = NULL;
	u32 ratr_bitmap;
	u8 ratr_index;
	u8 rate_mask[5];
#if 0
	switch (wirelessmode) {
	case WIRELESS_MODE_B:
		ratr_index = RATR_INX_WIRELESS_B;
		if (ratr_bitmap & 0x0000000c)
			ratr_bitmap &= 0x0000000d;
		else
			ratr_bitmap &= 0x0000000f;
		break;
	case WIRELESS_MODE_G:
		ratr_index = RATR_INX_WIRELESS_GB;

		if (rssi_level == 1)
			ratr_bitmap &= 0x00000f00;
		else if (rssi_level == 2)
			ratr_bitmap &= 0x00000ff0;
		else
			ratr_bitmap &= 0x00000ff5;
		break;
	case WIRELESS_MODE_N_24G:
	case WIRELESS_MODE_N_5G:
		ratr_index = RATR_INX_WIRELESS_NGB;
		if (rtlphy.rf_type == RF_1T2R ||
		    rtlphy.rf_type == RF_1T1R) {
			if (curtxbw_40mhz) {
				if (rssi_level == 1)
					ratr_bitmap &= 0x000f0000;
				else if (rssi_level == 2)
					ratr_bitmap &= 0x000ff000;
				else
					ratr_bitmap &= 0x000ff015;
			} else {
				if (rssi_level == 1)
					ratr_bitmap &= 0x000f0000;
				else if (rssi_level == 2)
					ratr_bitmap &= 0x000ff000;
				else
					ratr_bitmap &= 0x000ff005;
			}
		}
#if 0
		if ((curtxbw_40mhz && curshortgi_40mhz) ||
		    (!curtxbw_40mhz && curshortgi_20mhz)) {

			if (macid == 0)
				b_shortgi = true;
			else if (macid == 1)
				b_shortgi = false;
		}
#endif
		break;
	default:
		ratr_index = RATR_INX_WIRELESS_NGB;

		if (rtlphy.rf_type == RF_1T2R)
			ratr_bitmap &= 0x000ff0ff;
		else
			ratr_bitmap &= 0x0f0ff0ff;
		break;
	}
	sta_entry->ratr_index = ratr_index;

	RT_TRACE(priv, COMP_RATR, DBG_DMESG,
		 "ratr_bitmap :%x\n", ratr_bitmap);
	*(u32 *)&rate_mask = (ratr_bitmap & 0x0fffffff) |
			     (ratr_index << 28);
	rate_mask[4] = macid | (b_shortgi ? 0x20 : 0x00) | 0x80;
	RT_TRACE(priv, COMP_RATR, DBG_DMESG,
		 "Rate_index:%x, ratr_val:%x, %x:%x:%x:%x:%x\n",
		 ratr_index, ratr_bitmap,
		 rate_mask[0], rate_mask[1],
		 rate_mask[2], rate_mask[3],
		 rate_mask[4]);
#endif
#if 1
	*(u32 *)&rate_mask = ((0xb & 0x00000f00) & 0x0fffffff) |
			     (4 << 28);
	rate_mask[4] = 0 | 0x00 | 0x80;
	rtl88eu_fill_h2c_cmd(priv, H2C_88E_RA_MASK, 5, (u8 *)&ramask);
#endif
	_rtl88eu_set_bcn_ctrl_reg(priv, BIT(3), 0);
}

#if 0
static void rtl88eu_update_hal_rate_table(struct rtl8xxxu_priv *priv,
					  struct ieee80211_sta *sta)
{
	u32 ratr_value;
	u8 ratr_index = 0;
	u8 nmode = mac->ht_enable;
	u8 mimo_ps = IEEE80211_SMPS_OFF;
	u16 shortgi_rate;
	u32 tmp_ratr_value;
	u8 curtxbw_40mhz = mac->bw_40;
	u8 curshortgi_40mhz = (sta->ht_cap.cap & IEEE80211_HT_CAP_SGI_40) ?
			       1 : 0;
	u8 curshortgi_20mhz = (sta->ht_cap.cap & IEEE80211_HT_CAP_SGI_20) ?
			       1 : 0;
	enum wireless_mode wirelessmode = mac->mode;

	if (rtlhal->current_bandtype == BAND_ON_5G)
		ratr_value = sta->supp_rates[1] << 4;
	else
		ratr_value = sta->supp_rates[0];
	if (mac->opmode == NL80211_IFTYPE_ADHOC)
		ratr_value = 0xfff;

	ratr_value |= (sta->ht_cap.mcs.rx_mask[1] << 20 |
			sta->ht_cap.mcs.rx_mask[0] << 12);
	switch (wirelessmode) {
	case WIRELESS_MODE_B:
		if (ratr_value & 0x0000000c)
			ratr_value &= 0x0000000d;
		else
			ratr_value &= 0x0000000f;
		break;
	case WIRELESS_MODE_G:
		ratr_value &= 0x00000FF5;
		break;
	case WIRELESS_MODE_N_24G:
	case WIRELESS_MODE_N_5G:
		nmode = 1;
		if (mimo_ps == IEEE80211_SMPS_STATIC) {
			ratr_value &= 0x0007F005;
		} else {
			u32 ratr_mask;

			if (get_rf_type(rtlphy) == RF_1T2R ||
			    get_rf_type(rtlphy) == RF_1T1R)
				ratr_mask = 0x000ff005;
			else
				ratr_mask = 0x0f0ff005;

			ratr_value &= ratr_mask;
		}
		break;
	default:
		if (rtlphy.rf_type == RF_1T2R)
			ratr_value &= 0x000ff0ff;
		else
			ratr_value &= 0x0f0ff0ff;

		break;
	}

	ratr_value &= 0x0FFFFFFF;

	if (nmode && ((curtxbw_40mhz &&
			 curshortgi_40mhz) || (!curtxbw_40mhz &&
					       curshortgi_20mhz))) {

		ratr_value |= 0x10000000;
		tmp_ratr_value = (ratr_value >> 12);

		for (shortgi_rate = 15; shortgi_rate > 0; shortgi_rate--) {
			if ((1 << shortgi_rate) & tmp_ratr_value)
				break;
		}

		shortgi_rate = (shortgi_rate << 12) | (shortgi_rate << 8) |
		    (shortgi_rate << 4) | (shortgi_rate);
	}

	rtl8xxxu_write32(priv, REG_ARFR0 + ratr_index * 4, ratr_value);

	RT_TRACE(priv, COMP_RATR, DBG_DMESG, "%x\n",
		 rtl8xxxu_read32(priv, REG_ARFR0));
}
#endif

void rtl88eu_update_hal_rate_tbl(struct rtl8xxxu_priv *priv,
				 u32 ramask, int sgi)
{

#if 0
	if (rtldm.useramask)
		rtl88eu_update_hal_rate_mask(hw, sta, rssi_level);
	else
		rtl88eu_update_hal_rate_table(hw, sta);
#else
	rtl88eu_update_hal_rate_mask(priv, ramask, sgi);
#endif
}

//생략 많이 함 다시 구성할것 */
void rtl88eu_update_channel_access_setting(struct rtl8xxxu_priv *priv)
{
	u16 sifs_timer;

	rtl8xxxu_write8(priv, REG_SLOT, 9);
#if 0
	if (!rtlmac.ht_enable)
		sifs_timer = 0x0a0a;
	else
		sifs_timer = 0x0e0e;
#else
	sifs_timer = 0x0a0a;
#endif

	rtl8xxxu_write8(priv, REG_SIFS_CTX + 1, sifs_timer >> 8);
	rtl8xxxu_write8(priv, REG_SIFS_TRX + 1, sifs_timer & 0x000000ff);

	rtl8xxxu_write8(priv, REG_SPEC_SIFS + 1, sifs_timer & 0x000000ff);
	rtl8xxxu_write8(priv, REG_MAC_SPEC_SIFS + 1, sifs_timer & 0x000000ff);
#if 0
	if (!rtlmac.ht_enable)
		rtl8xxxu_write16(priv, REG_RESP_SIFS_OFDM,
				0x0e0e);
	else
		rtl8xxxu_write16(priv, REG_RESP_SIFS_OFDM,
				*((u16 *)val));
#else
	rtl8xxxu_write16(priv, REG_RESP_SIFS_OFDM,
			0x0e0e);
#endif

}
#if 0

bool rtl88eu_gpio_radio_on_off_checking(struct rtl8xxxu_priv *priv, u8 *valid)
{
	enum rf_pwrstate e_rfpowerstate_toset, cur_rfstate;
	u32 u4tmp;
	bool b_actuallyset = false;

	if (rtlpriv->rtlhal.being_init_adapter)
		return false;

	if (ppsc->swrf_processing)
		return false;

	spin_lock(&rtlpriv->locks.rf_ps_lock);
	if (ppsc->rfchange_inprogress) {
		spin_unlock(&rtlpriv->locks.rf_ps_lock);
		return false;
	} else {
		ppsc->rfchange_inprogress = true;
		spin_unlock(&rtlpriv->locks.rf_ps_lock);
	}

	cur_rfstate = ppsc->rfpwr_state;

	u4tmp = rtl8xxxu_read32(priv, REG_GPIO_OUTPUT);
	e_rfpowerstate_toset = (u4tmp & BIT(31)) ? ERFON : ERFOFF;

	if (ppsc->hwradiooff && (e_rfpowerstate_toset == ERFON)) {
		RT_TRACE(priv, COMP_RF, DBG_DMESG,
			 "GPIOChangeRF  - HW Radio ON, RF ON\n");

		e_rfpowerstate_toset = ERFON;
		ppsc->hwradiooff = false;
		b_actuallyset = true;
	} else if ((!ppsc->hwradiooff) &&
		   (e_rfpowerstate_toset == ERFOFF)) {
		RT_TRACE(priv, COMP_RF, DBG_DMESG,
			 "GPIOChangeRF  - HW Radio OFF, RF OFF\n");

		e_rfpowerstate_toset = ERFOFF;
		ppsc->hwradiooff = true;
		b_actuallyset = true;
	}

	if (b_actuallyset) {
		spin_lock(&rtlpriv->locks.rf_ps_lock);
		ppsc->rfchange_inprogress = false;
		spin_unlock(&rtlpriv->locks.rf_ps_lock);
	} else {
		if (ppsc->reg_rfps_level & RT_RF_OFF_LEVL_HALT_NIC)
			RT_SET_PS_LEVEL(ppsc, RT_RF_OFF_LEVL_HALT_NIC);

		spin_lock(&rtlpriv->locks.rf_ps_lock);
		ppsc->rfchange_inprogress = false;
		spin_unlock(&rtlpriv->locks.rf_ps_lock);
	}

	*valid = 1;
	return !ppsc->hwradiooff;

}


void rtl88eu_set_key(struct rtl8xxxu_priv *priv, u32 key_index,
		     u8 *p_macaddr, bool is_group, u8 enc_algo,
		     bool is_wepkey, bool clear_all)
{
	u8 *macaddr = p_macaddr;
	u32 entry_id = 0;
	bool is_pairwise = false;
	static u8 cam_const_addr[4][6] = {
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x01},
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x02},
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x03}
	};
	static u8 cam_const_broad[] = {
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff
	};

	if (clear_all) {
		u8 idx = 0;
		u8 cam_offset = 0;
		u8 clear_number = 5;

		RT_TRACE(priv, COMP_SEC, DBG_DMESG, "clear_all\n");

		for (idx = 0; idx < clear_number; idx++) {
			rtl_cam_mark_invalid(hw, cam_offset + idx);
			rtl_cam_empty_entry(hw, cam_offset + idx);

			if (idx < 5) {
				memset(rtlpriv->sec.key_buf[idx], 0,
				       MAX_KEY_LEN);
				rtlpriv->sec.key_len[idx] = 0;
			}
		}

	} else {
		switch (enc_algo) {
		case WEP40_ENCRYPTION:
			enc_algo = CAM_WEP40;
			break;
		case WEP104_ENCRYPTION:
			enc_algo = CAM_WEP104;
			break;
		case TKIP_ENCRYPTION:
			enc_algo = CAM_TKIP;
			break;
		case AESCCMP_ENCRYPTION:
			enc_algo = CAM_AES;
			break;
		default:
			RT_TRACE(priv, COMP_ERR, DBG_EMERG,
				 "switch case not process\n");
			enc_algo = CAM_TKIP;
			break;
		}

		if (is_wepkey || rtlpriv->sec.use_defaultkey) {
			macaddr = cam_const_addr[key_index];
			entry_id = key_index;
		} else {
			if (is_group) {
				macaddr = cam_const_broad;
				entry_id = key_index;
			} else {
				if (mac->opmode == NL80211_IFTYPE_AP ||
				    mac->opmode == NL80211_IFTYPE_MESH_POINT) {
					entry_id =
					  rtl_cam_get_free_entry(hw, p_macaddr);
					if (entry_id >=  TOTAL_CAM_ENTRY) {
						RT_TRACE(priv, COMP_SEC,
							 DBG_EMERG,
							 "Can not find free hw security cam entry\n");
						return;
					}
				} else {
					entry_id = CAM_PAIRWISE_KEY_POSITION;
				}
				key_index = PAIRWISE_KEYIDX;
				is_pairwise = true;
			}
		}

		if (rtlpriv->sec.key_len[key_index] == 0) {
			RT_TRACE(priv, COMP_SEC, DBG_DMESG,
				 "delete one entry, entry_id is %d\n",
				 entry_id);
			if (mac->opmode == NL80211_IFTYPE_AP ||
				mac->opmode == NL80211_IFTYPE_MESH_POINT)
				rtl_cam_del_entry(hw, p_macaddr);
			rtl_cam_delete_one_entry(hw, p_macaddr, entry_id);
		} else {
			RT_TRACE(priv, COMP_SEC, DBG_DMESG,
				 "add one entry\n");
			if (is_pairwise) {
				RT_TRACE(priv, COMP_SEC, DBG_DMESG,
					 "set Pairwise key\n");

				rtl_cam_add_one_entry(hw, macaddr, key_index,
						      entry_id, enc_algo,
						      CAM_CONFIG_NO_USEDK,
						      rtlpriv->sec.key_buf[key_index]);
			} else {
				RT_TRACE(priv, COMP_SEC, DBG_DMESG,
					 "set group key\n");

				if (mac->opmode == NL80211_IFTYPE_ADHOC) {
					rtl_cam_add_one_entry(hw,
							rtlefuse.dev_addr,
							PAIRWISE_KEYIDX,
							CAM_PAIRWISE_KEY_POSITION,
							enc_algo,
							CAM_CONFIG_NO_USEDK,
							rtlpriv->sec.key_buf
							[entry_id]);
				}

				rtl_cam_add_one_entry(hw, macaddr, key_index,
						      entry_id, enc_algo,
						      CAM_CONFIG_NO_USEDK,
						      rtlpriv->sec.key_buf[entry_id]);
			}

		}
	}
}

#endif

void rtl88eu_enable_hw_security_config(struct rtl8xxxu_priv *priv)
{
	u8 sec_reg_value;

	sec_reg_value = SCR_TXENCENABLE | SCR_RXDECENABLE;

#if 0
	if (rtlpriv->sec.use_defaultkey) {
		sec_reg_value |= SCR_TXUSEDK;
		sec_reg_value |= SCR_RXUSEDK;
	}
#else
	sec_reg_value |= SCR_TXUSEDK;
	sec_reg_value |= SCR_RXUSEDK;
#endif

	sec_reg_value |= (SCR_RXBCUSEDK | SCR_TXBCUSEDK);

	rtl8xxxu_write8(priv, REG_CR + 1, 0x02);

	RT_TRACE(priv, COMP_SEC, DBG_DMESG,
		 "The SECR-value %x\n", sec_reg_value);

	rtl8xxxu_write8(priv, REG_SECCFG, sec_reg_value);
}

char fw_name[100] = "rtlwifi/rtl8188eufw.bin";

int rtl88eu_hw_init(struct ieee80211_hw *hw)
{
	struct rtl8xxxu_priv *priv = hw->priv;
	u8 value8 = 0;
	u16 value16;
	u32 status = true;
	unsigned long flags;
	int i;

	reg_bcn_ctrl_val = 0;
	last_hmeboxnum = 0;

	memset(&rtlefuse, 0, sizeof(struct rtl_efuse));
	memset(&rtlphy, 0, sizeof(struct rtl_phy));
	memset(&rtldm, 0, sizeof(struct rtl_dm));
	memset(&dm_dig, 0, sizeof(struct dig_t));
	memset(&falsealm_cnt, 0, sizeof(struct false_alarm_statistics));
	memset(&p_ra, 0, sizeof(struct rate_adaptive));
	memset(&dm_pstable, 0, sizeof(struct ps_t));

#if 1
	/* TODO
	 *  temp code
	 */
	rtl88eu_read_eeprom_info(priv);
	rtlefuse.autoload_failflag = 0;
#endif

	spin_lock_init(&rf_lock);
	local_save_flags(flags);
	local_irq_disable();

	status = _rtl88eu_init_mac(priv);
	if (status) {
		RT_TRACE(priv, COMP_ERR, DBG_EMERG, "init mac failed!\n");
		goto exit;
	}
	status = rtl88eu_download_fw(priv, false);
	if (status) {
		RT_TRACE(priv, COMP_ERR, DBG_EMERG,
			 "Download Firmware failed!!\n");
		status = true;
		return status;
	}
	local_irq_enable();
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
	rtl88e_phy_mac_config(priv);
	rtl88e_phy_bb_config(priv);
	rtl88e_phy_rf_config(priv);
	rtlphy.rfreg_chnlval[0] = rtl_get_rfreg(priv, (enum radio_path)0,
				      RF_CHNLBW, RFREG_OFFSET_MASK);
	status = rtl88eu_iol_efuse_patch(priv);
	if (status == false) {
		RT_TRACE(priv, COMP_ERR, DBG_EMERG,
			 "rtl88eu_iol_efuse_patch failed\n");
		goto exit;
	}

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
	_rtl88eu_bb_turn_on_block(priv);
	rtl_cam_reset_all_entry(priv);
	rtl88eu_enable_hw_security_config(priv);

	for (i = 0; i < ETH_ALEN; i++)
		rtl8xxxu_write8(priv, REG_MACID + i, priv->mac_addr[i]);

	rtl88e_phy_set_txpower_level(priv, rtlphy.current_channel, false);
	_rtl88eu_init_ant_selection(priv);
	rtl8xxxu_write32(priv, REG_BAR_MODE_CTRL, 0x0201ffff);
	rtl8xxxu_write8(priv, REG_HWSEQ_CTRL, 0xFF);

	rtl8xxxu_write8(priv, 0x652, 0x0);
	rtl8xxxu_write8(priv,  REG_FWHW_TXQ_CTRL+1, 0x0F);
	rtl8xxxu_write8(priv, REG_EARLY_MODE_CONTROL+3, 0x01);
	rtl8xxxu_write16(priv, REG_TX_RPT_TIME, 0x3DF0);
	rtl8xxxu_write16(priv, REG_TXDMA_OFFSET_CHK,
		       (rtl8xxxu_read16(priv, REG_TXDMA_OFFSET_CHK) |
			DROP_DATA_EN));
#if 0
	if (ppsc->rfpwr_state == ERFON) {
		if ((rtlefuse.antenna_div_type == CGCS_RX_HW_ANTDIV) ||
		    ((rtlefuse.antenna_div_type == CG_TRX_HW_ANTDIV) &&
		     (rtlhal->oem_id == RT_CID_819X_HP))) {
#else
	if (1) {
		if (rtlefuse.antenna_div_type == CGCS_RX_HW_ANTDIV) {
#endif
			rtl88e_phy_set_rfpath_switch(priv, true);
			rtldm.fat_table.rx_idle_ant = MAIN_ANT;
		} else {
			rtl88e_phy_set_rfpath_switch(priv, false);
			rtldm.fat_table.rx_idle_ant = AUX_ANT;
		}
		RT_TRACE(priv, COMP_INIT, DBG_LOUD, "rx idle ant %s\n",
			 (rtldm.fat_table.rx_idle_ant == MAIN_ANT) ?
			 ("MAIN_ANT") : ("AUX_ANT"));

		if (rtlphy.iqk_initialized) {
				rtl88e_phy_iq_calibrate(priv);
		} else {
			rtl88e_phy_iq_calibrate(priv);
			rtlphy.iqk_initialized = true;
		}
		//rtl88e_dm_check_txpower_tracking(priv);
		rtl88e_phy_lc_calibrate(priv, false);
	}
	
	rtl8xxxu_write8(priv, REG_USB_HRPWM, 0);
	rtl8xxxu_write32(priv, REG_FWHW_TXQ_CTRL,
			rtl8xxxu_read32(priv, REG_FWHW_TXQ_CTRL)|BIT(12));
	//rtl88e_dm_init(priv);
	rtl88eu_hal_notch_filter(priv, 0);
	local_irq_restore(flags);
	return 0;
exit:
	local_irq_restore(flags);
	complete(&priv->firmware_loading_complete);
	return status;
}

static int rtl8192eu_parse_efuse(struct rtl8xxxu_priv *priv)
{
	return 0;
}

static int rtl8188eu_load_firmware(struct rtl8xxxu_priv *priv)
{
#if 0
	char *fw_name;
	int ret;

	fw_name = "rtlwifi/rtl8188eufw.bin";

	init_completion(&priv->firmware_loading_complete);
	err = request_firmware_nowait(THIS_MODULE, 1,
				      fw_name, &priv->udev->dev,
				      GFP_KERNEL, hw, rtl_fw_cb);


	return ret;
#endif
	return 0;
}

static int rtl8192eu_power_on(struct rtl8xxxu_priv *priv)
{
	return 0;
}

static void rtl8192eu_init_phy_bb(struct rtl8xxxu_priv *priv)
{
}

static int rtl8192eu_init_phy_rf(struct rtl8xxxu_priv *priv)
{
	return 0;
}

static void rtl8192eu_phy_iq_calibrate(struct rtl8xxxu_priv *priv)
{
}


static void rtl8192e_enable_rf(struct rtl8xxxu_priv *priv)
{
}

void rtl8188eu_config_channel(struct ieee80211_hw *hw)
{
	struct rtl8xxxu_priv *priv = hw->priv;
	rtl88e_phy_sw_chnl(priv);
	rtl88eu_update_channel_access_setting(priv);
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
#if 0
	rx_status->rate_idx = rate_idx;
#else

	rx_status->rate_idx = 7;
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



static void
rtl8192e_set_tx_power(struct rtl8xxxu_priv *priv, int channel, bool ht40)
{
	u32 val32, ofdm, mcs;
	u8 cck, ofdmbase, mcsbase;
	int group, tx_idx;

	tx_idx = 0;
	group = rtl8xxxu_gen2_channel_to_group(channel);

	cck = priv->cck_tx_power_index_A[group];

	val32 = rtl8xxxu_read32(priv, REG_TX_AGC_A_CCK1_MCS32);
	val32 &= 0xffff00ff;
	val32 |= (cck << 8);
	rtl8xxxu_write32(priv, REG_TX_AGC_A_CCK1_MCS32, val32);

	val32 = rtl8xxxu_read32(priv, REG_TX_AGC_B_CCK11_A_CCK2_11);
	val32 &= 0xff;
	val32 |= ((cck << 8) | (cck << 16) | (cck << 24));
	rtl8xxxu_write32(priv, REG_TX_AGC_B_CCK11_A_CCK2_11, val32);

	ofdmbase = priv->ht40_1s_tx_power_index_A[group];
	ofdmbase += priv->ofdm_tx_power_diff[tx_idx].a;
	ofdm = ofdmbase | ofdmbase << 8 | ofdmbase << 16 | ofdmbase << 24;

	rtl8xxxu_write32(priv, REG_TX_AGC_A_RATE18_06, ofdm);
	rtl8xxxu_write32(priv, REG_TX_AGC_A_RATE54_24, ofdm);

	mcsbase = priv->ht40_1s_tx_power_index_A[group];
	if (ht40)
		mcsbase += priv->ht40_tx_power_diff[tx_idx++].a;
	else
		mcsbase += priv->ht20_tx_power_diff[tx_idx++].a;
	mcs = mcsbase | mcsbase << 8 | mcsbase << 16 | mcsbase << 24;

	rtl8xxxu_write32(priv, REG_TX_AGC_A_MCS03_MCS00, mcs);
	rtl8xxxu_write32(priv, REG_TX_AGC_A_MCS07_MCS04, mcs);
	rtl8xxxu_write32(priv, REG_TX_AGC_A_MCS11_MCS08, mcs);
	rtl8xxxu_write32(priv, REG_TX_AGC_A_MCS15_MCS12, mcs);

	if (priv->tx_paths > 1) {
		cck = priv->cck_tx_power_index_B[group];

		val32 = rtl8xxxu_read32(priv, REG_TX_AGC_B_CCK1_55_MCS32);
		val32 &= 0xff;
		val32 |= ((cck << 8) | (cck << 16) | (cck << 24));
		rtl8xxxu_write32(priv, REG_TX_AGC_B_CCK1_55_MCS32, val32);

		val32 = rtl8xxxu_read32(priv, REG_TX_AGC_B_CCK11_A_CCK2_11);
		val32 &= 0xffffff00;
		val32 |= cck;
		rtl8xxxu_write32(priv, REG_TX_AGC_B_CCK11_A_CCK2_11, val32);

		ofdmbase = priv->ht40_1s_tx_power_index_B[group];
		ofdmbase += priv->ofdm_tx_power_diff[tx_idx].b;
		ofdm = ofdmbase | ofdmbase << 8 |
			ofdmbase << 16 | ofdmbase << 24;

		rtl8xxxu_write32(priv, REG_TX_AGC_B_RATE18_06, ofdm);
		rtl8xxxu_write32(priv, REG_TX_AGC_B_RATE54_24, ofdm);

		mcsbase = priv->ht40_1s_tx_power_index_B[group];
		if (ht40)
			mcsbase += priv->ht40_tx_power_diff[tx_idx++].b;
		else
			mcsbase += priv->ht20_tx_power_diff[tx_idx++].b;
		mcs = mcsbase | mcsbase << 8 | mcsbase << 16 | mcsbase << 24;

		rtl8xxxu_write32(priv, REG_TX_AGC_B_MCS03_MCS00, mcs);
		rtl8xxxu_write32(priv, REG_TX_AGC_B_MCS07_MCS04, mcs);
		rtl8xxxu_write32(priv, REG_TX_AGC_B_MCS11_MCS08, mcs);
		rtl8xxxu_write32(priv, REG_TX_AGC_B_MCS15_MCS12, mcs);
	}
}
#if 0
void rtl88eu_set_fw_joinbss_report_cmd(struct ieee80211_hw *hw, u8 mstatus)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	struct rtl_mac *mac = rtl_mac(rtl_priv(hw));

	u8 dlbcn_cnt = 0;
	u32 poll = 0;
	u8 value8;
	bool send_beacon = false;
	bool bcn_valid = false;

	if (mstatus == 1 ) {
		rtl8xxxu_write16(priv, REG_BCN_PSR_RPT,
			       (0xc000 | mac->assoc_id));
		rtl8xxxu_write8(priv, REG_CR + 1,
			       rtl8xxxu_read8(priv, REG_CR + 1) | BIT(0));
		rtl8xxxu_write8(priv, REG_BCN_CTRL,
			       rtl8xxxu_read8(priv, REG_BCN_CTRL) &
					     (~BIT(3)));
		rtl8xxxu_write8(priv, REG_BCN_CTRL,
			       rtl8xxxu_read8(priv, REG_BCN_CTRL) | BIT(4));

		value8 = rtl8xxxu_read8(priv, REG_FWHW_TXQ_CTRL + 2);
		if (value8 & BIT(6))
			send_beacon = true;

		rtl8xxxu_write8(priv, REG_FWHW_TXQ_CTRL + 2,
			       (value8 & (~BIT(6))));
		rtl8xxxu_write8(priv, REG_TDECTRL + 2,
			       rtl8xxxu_read8(priv, REG_TDECTRL + 2) |
					     BIT(0));

		dlbcn_cnt = 0;
		poll = 0;

		do {
			rtl88eu_set_fw_rsvdpagepkt(hw, false);
			dlbcn_cnt++;
			do {
				yield();
				bcn_valid = (BIT(0) & rtl8xxxu_read8(priv,
						REG_TDECTRL+2)) ? true : false;
				poll++;
			} while (!bcn_valid && (poll%10) != 0);
		} while ((!bcn_valid) && (dlbcn_cnt <= 100));

		if (!bcn_valid) {
			RT_TRACE(rtlpriv, COMP_FW, DBG_LOUD,
				 "Download RSVD page failed!\n");
		} else {
			RT_TRACE(rtlpriv, COMP_FW, DBG_LOUD,
				 "Download RSVD success!\n");
		}

		rtl8xxxu_write8(priv, REG_BCN_CTRL,
			       rtl8xxxu_read8(priv, REG_BCN_CTRL) | BIT(3));
		rtl8xxxu_write8(priv, REG_BCN_CTRL,
			       rtl8xxxu_read8(priv, REG_BCN_CTRL) &
					     (~BIT(4)));

		if (send_beacon) {
			rtl8xxxu_write8(priv, REG_FWHW_TXQ_CTRL + 2,
				       rtl8xxxu_read8(priv,
						     REG_FWHW_TXQ_CTRL + 2) |
						     BIT(6));
		}

		if (bcn_valid) {
			rtl8xxxu_write8(priv, REG_TDECTRL + 2,
				rtl8xxxu_read8(priv, REG_TDECTRL + 2) |
					      BIT(0));
		}

		rtl8xxxu_write8(priv, REG_CR + 1,
			       rtl8xxxu_read8(priv, REG_CR + 1) | (~BIT(0)));
	}
}
#endif

static int rtl8xxxu_gen1_channel_to_group(int channel)
{
	int group;

	if (channel < 4)
		group = 0;
	else if (channel < 10)
		group = 1;
	else
		group = 2;

	return group;
}

static void _rtl_tx_desc_checksum(u8 *txdesc)
{
	u16 *ptr = (u16 *)txdesc;
	u16	checksum = 0;
	u32 index;

	/* Clear first */
	SET_TX_DESC_TX_DESC_CHECKSUM(txdesc, 0);
	for (index = 0; index < 16; index++)
		checksum = checksum ^ (*(ptr + index));
	SET_TX_DESC_TX_DESC_CHECKSUM(txdesc, checksum);
}


static enum rtl_desc_qsel _rtl8188eu_mq_to_descq(struct ieee80211_hw *hw,
					 __le16 fc, u16 mac80211_queue_index)
{
	enum rtl_desc_qsel qsel;

	if (unlikely(ieee80211_is_beacon(fc))) {
		qsel = QSLT_BEACON;
		goto out;
	}
	if (ieee80211_is_mgmt(fc)) {
		qsel = QSLT_MGNT;
		goto out;
	}
	switch (mac80211_queue_index) {
	case 0:	/* VO */
		qsel = QSLT_VO;
		break;
	case 1:	/* VI */
		qsel = QSLT_VI;
		break;
	case 3:	/* BK */
		qsel = QSLT_BK;
		break;
	case 2:	/* BE */
	default:
		qsel = QSLT_BE;
		break;
	}
out:
	return qsel;
}

#if 0
void rtl_get_tcb_desc(struct ieee80211_hw *hw,
		      struct ieee80211_tx_info *info,
		      struct ieee80211_sta *sta,
		      struct sk_buff *skb, struct rtl_tcb_desc *tcb_desc)
{
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)(skb->data);
	struct ieee80211_rate *txrate;
	__le16 fc = hdr->frame_control;

	txrate = ieee80211_get_tx_rate(hw, info);
	if (txrate)
		tcb_desc->hw_rate = txrate->hw_value;

	if (ieee80211_is_data(fc)) {
		/*
		 *we set data rate INX 0
		 *in rtl_rc.c   if skb is special data or
		 *mgt which need low data rate.
		 */

		/*
		 *So tcb_desc->hw_rate is just used for
		 *special data and mgt frames
		 */
		if (info->control.rates[0].idx == 0 ||
				ieee80211_is_nullfunc(fc)) {
			tcb_desc->use_driver_rate = true;
			tcb_desc->ratr_index = 7;

			tcb_desc->disable_ratefallback = 1;
		} else {
			/*
			 *because hw will nerver use hw_rate
			 *when tcb_desc->use_driver_rate = false
			 *so we never set highest N rate here,
			 *and N rate will all be controlled by FW
			 *when tcb_desc->use_driver_rate = false
			 */
			if (sta && sta->vht_cap.vht_supported) {
#if 0
				tcb_desc->hw_rate =
				_rtl_get_vht_highest_n_rate(hw, sta);
#else
				tcb_desc->hw_rate = 0x13;
#endif
			} else {
				if (sta && (sta->ht_cap.ht_supported)) {
					tcb_desc->hw_rate = 0x13;
				} else {
					if (rtlmac->mode == WIRELESS_MODE_B) {
						tcb_desc->hw_rate = 0x03;
					} else {
						tcb_desc->hw_rate = 0x0b;
					}
				}
			}
		}

		if (is_multicast_ether_addr(ieee80211_get_DA(hdr)))
			tcb_desc->multicast = 1;
		else if (is_broadcast_ether_addr(ieee80211_get_DA(hdr)))
			tcb_desc->broadcast = 1;

		_rtl_txrate_selectmode(hw, sta, tcb_desc);
		_rtl_query_bandwidth_mode(hw, sta, tcb_desc);
		_rtl_qurey_shortpreamble_mode(hw, tcb_desc, info);
		_rtl_query_shortgi(hw, sta, tcb_desc, info);
		_rtl_query_protection_mode(hw, tcb_desc, info);
	} else {
		tcb_desc->use_driver_rate = true;
		tcb_desc->ratr_index = RATR_INX_WIRELESS_MC;
		tcb_desc->disable_ratefallback = 1;
		tcb_desc->mac_id = 0;
		tcb_desc->packet_bw = false;
	}
}
#endif

int rtl8188eu_tx(struct ieee80211_hw *hw, struct ieee80211_sta *sta,
			struct sk_buff *skb)
{
	u16 seq_number;
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
	__le16 fc = hdr->frame_control;
	u16 pktlen = skb->len;
	int fw_qsel = 0;
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	u8 *txdesc;
	//struct rtl_tcb_desc tcb_desc;
	//memset(&tcb_desc, 0, sizeof(struct rtl_tcb_desc));

	seq_number = (le16_to_cpu(hdr->seq_ctrl) & IEEE80211_SCTL_SEQ) >> 4;
	//rtl_get_tcb_desc(hw, info, sta, skb, &tcb_desc);
	txdesc = (u8 *)skb_push(skb, 32);
	memset(txdesc, 0, 32);

	info->rate_driver_data[0] = hw;
	/* TODO 반드시 알아낼것 */
	info->control.rates[0].idx = 0xb;

	
	SET_TX_DESC_OWN(txdesc, 1);
	SET_TX_DESC_LAST_SEG(txdesc, 1);
	SET_TX_DESC_FIRST_SEG(txdesc, 1);
	SET_TX_DESC_PKT_SIZE(txdesc, pktlen);
	SET_TX_DESC_OFFSET(txdesc, 32);
	
	if (is_multicast_ether_addr(ieee80211_get_DA(hdr)) ||
	    is_broadcast_ether_addr(ieee80211_get_DA(hdr)))
		SET_TX_DESC_BMC(txdesc, 1);
	/* TODO */
	SET_TX_DESC_PKT_OFFSET(txdesc, 0);
	SET_TX_DESC_USE_RATE(txdesc, 1);
	if (ieee80211_is_data(fc) || ieee80211_is_data_qos(fc)) {
#if 0
		SET_TX_DESC_MACID(txdesc, tcb_desc.mac_id);
#else
		SET_TX_DESC_MACID(txdesc, 1);
#endif
		SET_TX_DESC_QUEUE_SEL(txdesc, fw_qsel);
#if 0
		SET_TX_DESC_RATE_ID(txdesc, tcb_desc.ratr_index);
#else
		SET_TX_DESC_RATE_ID(txdesc, 5);
#endif
	
#if 0
		rcu_read_lock();
		sta = ieee80211_find_sta(mac->vif, mac->bssid);
		if (sta) {
			u8 ampdu_density = sta->ht_cap.ampdu_density;
			SET_TX_DESC_AMPDU_DENSITY(txdesc, ampdu_density);
		}
		rcu_read_unlock();
#endif
		if (info->control.hw_key) {
			struct ieee80211_key_conf *keyconf = info->control.hw_key;
			switch (keyconf->cipher) {
			case WLAN_CIPHER_SUITE_WEP40:
			case WLAN_CIPHER_SUITE_WEP104:
			case WLAN_CIPHER_SUITE_TKIP:
				SET_TX_DESC_SEC_TYPE(txdesc, 0x1);
				break;
			case WLAN_CIPHER_SUITE_CCMP:
				SET_TX_DESC_SEC_TYPE(txdesc, 0x3);
				break;
			default:
				SET_TX_DESC_SEC_TYPE(txdesc, 0x0);
				break;
			}
		}
		if (info->flags & IEEE80211_TX_CTL_AMPDU) {
			SET_TX_DESC_AGG_ENABLE(txdesc, 1);
			SET_TX_DESC_MAX_AGG_NUM(txdesc, 0x1f);
			SET_TX_DESC_MCSG1_MAX_LEN(txdesc, 6);
			SET_TX_DESC_MCSG2_MAX_LEN(txdesc, 6);
			SET_TX_DESC_MCSG3_MAX_LEN(txdesc, 6);
			SET_TX_DESC_MCS7_SGI_MAX_LEN(txdesc, 6);
		} else {
			SET_TX_DESC_AGG_BREAK(txdesc, 1);
		}
		SET_TX_DESC_SEQ(txdesc, seq_number);

		/* TODO */
		if (ieee80211_is_data_qos(fc))
			SET_TX_DESC_QOS(txdesc, 1);
		/* TODO */
		/* USB_TXAGG_NUM */
#if 0
		if (!rtlpriv->ra.is_special_data) {
#else
		if (1) {
#endif
			/* SET_TX_DESC_RTS_ENABLE(txdesc, 1); */
			/* SET_TX_DESC_HW_RTS_ENABLE(txdesc, 1); */
			SET_TX_DESC_DATA_BW(txdesc, 1);
			SET_TX_DESC_TX_SUB_CARRIER(txdesc, 3);
			SET_TX_DESC_RTS_RATE(txdesc, 8);
			SET_TX_DESC_DATA_RATE_FB_LIMIT(txdesc, 0x1f);
			SET_TX_DESC_RTS_RATE_FB_LIMIT(txdesc, 0xf);
#if 0
			if (tcb_desc.use_shortgi)
				SET_TX_DESC_DATA_SHORTGI(txdesc, 1);
#endif
#if 0
			SET_TX_DESC_TX_RATE(txdesc, tcb_desc.hw_rate);
#else
			SET_TX_DESC_TX_RATE(txdesc, 0x0b);
#endif
			/* TODO */
			/* SET_TX_DESC_PWR_STATUS(txdesc, pwr_status); */
		} else {
			SET_TX_DESC_AGG_BREAK(txdesc, 1);
#if 0
			if (tcb_desc.use_shortpreamble)
				SET_TX_DESC_DATA_SHORT(txdesc, 1);
#endif
			SET_TX_DESC_TX_RATE(txdesc, 0x0b);
		}
#if 1
	} else if (ieee80211_is_mgmt(fc)){
		SET_TX_DESC_MACID(txdesc, 1);
		SET_TX_DESC_QUEUE_SEL(txdesc, fw_qsel);
		SET_TX_DESC_RATE_ID(txdesc, 5);
		/* TODO */
#if 0
		if (pxmitframe->ack_report)
			SET_TX_DESC_CCX(txdesc, 1);
#endif
		SET_TX_DESC_SEQ(txdesc, seq_number);
		SET_TX_DESC_RETRY_LIMIT_ENABLE(txdesc, 1);
		/* TODO */
		SET_TX_DESC_DATA_RETRY_LIMIT(txdesc, 0x6);
		SET_TX_DESC_TX_RATE(txdesc, 1);
	} else {
		SET_TX_DESC_MACID(txdesc, 1);
		SET_TX_DESC_RATE_ID(txdesc, 5);
		SET_TX_DESC_SEQ(txdesc, seq_number);
		SET_TX_DESC_TX_RATE(txdesc, 1);
	}
	if (!ieee80211_is_data_qos(fc)) {
		SET_TX_DESC_HWSEQ_EN(txdesc, 1);
		SET_TX_DESC_HWSEQ_SSN(txdesc, 1);
#endif
	}
	rtl88e_dm_set_tx_ant_by_tx_info(hw->priv, txdesc, 0);
	_rtl_tx_desc_checksum(txdesc);
	return 0;
}


void rtl8188eu_disable_rf(struct rtl8xxxu_priv *priv)
{
}

struct rtl8xxxu_fileops rtl8188eu_fops = {
	.init_device = rtl88eu_hw_init, //done
#if 0
	.parse_efuse = rtl8188eu_parse_efuse, // done
	.read_efuse = rtl8188eu_read_eeprom_info, // 초기화 부분 수정해야함.
	.load_firmware = rtl8188eu_load_firmware, // maybe done
	.power_on = rtl8188eu_power_on, //done
	.power_off = rtl8xxxu_power_off,
	.reset_8051 = rtl8188eu_reset_8051, //done
	.llt_init = rtl8188eu_init_llt_table, //done
	.init_phy_bb = rtl8188eu_init_phy_bb, //maybe done
	.init_phy_rf = rtl8188eu_init_phy_rf,  //done
	.phy_iq_calibrate = rtl88eu_phy_iq_calibrate, //maybe done
#if 1
	.config_channel = rtl88e_phy_set_bw_mode_callback,
#else
	.config_channel = rtl8xxxu_gen1_config_channel,
#endif
	.parse_rx_desc = rtl8xxxu_parse_rxdesc16,
	.enable_rf = rtl8xxxu_gen1_enable_rf,
	.disable_rf = rtl8xxxu_gen1_disable_rf,
	.usb_quirks = rtl8xxxu_gen1_usb_quirks,
#if 0
	.set_tx_power = rtl8188eu_set_tx_power, //NOW!!!!!!!!
#else
	.set_tx_power = rtl8192e_set_tx_power, //NOW!!!!!!!!
#endif
	.update_rate_mask = rtl8xxxu_update_rate_mask,
	.report_connect = rtl8xxxu_gen1_report_connect,
	.writeN_block_size = 128,
	.tx_desc_size = sizeof(struct rtl8xxxu_txdesc32),
	.rx_desc_size = sizeof(struct rtl8xxxu_rxdesc16),
	.adda_1t_init = 0x0b1b25a0,
	.adda_1t_path_on = 0x0bdb25a0,
	.adda_2t_path_on_a = 0x04db25a4,
	.adda_2t_path_on_b = 0x0b1b25a4,
	.trxff_boundary = 0x23ff,
	.mactable = rtl8xxxu_gen1_mac_init_table,
#else
	.parse_efuse = rtl8192eu_parse_efuse,
	.load_firmware = rtl8188eu_load_firmware,
	.power_on = rtl8192eu_power_on,
	.power_off = rtl88eu_card_disable,
	.reset_8051 = rtl8xxxu_reset_8051,
	.llt_init = rtl8xxxu_auto_llt_table,
	.init_phy_bb = rtl8192eu_init_phy_bb,
	.init_phy_rf = rtl8192eu_init_phy_rf,
	.phy_iq_calibrate = rtl88e_phy_iq_calibrate, //done
#if 0
	.config_channel = rtl8188eu_config_channel, //maybe done?
#else
	.config_channel = rtl8xxxu_gen1_config_channel,
#endif
#if 0
	.parse_rx_desc = rtl8xxxu_parse_rxdesc24,
#else
	.parse_rx_desc = rtl88eu_rx_query_desc,
#endif
#if 1
	.enable_rf = rtl8192e_enable_rf,
#else
	.enable_rf = rtl8xxxu_gen1_enable_rf,
#endif
#if 1
	.disable_rf = rtl8xxxu_gen1_disable_rf,
#else
	.disable_rf = rtl8188eu_disable_rf,
#endif
	.usb_quirks = rtl8xxxu_gen1_usb_quirks,
	.set_tx_power = rtl88e_phy_set_txpower_level, //done
#if 0
	.update_rate_mask = rtl88eu_update_hal_rate_tbl, //done
#else
	.update_rate_mask = rtl8xxxu_update_rate_mask,
#endif
	.report_connect = rtl8xxxu_gen2_report_connect,
	.adapter_tx = rtl8188eu_tx,
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
	.mactable = rtl8xxxu_gen1_mac_init_table,

#endif
};
