#ifndef OPTION_STK_H
#define OPTION_STK_H 1

#ifdef __cplusplus
extern "C" {
#endif

// First byte
#define STPD_DOWNLOAD       0x01
#define STPD_SMS_PP_DATA    0x02
#define STPD_CELL_BROADCAST 0x04
#define STPD_MENU_SELECTION 0x08
// RFU, bit = 0, 4 bits

// Second byte
#define STPD_COMMAND_RESULT 0x01
#define STPD_CALL_CONTROL   0x02
// RFU, bit = 0, 6 bits

// Third byte
#define STPD_DISPLAY_TEXT   0x01
#define STPD_GET_INKEY      0x02
#define STPD_GET_INPUT      0x04
#define STPD_MORE_TIME      0x08
#define STPD_PLAY_TONE      0x10
#define STPD_POLL_INTERVAL  0x20
#define STPD_POLLING_OFF    0x40
#define STPD_REFRESH        0x80

// Fourth byte
#define STPD_SELECT_ITEM    0x01
#define STPD_SEND_SMS       0x02
#define STPD_SEND_SS        0x04
#define STPD_SEND_USSD      0x08
#define STPD_SET_UP_CALL    0x10
#define STPD_SET_UP_MENU    0x20
#define STPD_LOCAL_INFO     0x40
// RFU, bit = 0, 1 bit

typedef union {
    struct {
        char fourth_byte;
        char third_byte;
        char second_byte;
        char first_byte;
    } bytes;
    int profile;
} STKProfile;

#define STK_CMD_REFRESH                 0x01
#define STK_CMD_MORE_TIME               0x02
#define STK_CMD_POLL_INTERVAL           0x03
#define STK_CMD_POLLING_OFF             0x04
#define STK_CMD_SET_UP_EVENT_LIST       0x05

#define STK_CMD_SET_UP_CALL             0x10
#define STK_CMD_SEND_SS                 0x11
#define STK_CMD_SEND_USSD               0x12
#define STK_CMD_SEND_SMS                0x13
#define STK_CMD_SEND_DTMF               0x14
#define STK_CMD_LAUNCH_BROWSER          0x15

#define STK_CMD_PLAY_TONE               0x20
#define STK_CMD_DISPLAY_TEXT            0x21
#define STK_CMD_GET_INKEY               0x22
#define STK_CMD_GET_INPUT               0x23
#define STK_CMD_SELECT_ITEM             0x24
#define STK_CMD_SET_UP_MENU             0x25
#define STK_CMD_PROVIDE_LOCAL_INFO      0x26

// From Mauro doc:
#define STK_CMD_SET_UP_IDLE_TEXT        0x28
#define STK_CMD_RUN_AT_COMMAND          0x34
#define STK_CMD_LANGUAGE_NOTIFICATION   0x35
#define STK_CMD_OPEN_CHANNEL            0x40
#define STK_CMD_CLOSE_CHANNEL           0x41
#define STK_CMD_RECEIVE_DATA            0x42
#define STK_CMD_SEND_DATA               0x43

#define STK_CMD_SESSION_END             0x81


#define STK_TAG_COMMAND_DETAILS         0x01
#define STK_TAG_DEVICE_IDENTITIES       0x02
#define STK_TAG_RESULT                  0x03
#define STK_TAG_DURATION                0x04
#define STK_TAG_ALPHA_IDENTIFIER        0x05
#define STK_TAG_ADDRESS                 0x06
#define STK_TAG_CAPABILITY_CONF         0x07
#define STK_TAG_CALLED_PARTY_SUBADDRESS 0x08
#define STK_TAG_SS_STRING               0x09
#define STK_TAG_USSD_STRING             0x0A
#define STK_TAG_SMS_TPDU                0x0B
#define STK_TAG_CELL_BROADCAST_PAGE     0x0C
#define STK_TAG_TEXT_STRING             0x0D
#define STK_TAG_TONE                    0x0E
#define STK_TAG_ITEM                    0x0F
#define STK_TAG_ITEM_IDENTIFIER         0x10
#define STK_TAG_RESPONSE_LENGTH         0x11
#define STK_TAG_LOCATION_INFORMATION    0x15
#define STK_TAG_DEFAULT_TEXT            0x17
#define STK_TAG_ICON_IDENTIFIER         0x1E

#define STK_TAG_PROACTIVE_COMMAND       0xD0
#define STK_TAG_SMS_PP_DOWNLOAD         0xD1
#define STK_TAG_CELL_DOWNLOAD           0xD2
#define STK_TAG_MENU_SELECTION          0xD3
#define STK_TAG_CALL_CONTROL            0xD4

#define STK_TLV_NEEDS_COMPREHENSION     0x80
#define STK_TLV_FIXED_DIRECTION         0xD0


#define STK_DEVID_KEYPAD    "01"
#define STK_DEVID_DISPLAY   "02"
#define STK_DEVID_EARPIECE  "03"
#define STK_DEVID_SIM       "81"
#define STK_DEVID_ME        "82"
#define STK_DEVID_NETWORK   "83"

struct _TlvValue {
    const char *value;
    struct _TlvValue *next;
};
typedef struct _TlvValue TlvValue;

struct _SimpleTlv {
    char tag[2];
    int length; // length needed to allocate the final tlv
    TlvValue *first;
    TlvValue *head;
    struct _SimpleTlv *next;
};
typedef struct _SimpleTlv SimpleTlv;

typedef struct {
    char tag[2];
    int length; // length needed to allocate the final tlv
    SimpleTlv *first;
    SimpleTlv *head;
} Ber_Tlv;

void simpletlv_add_value(SimpleTlv *tlv, char *value);
void simpletlv_free(SimpleTlv *simpletlv);

void bertlv_add_tlv(Ber_Tlv *bertlv, SimpleTlv *tlv);
void bertlv_free(Ber_Tlv *bertlv);
char *bertlv_to_string(Ber_Tlv *bertlv);

char *stk_at_to_hex(ATResponse *p_response);
char *hex_to_stk_at(const char *at);

#ifdef __cplusplus
}
#endif

#endif
