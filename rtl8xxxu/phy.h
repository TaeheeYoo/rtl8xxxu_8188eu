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
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 * wlanfae <wlanfae@realtek.com>
 * Realtek Corporation, No. 2, Innovation Road II, Hsinchu Science Park,
 * Hsinchu 300, Taiwan.
 *
 * Taehee Yoo	<ap420073@gmail.com
 * Larry Finger <Larry.Finger@lwfinger.net>
 *
 *****************************************************************************/

#ifndef __RTL88EU_PHY_H__
#define __RTL88EU_PHY_H__

/* MAX_TX_COUNT must always set to 4, otherwise read efuse
 * table secquence will be wrong.
 */
u32 rtl88e_phy_query_bb_reg(struct rtl8xxxu_priv *priv,
			    u32 regaddr, u32 bitmask);
void rtl88e_phy_set_bb_reg(struct rtl8xxxu_priv *priv,
			   u32 regaddr, u32 bitmask, u32 data);
u32 rtl88e_phy_query_rf_reg(struct rtl8xxxu_priv *priv,
			    enum radio_path rfpath, u32 regaddr,
			    u32 bitmask);
void rtl88e_phy_set_rf_reg(struct rtl8xxxu_priv *priv,
			   enum radio_path rfpath, u32 regaddr,
			   u32 bitmask, u32 data);
bool rtl88e_phy_mac_config(struct rtl8xxxu_priv *priv);
bool rtl88e_phy_bb_config(struct rtl8xxxu_priv *priv);
bool rtl88e_phy_rf_config(struct rtl8xxxu_priv *priv);
void rtl88e_phy_get_hw_reg_originalvalue(struct rtl8xxxu_priv *priv);
void rtl88e_phy_get_txpower_level(struct rtl8xxxu_priv *priv,
				  long *powerlevel);
void rtl88e_phy_set_txpower_level(struct rtl8xxxu_priv *priv, u8 channel);
void rtl88e_phy_scan_operation_backup(struct rtl8xxxu_priv *priv,
				      u8 operation);
void rtl88e_phy_set_bw_mode_callback(struct rtl8xxxu_priv *priv);
void rtl88e_phy_set_bw_mode(struct rtl8xxxu_priv *priv,
			    enum nl80211_channel_type ch_type);
void rtl88e_phy_sw_chnl_callback(struct rtl8xxxu_priv *priv);
u8 rtl88e_phy_sw_chnl(struct rtl8xxxu_priv *priv);
void rtl88e_phy_iq_calibrate(struct rtl8xxxu_priv *priv, bool b_recovery);
void rtl88e_phy_lc_calibrate(struct rtl8xxxu_priv *priv, bool is2t);
void rtl88e_phy_set_rfpath_switch(struct rtl8xxxu_priv *priv, bool bmain);
bool rtl88e_phy_config_rf_with_headerfile(struct rtl8xxxu_priv *priv,
					  enum radio_path rfpath);
bool rtl88e_phy_set_io_cmd(struct rtl8xxxu_priv *priv, enum io_type iotype);
bool rtl88e_phy_set_rf_power_state(struct rtl8xxxu_priv *priv,
				   enum rf_pwrstate rfpwr_state);

#endif
