/*
 *Fatal bug.
 *For example, Tx/Rx/IO locked up,
 *memory access violation,
 *resource allocation failed,
 *unexpected HW behavior, HW BUG
 *and so on.
 */
#define DBG_EMERG			0

/*
 *Abnormal, rare, or unexpeted cases.
 *For example, Packet/IO Ctl canceled,
 *device suprisely unremoved and so on.
 */
#define	DBG_WARNING			2

/*
 *Normal case driver developer should
 *open, we can see link status like
 *assoc/AddBA/DHCP/adapter start and
 *so on basic and useful infromations.
 */
#define DBG_DMESG			3

/*
 *Normal case with useful information
 *about current SW or HW state.
 *For example, Tx/Rx descriptor to fill,
 *Tx/Rx descriptor completed status,
 *SW protocol state change, dynamic
 *mechanism state change and so on.
 */
#define DBG_LOUD			4

/*
 *Normal case with detail execution
 *flow or information.
 */
#define	DBG_TRACE			5

/*--------------------------------------------------------------
		Define the rt_trace components
--------------------------------------------------------------*/
#define COMP_ERR			BIT(0)
#define COMP_FW				BIT(1)
#define COMP_INIT			BIT(2)	/*For init/deinit */
#define COMP_RECV			BIT(3)	/*For Rx. */
#define COMP_SEND			BIT(4)	/*For Tx. */
#define COMP_MLME			BIT(5)	/*For MLME. */
#define COMP_SCAN			BIT(6)	/*For Scan. */
#define COMP_INTR			BIT(7)	/*For interrupt Related. */
#define COMP_LED			BIT(8)	/*For LED. */
#define COMP_SEC			BIT(9)	/*For sec. */
#define COMP_BEACON			BIT(10)	/*For beacon. */
#define COMP_RATE			BIT(11)	/*For rate. */
#define COMP_RXDESC			BIT(12)	/*For rx desc. */
#define COMP_DIG			BIT(13)	/*For DIG */
#define COMP_TXAGC			BIT(14)	/*For Tx power */
#define COMP_HIPWR			BIT(15)	/*For High Power Mechanism */
#define COMP_POWER			BIT(16)	/*For lps/ips/aspm. */
#define COMP_POWER_TRACKING	BIT(17)	/*For TX POWER TRACKING */
#define COMP_BB_POWERSAVING	BIT(18)
#define COMP_SWAS			BIT(19)	/*For SW Antenna Switch */
#define COMP_RF				BIT(20)	/*For RF. */
#define COMP_TURBO			BIT(21)	/*For EDCA TURBO. */
#define COMP_RATR			BIT(22)
#define COMP_CMD			BIT(23)
#define COMP_EFUSE			BIT(24)
#define COMP_QOS			BIT(25)
#define COMP_MAC80211		BIT(26)
#define COMP_REGD			BIT(27)
#define COMP_CHAN			BIT(28)
#define COMP_USB			BIT(29)
#define COMP_EASY_CONCURRENT	COMP_USB /* reuse of this bit is OK */
#define COMP_BT_COEXIST			BIT(30)
#define COMP_IQK			BIT(31)

/*--------------------------------------------------------------
		Define the rt_print components
--------------------------------------------------------------*/
/* Define EEPROM and EFUSE  check module bit*/
#define EEPROM_W			BIT(0)
#define EFUSE_PG			BIT(1)
#define EFUSE_READ_ALL			BIT(2)

/* Define init check for module bit*/
#define	INIT_EEPROM			BIT(0)
#define	INIT_TXPOWER			BIT(1)
#define	INIT_IQK			BIT(2)
#define	INIT_RF				BIT(3)

/* Define PHY-BB/RF/MAC check module bit */
#define	PHY_BBR				BIT(0)
#define	PHY_BBW				BIT(1)
#define	PHY_RFR				BIT(2)
#define	PHY_RFW				BIT(3)
#define	PHY_MACR			BIT(4)
#define	PHY_MACW			BIT(5)
#define	PHY_ALLR			BIT(6)
#define	PHY_ALLW			BIT(7)
#define	PHY_TXPWR			BIT(8)
#define	PHY_PWRDIFF			BIT(9)

/* Define Dynamic Mechanism check module bit --> FDM */
#define WA_IOT				BIT(0)
#define DM_PWDB				BIT(1)
#define DM_MONITOR			BIT(2)
#define DM_DIG				BIT(3)
#define DM_EDCA_TURBO			BIT(4)

#define DM_PWDB				BIT(1)


