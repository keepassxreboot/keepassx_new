/* -*- mode:C; c-file-style: "bsd" -*- */
/*****************************************************************************************
**											**
**		Y K D E F  -  Common Yubikey project header				**
**											**
**	Date		/ Rev		/ Sign	/ Remark				**
**	06-06-03	/ 0.9.0		/ J E	/ Main					**
**	06-08-25	/ 1.0.0		/ J E	/ Rewritten for final spec		**
**	08-06-03	/ 1.3.0		/ J E	/ Added static OTP feature		**
**	09-06-02	/ 2.0.0		/ J E	/ Added version 2 flags			**
**	09-09-23	/ 2.1.0		/ J E	/ Added version 2.1 flags (OATH-HOTP)	**
**	10-05-01	/ 2.2.0		/ J E	/ Added support for 2.2 ext. + frame	**
**	11-04-15	/ 2.3.0		/ J E	/ Added support for 2.3 extensions	**
**	11-12-05	/ 2.4.0		/ J E	/ Added support for NFC and NDEF	**
**	12-10-28	/ 3.0.0		/ J E	/ NEO changes				**
**      13-03-05	/ 3.1.0		/ J E	/ Added EXTFLAG_LED_INV flag		**         
**      13-03-06	/ 3.1.0		/ J E	/ Added NEO startup busy flag		**
**      14-06-13	/ 3.3.0		/ J E	/ Added U2F mode modifiers		**
**      14-11-20    / 4.0.0     / J E   / Updated with Yubikey 4 PIDs                   **
**      15-03-27    / 4.1.0     / K L   / Added YK4 Capabilities                        **
**      15-06-23    / 4.2.0     / K L   / Added more YK4 Capabilities                   **
**											**
*****************************************************************************************/

#ifndef	__YKDEF_H_INCLUDED__
#define	__YKDEF_H_INCLUDED__

/* We need the structures defined here to be packed byte-wise */
#if defined(_WIN32) || defined(__GNUC__)
#pragma pack(push, 1)
#endif

/* Slot entries */

#define	SLOT_CONFIG		1   /* First (default / V1) configuration */
#define	SLOT_NAV		2   /* V1 only */
#define	SLOT_CONFIG2		3   /* Second (V2) configuration */
#define	SLOT_UPDATE1		4   /* Update slot 1 */
#define	SLOT_UPDATE2		5   /* Update slot 2 */
#define	SLOT_SWAP		6   /* Swap slot 1 and 2 */
#define	SLOT_NDEF		8   /* Write NDEF record */
#define	SLOT_NDEF2		9   /* Write NDEF record for slot 2 */

#define SLOT_DEVICE_SERIAL	0x10	/* Device serial number */
#define SLOT_DEVICE_CONFIG	0x11	/* Write device configuration record */
#define SLOT_SCAN_MAP		0x12	/* Write scancode map */
#define SLOT_YK4_CAPABILITIES	0x13	/* Read YK4 capabilities (device info) list */
#define SLOT_YK4_SET_DEVICE_INFO 0x15 /* Write device info */

#define SLOT_CHAL_OTP1		0x20	/* Write 6 byte challenge to slot 1, get Yubico OTP response */
#define SLOT_CHAL_OTP2		0x28	/* Write 6 byte challenge to slot 2, get Yubico OTP response */

#define SLOT_CHAL_HMAC1		0x30	/* Write 64 byte challenge to slot 1, get HMAC-SHA1 response */
#define SLOT_CHAL_HMAC2		0x38	/* Write 64 byte challenge to slot 2, get HMAC-SHA1 response */

#define RESP_ITEM_MASK		0x07	/* Mask for slice item # in responses */

#define RESP_TIMEOUT_WAIT_MASK	0x1f	/* Mask to get timeout value */
#define RESP_TIMEOUT_WAIT_FLAG	0x20	/* Waiting for timeout operation - seconds left in lower 5 bits */
#define RESP_PENDING_FLAG	0x40	/* Response pending flag */
#define SLOT_WRITE_FLAG		0x80	/* Write flag - set by app - cleared by device */

#define DUMMY_REPORT_WRITE	0x8f	/* Write a dummy report to force update or abort */
#define NEO_STARTUP_BUSY	0x9f	/* Status during startup (writes blocked) */

#define SHA1_MAX_BLOCK_SIZE	64	/* Max size of input SHA1 block */
#define SHA1_DIGEST_SIZE	20	/* Size of SHA1 digest = 160 bits */

#define SERIAL_NUMBER_SIZE	4	/* Size of device serial number */

/* Frame structure */

#define	SLOT_DATA_SIZE		64

struct frame_st {
    unsigned char payload[SLOT_DATA_SIZE]; /* Frame payload */
    unsigned char slot;                 /* Slot # field */
    unsigned short crc;                 /* CRC field */
    unsigned char filler[3];            /* Filler */
};

/* Ticket structure */

#define	UID_SIZE		6	/* Size of secret ID field */

struct ticket_st {
	unsigned char uid[UID_SIZE];	/* Unique (secret) ID */
	unsigned short useCtr;		/* Use counter (incremented by 1 at first use after power up) + usage flag in msb */
	unsigned short tstpl;		/* Timestamp incremented by approx 8Hz (low part) */
	unsigned char tstph;		/* Timestamp (high part) */
	unsigned char sessionCtr;	/* Number of times used within session. 0 for first use. After it wraps from 0xff to 1 */
	unsigned short rnd;		/* Pseudo-random value */
	unsigned short crc;		/* CRC16 value of all fields */
};

/* Activation modifier of sessionUse field (bitfields not uses as they are not portable) */

#define	TICKET_ACT_HIDRPT	0x8000	/* Ticket generated at activation by keyboard (scroll/num/caps) */
#define	TICKET_CTR_MASK		0x7fff	/* Mask for useCtr value (except HID flag) */

/* Configuration structure */

#define	FIXED_SIZE		16	/* Max size of fixed field */
#define	KEY_SIZE		16	/* Size of AES key */
#define	KEY_SIZE_OATH		20      /* Size of OATH-HOTP key (key field + first 4 of UID field) */
#define	ACC_CODE_SIZE		6	/* Size of access code to re-program device */

struct config_st {
	unsigned char fixed[FIXED_SIZE];/* Fixed data in binary format */
	unsigned char uid[UID_SIZE];	/* Fixed UID part of ticket */
	unsigned char key[KEY_SIZE];	/* AES key */
	unsigned char accCode[ACC_CODE_SIZE]; /* Access code to re-program device */
	unsigned char fixedSize;	/* Number of bytes in fixed field (0 if not used) */
	unsigned char extFlags;		/* Extended flags - YubiKey 2.? and above */
	unsigned char tktFlags;		/* Ticket configuration flags */
	unsigned char cfgFlags;		/* General configuration flags */
	unsigned char rfu[2];		/* Reserved for future use */
	unsigned short crc;		/* CRC16 value of all fields */
};

/* Ticket flags **************************************************************/

/* Yubikey 1 and above */
#define	TKTFLAG_TAB_FIRST	0x01	/* Send TAB before first part */
#define	TKTFLAG_APPEND_TAB1	0x02	/* Send TAB after first part */
#define	TKTFLAG_APPEND_TAB2	0x04	/* Send TAB after second part */
#define	TKTFLAG_APPEND_DELAY1	0x08	/* Add 0.5s delay after first part */
#define	TKTFLAG_APPEND_DELAY2	0x10	/* Add 0.5s delay after second part */
#define	TKTFLAG_APPEND_CR	0x20	/* Append CR as final character */

/* Yubikey 2 and above */
#define TKTFLAG_PROTECT_CFG2	0x80	/* Block update of config 2 unless config 2 is configured and has this bit set */

/* Configuration flags *******************************************************/

/* Yubikey 1 and above */
#define CFGFLAG_SEND_REF	0x01	/* Send reference string (0..F) before data */
#define CFGFLAG_PACING_10MS	0x04	/* Add 10ms intra-key pacing */
#define CFGFLAG_PACING_20MS	0x08	/* Add 20ms intra-key pacing */
#define CFGFLAG_STATIC_TICKET	0x20	/* Static ticket generation */

/* Yubikey 1 only */
#define	CFGFLAG_TICKET_FIRST	0x02	/* Send ticket first (default is fixed part) */
#define CFGFLAG_ALLOW_HIDTRIG	0x10	/* Allow trigger through HID/keyboard */

/* Yubikey 2 and above */
#define CFGFLAG_SHORT_TICKET	0x02	/* Send truncated ticket (half length) */
#define CFGFLAG_STRONG_PW1	0x10	/* Strong password policy flag #1 (mixed case) */
#define CFGFLAG_STRONG_PW2	0x40	/* Strong password policy flag #2 (subtitute 0..7 to digits) */
#define CFGFLAG_MAN_UPDATE	0x80	/* Allow manual (local) update of static OTP */

/* Yubikey 2.1 and above */
#define TKTFLAG_OATH_HOTP		0x40	/*  OATH HOTP mode */
#define CFGFLAG_OATH_HOTP8		0x02	/*  Generate 8 digits HOTP rather than 6 digits */
#define CFGFLAG_OATH_FIXED_MODHEX1	0x10	/*  First byte in fixed part sent as modhex */
#define CFGFLAG_OATH_FIXED_MODHEX2	0x40	/*  First two bytes in fixed part sent as modhex */
#define CFGFLAG_OATH_FIXED_MODHEX	0x50	/*  Fixed part sent as modhex */
#define CFGFLAG_OATH_FIXED_MASK		0x50	/*  Mask to get out fixed flags */

/* Yubikey 2.2 and above */

#define TKTFLAG_CHAL_RESP		0x40	/* Challenge-response enabled (both must be set) */
#define CFGFLAG_CHAL_YUBICO		0x20	/* Challenge-response enabled - Yubico OTP mode */
#define CFGFLAG_CHAL_HMAC		0x22	/* Challenge-response enabled - HMAC-SHA1 */
#define CFGFLAG_HMAC_LT64		0x04	/* Set when HMAC message is less than 64 bytes */
#define CFGFLAG_CHAL_BTN_TRIG		0x08	/* Challenge-response operation requires button press */

#define EXTFLAG_SERIAL_BTN_VISIBLE	0x01	/* Serial number visible at startup (button press) */
#define EXTFLAG_SERIAL_USB_VISIBLE	0x02	/* Serial number visible in USB iSerial field */
#define EXTFLAG_SERIAL_API_VISIBLE	0x04	/* Serial number visible via API call */

/* V2.3 flags only */

#define EXTFLAG_USE_NUMERIC_KEYPAD	0x08	/* Use numeric keypad for digits */
#define EXTFLAG_FAST_TRIG		0x10	/* Use fast trig if only cfg1 set */
#define EXTFLAG_ALLOW_UPDATE		0x20	/* Allow update of existing configuration (selected flags + access code) */
#define EXTFLAG_DORMANT			0x40	/* Dormant configuration (can be woken up and flag removed = requires update flag) */

/* V2.4/3.1 flags only */

#define EXTFLAG_LED_INV             0x80        /* LED idle state is off rather than on */

/* Flags valid for update */

#define TKTFLAG_UPDATE_MASK         (TKTFLAG_TAB_FIRST | TKTFLAG_APPEND_TAB1 | TKTFLAG_APPEND_TAB2 | TKTFLAG_APPEND_DELAY1 | TKTFLAG_APPEND_DELAY2 | TKTFLAG_APPEND_CR)
#define CFGFLAG_UPDATE_MASK         (CFGFLAG_PACING_10MS | CFGFLAG_PACING_20MS)
#define EXTFLAG_UPDATE_MASK         (EXTFLAG_SERIAL_BTN_VISIBLE | EXTFLAG_SERIAL_USB_VISIBLE |  EXTFLAG_SERIAL_API_VISIBLE | EXTFLAG_USE_NUMERIC_KEYPAD | EXTFLAG_FAST_TRIG | EXTFLAG_ALLOW_UPDATE | EXTFLAG_DORMANT | EXTFLAG_LED_INV)

/* NDEF structure */
#define	NDEF_DATA_SIZE			54

/* backwards compatibility with version 1.7.0  */
typedef struct ndef_st YKNDEF;

struct ndef_st {
	unsigned char len;				/* Payload length */
	unsigned char type;				/* NDEF type specifier */
	unsigned char data[NDEF_DATA_SIZE];		/* Payload size */
	unsigned char curAccCode[ACC_CODE_SIZE];	/* Access code */
};


/* Navigation */

/* NOTE: Navigation isn't available since Yubikey 1.3.5 and is strongly
   discouraged. */
#define	MAX_URL			48

struct nav_st {
	unsigned char scancode[MAX_URL];/* Scancode (lower 7 bits) */
	unsigned char scanmod[MAX_URL >> 2];	/* Modifier fields (packed 2 bits each) */
	unsigned char flags;		/* NAVFLAG_xxx flags */
	unsigned char filler;		/* Filler byte */
	unsigned short crc;		/* CRC16 value of all fields */
};

#define	SCANMOD_SHIFT		0x80	/* Highest bit in scancode */
#define	SCANMOD_ALT_GR		0x01	/* Lowest bit in mod */
#define	SCANMOD_WIN		0x02	/* WIN key */

/* Navigation flags */

#define	NAVFLAG_INSERT_TRIG	0x01	/* Automatic trigger when device is inserted */
#define NAVFLAG_APPEND_TKT	0x02	/* Append ticket to URL */
#define	NAVFLAG_DUAL_KEY_USAGE	0x04	/* Dual usage of key: Short = ticket  Long = Navigate */

/* Device configuration block (version 3.0) */

struct device_config_st {
	unsigned char mode;		/* Device mode */
	unsigned char crTimeout;	/* Challenge-response timeout in seconds */
	unsigned short autoEjectTime;	/* Auto eject time in x10 seconds */
};

#define MODE_OTP		0x00	/* OTP only */
#define MODE_CCID		0x01	/* CCID only, no eject */
#define MODE_OTP_CCID		0x02	/* OTP + CCID composite */
#define MODE_U2F		0x03	/* U2F mode */
#define MODE_OTP_U2F		0x04	/* OTP + U2F composite */
#define MODE_U2F_CCID		0x05	/* U2F + CCID composite */
#define MODE_OTP_U2F_CCID	0x06	/* OTP + U2F + CCID composite */
#define MODE_MASK		0x07	/* Mask for mode bits */

#define MODE_FLAG_EJECT		0x80	/* CCID device supports eject (mode 1 only) */

#define DEFAULT_CHAL_TIMEOUT	15	/* Default challenge timeout in seconds */

/* Scancode mapping (version 3.0) */

#define SCAN_MAP		"cbdefghijklnrtuvCBDEFGHIJKLNRTUV0123456789!\t\r"
#define SHIFT_FLAG		0x80	/* Flag for shifted scan codes */

/* Status block */

struct status_st {
	unsigned char versionMajor;	/* Firmware version information */
	unsigned char versionMinor;
	unsigned char versionBuild;
	unsigned char pgmSeq;		/* Programming sequence number. 0 if no valid configuration */
	unsigned short touchLevel;	/* Level from touch detector */
};

#define CONFIG1_VALID               0x01        /* Bit in touchLevel indicating that configuration 1 is valid (from firmware 2.1) */
#define CONFIG2_VALID               0x02        /* Bit in touchLevel indicating that configuration 2 is valid (from firmware 2.1) */
#define CONFIG1_TOUCH               0x04        /* Bit in touchLevel indicating that configuration 1 requires touch (from firmware 3.0) */
#define CONFIG2_TOUCH               0x08        /* Bit in touchLevel indicating that configuration 2 requires touch (from firmware 3.0) */
#define CONFIG_LED_INV              0x10        /* Bit in touchLevel indicating that LED behavior is inverted (EXTFLAG_LED_INV mirror) */
#define CONFIG_STATUS_MASK          0x1f        /* Mask for status bits */

/* Modified hex string mapping */

#define	MODHEX_MAP		"cbdefghijklnrtuv"

/* USB vendor ID (VID) and product ID (PID) mapping */

#define	YUBICO_VID		0x1050	/* Global vendor ID */
#define	YUBIKEY_PID		0x0010	/* Yubikey (version 1 and 2) */
#define	NEO_OTP_PID		0x0110	/* Yubikey NEO - OTP only */
#define	NEO_OTP_CCID_PID	0x0111	/* Yubikey NEO - OTP and CCID */
#define	NEO_CCID_PID		0x0112	/* Yubikey NEO - CCID only */
#define	NEO_U2F_PID		0x0113	/* Yubikey NEO - U2F only */
#define	NEO_OTP_U2F_PID		0x0114	/* Yubikey NEO - OTP and U2F */
#define	NEO_U2F_CCID_PID	0x0115	/* Yubikey NEO - U2F and CCID */
#define	NEO_OTP_U2F_CCID_PID	0x0116	/* Yubikey NEO - OTP, U2F and CCID */

#define	YK4_OTP_PID		0x0401	/* Yubikey 4 - OTP only */
#define	YK4_U2F_PID		0x0402	/* Yubikey 4 - U2F only */
#define	YK4_OTP_U2F_PID		0x0403	/* Yubikey 4 - OTP and U2F */
#define	YK4_CCID_PID		0x0404	/* Yubikey 4 - CCID only */
#define	YK4_OTP_CCID_PID	0x0405	/* Yubikey 4 - OTP and CCID */
#define	YK4_U2F_CCID_PID	0x0406	/* Yubikey 4 - U2F and CCID */
#define	YK4_OTP_U2F_CCID_PID	0x0407	/* Yubikey 4 - OTP, U2F and CCID */

#define	PLUS_U2F_OTP_PID	0x0410	/* Yubikey plus - OTP+U2F */

#define ONLYKEY_VID     0x1d50
#define ONLYKEY_PID     0x60fc

#define YK4_CAPA_TAG		0x01	/* TAG for capabilities */
#define YK4_SERIAL_TAG		0x02	/* TAG for serial number */

#define YK4_CAPA1_OTP		0x01	/* Capability bit for OTP functonality */
#define YK4_CAPA1_U2F		0x02	/* Capability bit for U2F functionality */
#define YK4_CAPA1_CCID		0x04	/* Capability bit for CCID functionality */
#define YK4_CAPA1_OPGP		0x08	/* Capability bit for OpenPGP functionality */
#define YK4_CAPA1_PIV		0x10	/* Capability bit for PIV functionality */
#define YK4_CAPA1_OATH		0x20	/* Capability bit for OATH functionality */

#if defined(_WIN32) || defined(__GNUC__)
#pragma pack(pop)
#endif

#endif	/* __YKDEF_H_INCLUDED__ */
