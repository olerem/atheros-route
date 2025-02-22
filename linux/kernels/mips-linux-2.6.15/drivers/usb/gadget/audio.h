/*-------------------------------------------------------------------------*/

#define DRIVER_VENDOR_NUM	0x1d6b		/* NetChip */
#define DRIVER_PRODUCT_NUM	0x0101		/* Linux-USB "Gadget Zero" */


#define STRING_MANUFACTURER		0x1
#define STRING_PRODUCT			0x2
#define STRING_SERIAL			0x3
#define STRING_AUDIO			0x4


/*To be put into a seperate header file*/

#define USB_AUDIO_SUBCLASS_CONTROL 	0x1
#define USB_AUDIO_SUBCLASS_STREAMING 	0x2

#define AS_GENERAL			0x01
#define FORMAT_TYPE			0x02
#define FORMAT_TYPE_PCM			0x01
#define FORMAT_TYPE_PCM8		0x02

/* A.2 Audio Interface Subclass Codes */
#define USB_SUBCLASS_AUDIOCONTROL       0x01
#define USB_SUBCLASS_AUDIOSTREAMING     0x02
#define USB_SUBCLASS_MIDISTREAMING      0x03

/* A.5 Audio Class-Specific AC Interface Descriptor Subtypes */
#define UAC_HEADER                      0x01
#define UAC_INPUT_TERMINAL              0x02
#define UAC_OUTPUT_TERMINAL             0x03
#define UAC_MIXER_UNIT                  0x04
#define UAC_SELECTOR_UNIT               0x05
#define UAC_FEATURE_UNIT                0x06
#define UAC_PROCESSING_UNIT             0x07
#define UAC_EXTENSION_UNIT              0x08

/* A.6 Audio Class-Specific AS Interface Descriptor Subtypes */
#define UAC_AS_GENERAL                  0x01
#define UAC_FORMAT_TYPE                 0x02
#define UAC_FORMAT_SPECIFIC             0x03

/* A.8 Audio Class-Specific Endpoint Descriptor Subtypes */
#define UAC_EP_GENERAL                  0x01

#define UAC_TERMINAL_STREAMING          0x101


/* A.9 Audio Class-Specific Request Codes */
#define UAC_SET_                        0x00
#define UAC_GET_                        0x80

#define UAC__CUR                        0x1
#define UAC__MIN                        0x2
#define UAC__MAX                        0x3
#define UAC__RES                        0x4
#define UAC__MEM                        0x5

#define UAC_SET_CUR                     (UAC_SET_ | UAC__CUR)
#define UAC_GET_CUR                     (UAC_GET_ | UAC__CUR)
#define UAC_SET_MIN                     (UAC_SET_ | UAC__MIN)
#define UAC_GET_MIN                     (UAC_GET_ | UAC__MIN)
#define UAC_SET_MAX                     (UAC_SET_ | UAC__MAX)
#define UAC_GET_MAX                     (UAC_GET_ | UAC__MAX)
#define UAC_SET_RES                     (UAC_SET_ | UAC__RES)
#define UAC_GET_RES                     (UAC_GET_ | UAC__RES)
#define UAC_SET_MEM                     (UAC_SET_ | UAC__MEM)
#define UAC_GET_MEM                     (UAC_GET_ | UAC__MEM)

#define UAC_GET_STAT                    0xff

struct usb_as_interface_desc {
	__u8	bLength;
	__u8	bDescriptorType;
	__u8	bDescriptorSubType;

	__u8	bTerminalLink;
	__u8	bDelay;
	__le16	wFormatTag;
} __attribute__ ((packed));

struct usb_ac_interface_desc {
        __u8  bLength;                  /* 8 + n */
        __u8  bDescriptorType;          /* USB_DT_CS_INTERFACE */
        __u8  bDescriptorSubtype;       /* UAC_MS_HEADER */
        __le16 bcdADC;                  /* 0x0100 */
        __le16 wTotalLength;            /* includes Unit and Terminal desc. */
        __u8  bInCollection;            /* n */
        __u8  baInterfaceNr[];          /* [n] */
} __attribute__ ((packed));



#define DECLARE_UAC_FORMAT_TYPE_I_DISCRETE_DESC(n)              \
struct uac_as_format_desc_##n {       		       	\
        __u8  bLength;                                          \
        __u8  bDescriptorType;                                  \
        __u8  bDescriptorSubType;                               \
        __u8  bFormatType;                                      \
        __u8  bNrChannels;                                      \
        __u8  bSubFrameSize;                                    \
        __u8  bBitResolution;                                   \
        __u8  bSamFreqType;                                     \
        __u8  tSamFreq[n][3];                                   \
} __attribute__ ((packed))

#define UAC_FORMAT_TYPE_I_DISCRETE_DESC_SIZE(n) (8 + (n * 3))


/* 4.3.2.1 Input Terminal Descriptor */
struct uac_input_terminal_descriptor {
        __u8  bLength;                  /* in bytes: 12 */
        __u8  bDescriptorType;          /* CS_INTERFACE descriptor type */
        __u8  bDescriptorSubtype;       /* INPUT_TERMINAL descriptor subtype */
        __u8  bTerminalID;              /* Constant uniquely terminal ID */
        __le16 wTerminalType;           /* USB Audio Terminal Types */
        __u8  bAssocTerminal;           /* ID of the Output Terminal associated */
        __u8  bNrChannels;              /* Number of logical output channels */
        __le16 wChannelConfig;
        __u8  iChannelNames;
        __u8  iTerminal;
} __attribute__ ((packed));

#define UAC_DT_INPUT_TERMINAL_SIZE                      12

/* Terminals - 2.2 Input Terminal Types */
#define UAC_INPUT_TERMINAL_UNDEFINED                    0x200
#define UAC_INPUT_TERMINAL_MICROPHONE                   0x201
#define UAC_INPUT_TERMINAL_DESKTOP_MICROPHONE           0x202
#define UAC_INPUT_TERMINAL_PERSONAL_MICROPHONE          0x203
#define UAC_INPUT_TERMINAL_OMNI_DIR_MICROPHONE          0x204
#define UAC_INPUT_TERMINAL_MICROPHONE_ARRAY             0x205
#define UAC_INPUT_TERMINAL_PROC_MICROPHONE_ARRAY        0x206

/* 4.3.2.2 Output Terminal Descriptor */
struct uac_output_terminal_descriptor {
        __u8  bLength;                  /* in bytes: 9 */
        __u8  bDescriptorType;          /* CS_INTERFACE descriptor type */
        __u8  bDescriptorSubtype;       /* OUTPUT_TERMINAL descriptor subtype */
        __u8  bTerminalID;              /* Constant uniquely terminal ID */
        __le16 wTerminalType;           /* USB Audio Terminal Types */
        __u8  bAssocTerminal;           /* ID of the Input Terminal associated */
        __u8  bSourceID;                /* ID of the connected Unit or Terminal*/
        __u8  iTerminal;
} __attribute__ ((packed));

#define UAC_DT_OUTPUT_TERMINAL_SIZE                     9

/* Terminals - 2.3 Output Terminal Types */
#define UAC_OUTPUT_TERMINAL_UNDEFINED                   0x300
#define UAC_OUTPUT_TERMINAL_SPEAKER                     0x301
#define UAC_OUTPUT_TERMINAL_HEADPHONES                  0x302
#define UAC_OUTPUT_TERMINAL_HEAD_MOUNTED_DISPLAY_AUDIO  0x303
#define UAC_OUTPUT_TERMINAL_DESKTOP_SPEAKER             0x304
#define UAC_OUTPUT_TERMINAL_ROOM_SPEAKER                0x305
#define UAC_OUTPUT_TERMINAL_COMMUNICATION_SPEAKER       0x306
#define UAC_OUTPUT_TERMINAL_LOW_FREQ_EFFECTS_SPEAKER    0x307

/* Set bControlSize = 2 as default setting */
#define UAC_DT_FEATURE_UNIT_SIZE(ch)            (7 + ((ch) + 1) * 2)

/* As above, but more useful for defining your own descriptors: */
#define DECLARE_UAC_FEATURE_UNIT_DESCRIPTOR(ch)                 \
struct uac_feature_unit_descriptor_##ch {                       \
        __u8  bLength;                                          \
        __u8  bDescriptorType;                                  \
        __u8  bDescriptorSubtype;                               \
        __u8  bUnitID;                                          \
        __u8  bSourceID;                                        \
        __u8  bControlSize;                                     \
        __le16 bmaControls[ch + 1];                             \
        __u8  iFeature;                                         \
} __attribute__ ((packed))


/* Formats - A.2 Format Type Codes */
#define UAC_FORMAT_TYPE_UNDEFINED       0x0
#define UAC_FORMAT_TYPE_I               0x1
#define UAC_FORMAT_TYPE_II              0x2
#define UAC_FORMAT_TYPE_III             0x3

struct uac_iso_endpoint_descriptor {
        __u8  bLength;                  /* in bytes: 7 */
        __u8  bDescriptorType;          /* USB_DT_CS_ENDPOINT */
        __u8  bDescriptorSubtype;       /* EP_GENERAL */
        __u8  bmAttributes;
        __u8  bLockDelayUnits;
        __le16 wLockDelay;
};
#define UAC_ISO_ENDPOINT_DESC_SIZE      7

#define UAC_EP_CS_ATTR_SAMPLE_RATE      0x01
#define UAC_EP_CS_ATTR_PITCH_CONTROL    0x02
#define UAC_EP_CS_ATTR_FILL_MAX         0x80

#define UAC_DT_AC_HEADER_SIZE(n)        (8 + (n))
#define USB_ENDPOINT_SYNC_ADAPTIVE	(2 << 2)
#define USB_ENDPOINT_SYNC_ASYNCRONOUS	(1 << 2)
#define USB_ENDPOINT_SYNC_SYNCRONOUS	(3 << 2)



/* A.10.2 Feature Unit Control Selectors */
#define UAC_FU_CONTROL_UNDEFINED	0x00
#define UAC_MUTE_CONTROL		0x01
#define UAC_VOLUME_CONTROL		0x02
#define UAC_BASS_CONTROL		0x03
#define UAC_MID_CONTROL			0x04
#define UAC_TREBLE_CONTROL		0x05
#define UAC_GRAPHIC_EQUALIZER_CONTROL	0x06
#define UAC_AUTOMATIC_GAIN_CONTROL	0x07
#define UAC_DELAY_CONTROL		0x08
#define UAC_BASS_BOOST_CONTROL		0x09
#define UAC_LOUDNESS_CONTROL		0x0a

#define UAC_FU_MUTE		(1 << (UAC_MUTE_CONTROL - 1))
#define UAC_FU_VOLUME		(1 << (UAC_VOLUME_CONTROL - 1))
#define UAC_FU_BASS		(1 << (UAC_BASS_CONTROL - 1))
#define UAC_FU_MID		(1 << (UAC_MID_CONTROL - 1))
#define UAC_FU_TREBLE		(1 << (UAC_TREBLE_CONTROL - 1))
#define UAC_FU_GRAPHIC_EQ	(1 << (UAC_GRAPHIC_EQUALIZER_CONTROL - 1))
#define UAC_FU_AUTO_GAIN	(1 << (UAC_AUTOMATIC_GAIN_CONTROL - 1))
#define UAC_FU_DELAY		(1 << (UAC_DELAY_CONTROL - 1))
#define UAC_FU_BASS_BOOST	(1 << (UAC_BASS_BOOST_CONTROL - 1))
#define UAC_FU_LOUDNESS		(1 << (UAC_LOUDNESS_CONTROL - 1))

extern void ar7240_i2s_clk(unsigned long, unsigned long);
extern int  ar7242_i2s_open(void);
extern void ar7242_i2s_close(void);
extern void ar7242_i2s_write(size_t , const char *, int );
extern void ar7240_i2sound_dma_start(int);


