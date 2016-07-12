/******************************************************************************
 *
 * Copyright(c) 2009-2013  Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 * wlanfae <wlanfae@realtek.com>
 * Realtek Corporation, No. 2, Innovation Road II, Hsinchu Science Park,
 * Hsinchu 300, Taiwan.
 *
 * Taehee Yoo	<ap420073@gmail.com>
 * Larry Finger <Larry.Finger@lwfinger.net>
 *
 *****************************************************************************/

#ifndef __RTL88EU_HW_H__
#define __RTL88EU_HW_H__

#define SW_OFFLOAD_EN			BIT(7)
#define CMD_INIT_LLT			BIT(0)
#define CMD_READ_EFUSE_MAP		BIT(1)
#define CMD_EFUSE_PATCH			BIT(2)

#define	EEPROM_DEFAULT_24G_INDEX	0x2D
#define	EEPROM_DEFAULT_24G_HT20_DIFF	0X02
#define	EEPROM_DEFAULT_24G_OFDM_DIFF	0X04
#define	EEPROM_DEFAULT_DIFF		0XFE

#define	EEPROM_DEFAULT_BOARD_OPTION	0x00
#define	EEPROM_VERSION_88E		0xC4
#define	EEPROM_CUSTOMERID_88E		0xC5
#define TX_SELE_HQ			BIT(0)
#define TX_SELE_LQ			BIT(1)
#define TX_SELE_NQ			BIT(2)
#define CALTMR_EN			BIT(10)
#define REG_HISR_88E			0x00B4
#define	IMR_PSTIMEOUT_88E		BIT(29)
#define REG_HIMR_88E			0x00B0
#define	IMR_TXERR_88E			BIT(11)
#define REG_HIMRE_88E			0x00B8
#define INT_BULK_SEL			BIT(4)
#define	IMR_TBDER_88E			BIT(26)
#define	IMR_RXERR_88E			BIT(10)
#define	RCR_APP_PHYSTS			BIT(28)
#define BCN_DMA_ATIME_INT_TIME		0x02
#define TX_TOTAL_PAGE_NUMBER_88E	0xA9

#define TX_PAGE_BOUNDARY_88E (TX_TOTAL_PAGE_NUMBER_88E + 1)

#define WMM_NORMAL_TX_TOTAL_PAGE_NUMBER			\
	TX_TOTAL_PAGE_NUMBER_88E  /* 0xA9 , 0xb0=>176=>22k */
#define WMM_NORMAL_TX_PAGE_BOUNDARY_88E			\
	(WMM_NORMAL_TX_TOTAL_PAGE_NUMBER + 1) /* 0xA9 */

#define RAM_DL_SEL			BIT(7) /*  1:RAM, 0:ROM */

#define REG_BB_PAD_CTRL			0x0064
#define	IMR_DISABLED_88E		0x0
#define	EEPROM_VID_88EU			0xD0
#define	EEPROM_PID_88EU			0xD2

#define EEPROM_DEFAULT_PID		0x1234
#define EEPROM_DEFAULT_VID		0x5678
#define EEPROM_DEFAULT_CUSTOMERID	0xAB
#define	EEPROM_Default_CustomerID_8188E	0x00
#define EEPROM_Default_SubCustomerID	0xCD
#define EEPROM_Default_Version		0

#define	EEPROM_MAC_ADDR_88EU		0xD7
#define _HW_STATE_NOLINK_		0x00
#define _HW_STATE_ADHOC_		0x01
#define _HW_STATE_STATION_		0x02
#define _HW_STATE_AP_			0x03

#define REG_TSFTR_SYN_OFFSET		0x0518
#define REG_BCN_CTRL_1			0x0551
#define EEPROM_DEFAULT_CRYSTALCAP_88E	0x20
#define	EEPROM_Default_ThermalMeter_88E	0x18

#define	IMR_CPWM_88E			BIT(8)
#define	IMR_CPWM2_88E			BIT(9)

#define	IMR_TXFOVW_88E			BIT(9)
#define	IMR_RXFOVW_88E			BIT(8)

#define	MAX_TX_COUNT				4

#define MAX_PRECMD_CNT				16
#define MAX_RFDEPENDCMD_CNT		16
#define MAX_POSTCMD_CNT				16

#define MAX_DOZE_WAITING_TIMES_9x	64

#define IQK_BB_REG_NUM				9
#define MAX_TOLERANCE				5
#define	IQK_DELAY_TIME				10
#define	INDEX_MAPPING_NUM	15

#define	PATH_NUM					2

#define RF6052_MAX_PATH				2

enum swchnlcmd_id {
	CMDID_END,
	CMDID_SET_TXPOWEROWER_LEVEL,
	CMDID_BBREGWRITE10,
	CMDID_WRITEPORT_ULONG,
	CMDID_WRITEPORT_USHORT,
	CMDID_WRITEPORT_UCHAR,
	CMDID_RF_WRITEREG,
};

struct swchnlcmd {
	enum swchnlcmd_id cmdid;
	u32 para1;
	u32 para2;
	u32 msdelay;
};

enum hw90_block_e {
	HW90_BLOCK_MAC = 0,
	HW90_BLOCK_PHY0 = 1,
	HW90_BLOCK_PHY1 = 2,
	HW90_BLOCK_RF = 3,
	HW90_BLOCK_MAXIMUM = 4,
};

enum baseband_config_type {
	BASEBAND_CONFIG_PHY_REG = 0,
	BASEBAND_CONFIG_AGC_TAB = 1,
};

enum ra_offset_area {
	RA_OFFSET_LEGACY_OFDM1,
	RA_OFFSET_LEGACY_OFDM2,
	RA_OFFSET_HT_OFDM1,
	RA_OFFSET_HT_OFDM2,
	RA_OFFSET_HT_OFDM3,
	RA_OFFSET_HT_OFDM4,
	RA_OFFSET_HT_CCK,
};

enum antenna_path {
	ANTENNA_NONE,
	ANTENNA_D,
	ANTENNA_C,
	ANTENNA_CD,
	ANTENNA_B,
	ANTENNA_BD,
	ANTENNA_BC,
	ANTENNA_BCD,
	ANTENNA_A,
	ANTENNA_AD,
	ANTENNA_AC,
	ANTENNA_ACD,
	ANTENNA_AB,
	ANTENNA_ABD,
	ANTENNA_ABC,
	ANTENNA_ABCD
};

struct r_antenna_select_ofdm {
	u32 r_tx_antenna:4;
	u32 r_ant_l:4;
	u32 r_ant_non_ht:4;
	u32 r_ant_ht1:4;
	u32 r_ant_ht2:4;
	u32 r_ant_ht_s1:4;
	u32 r_ant_non_ht_s1:4;
	u32 ofdm_txsc:2;
	u32 reserved:2;
};

struct r_antenna_select_cck {
	u8 r_cckrx_enable_2:2;
	u8 r_cckrx_enable:2;
	u8 r_ccktx_enable:4;
};

struct efuse_contents {
	u8 mac_addr[ETH_ALEN];
	u8 cck_tx_power_idx[6];
	u8 ht40_1s_tx_power_idx[6];
	u8 ht40_2s_tx_power_idx_diff[3];
	u8 ht20_tx_power_idx_diff[3];
	u8 ofdm_tx_power_idx_diff[3];
	u8 ht40_max_power_offset[3];
	u8 ht20_max_power_offset[3];
	u8 channel_plan;
	u8 thermal_meter;
	u8 rf_option[5];
	u8 version;
	u8 oem_id;
	u8 regulatory;
};

struct tx_power_struct {
	u8 cck[RF6052_MAX_PATH][CHANNEL_MAX_NUMBER];
	u8 ht40_1s[RF6052_MAX_PATH][CHANNEL_MAX_NUMBER];
	u8 ht40_2s[RF6052_MAX_PATH][CHANNEL_MAX_NUMBER];
	u8 ht20_diff[RF6052_MAX_PATH][CHANNEL_MAX_NUMBER];
	u8 legacy_ht_diff[RF6052_MAX_PATH][CHANNEL_MAX_NUMBER];
	u8 legacy_ht_txpowerdiff;
	u8 groupht20[RF6052_MAX_PATH][CHANNEL_MAX_NUMBER];
	u8 groupht40[RF6052_MAX_PATH][CHANNEL_MAX_NUMBER];
	u8 pwrgroup_cnt;
	u32 mcs_original_offset[4][16];
};

enum _ANT_DIV_TYPE {
	NO_ANTDIV				= 0xFF,
	CG_TRX_HW_ANTDIV		= 0x01,
	CGCS_RX_HW_ANTDIV		= 0x02,
	FIXED_HW_ANTDIV         = 0x03,
	CG_TRX_SMART_ANTDIV		= 0x04,
	CGCS_RX_SW_ANTDIV		= 0x05,
};


#endif
