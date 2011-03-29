#include "at_tok.h"
#include "atchannel.h"
#include "stk.h"
#include <string.h>
#include <stdio.h>
#include <alloca.h>

#define LOG_TAG "RIL"
#include <utils/Log.h>

// In some Mauro firmwares there are differences on the returned strings
#define at_tok_try_to_start(line) if (strchr(line, ':')) at_tok_start(&line)

/*
 * Command number
 */
int stk_command_number = 0;

int new_command_number() {
    if (stk_command_number <= 0xFF) {
        stk_command_number++;
    } else {
        stk_command_number = 0x01;
    }
    return stk_command_number;
}

int current_command_number() {
    return stk_command_number;
}

char *command_number(char *number_p) {
    sprintf(number_p, "%02X", new_command_number());
    LOGD("Command number %s", number_p);
    return number_p;
}

#define STK_COMMAND_NUMBER command_number(alloca(3*sizeof(char)))

/*
 *  Helper Functions
 */
char *append(char *dest, const char *src) {
    realloc(dest, sizeof(char) * (strlen(dest) + strlen(src)));
    return strcat(dest, src);
}

TlvValue *create_tlv_value(const char *value) {
    TlvValue *tlv;
    tlv = malloc(sizeof(TlvValue));
    tlv->value = value;
    tlv->next = NULL;
    return tlv;
}

SimpleTlv *create_simple_tlv(int tag) {
    SimpleTlv *tlv;
    tlv = malloc(sizeof(SimpleTlv));
    sprintf(tlv->tag, "%02X", tag);
    tlv->first = NULL;
    tlv->head = NULL;
    tlv->next = NULL;
    tlv->length = 0;
    return tlv;
}

Ber_Tlv *create_ber_tlv(int tag) {
    Ber_Tlv *tlv;
    tlv = malloc(sizeof(Ber_Tlv));
    sprintf(tlv->tag, "%02X", tag);
    tlv->length = 1;
    tlv->first = NULL;
    tlv->head = NULL;
    return tlv;
}

void bertlv_add_tlv(Ber_Tlv *bertlv, SimpleTlv *tlv) {
    tlv->next = NULL;
    if (bertlv->first) {
        bertlv->head->next = tlv;
    } else {
        bertlv->first = tlv;
    }
    bertlv->head = tlv;
    bertlv->length += (2 + 2 + tlv->length); // Tag + Length + Value
}

void simpletlv_add_value(SimpleTlv *tlv, char *value) {
    int value_len;
    TlvValue *new_tlv = create_tlv_value(value);
    if (tlv->first) {
        tlv->head->next = new_tlv;
    } else {
        tlv->first = new_tlv;
    }
    tlv->head = new_tlv;
    tlv->length += strlen(value);
    if(strlen(value) % 2) tlv->length++; // We'll need a prefixed 0
}

void simpletlv_free(SimpleTlv *simpletlv) {
    TlvValue *value, *next_value;
    for (value = simpletlv->first; value; value = next_value) {
        next_value = value->next;
        free(value);
    }
    free(simpletlv);
}

void bertlv_free(Ber_Tlv *bertlv) {
    SimpleTlv *tlv, *next_tlv;
    for (tlv = bertlv->first; tlv; tlv = next_tlv) {
        next_tlv = tlv->next;
        simpletlv_free(tlv);
    }
    free(bertlv);
}

char *bertlv_to_string(Ber_Tlv *bertlv) {
    char *result, *p;
    SimpleTlv *tlv;
    TlvValue *value;
    int bertlv_length, length_length;

    bertlv_length = (bertlv->length / 2);
    length_length = (bertlv_length < 128)? 1 : 2;

    result = (char *)malloc(
        sizeof(char) * (2 + (2 * length_length) + bertlv->length));
    p = result;

    // Tag
    strcpy(p, bertlv->tag); p += 2;

    // Length
    if (length_length == 2) {
        sprintf(p, "81"); p += 2;
    }
    sprintf(p, "%02X", bertlv_length); p += 2;

    // Value
    for (tlv = bertlv->first; tlv; tlv = tlv->next) {
        strcpy(p, tlv->tag); p += 2;
        sprintf(p, "%02X", tlv->length / 2); p += 2;
        for (value = tlv->first; value; value = value->next) {
            if (strlen(value->value) % 2) {
                sprintf(p, "0%s", value->value); p += strlen(value->value) + 1;
            } else {
                sprintf(p, value->value); p += strlen(value->value);
            }
        }
    }

    return result;
}
/*
 * SET_UP_CALL bertlv
 */
char *build_set_up_call(char *line, ATLine *lines) {
    char *alphaid;
    int qualifier_flags = 0;
    char qualifier[2];
    Ber_Tlv *bertlv = NULL;
    SimpleTlv *simpletlv = NULL;
    char *result;

    // Params reading
    if(at_tok_nextstr(&line, &alphaid) < 0) goto invalid;

    bertlv = create_ber_tlv(STK_TAG_PROACTIVE_COMMAND);

    // Command details
    simpletlv = create_simple_tlv(STK_TAG_COMMAND_DETAILS |
        STK_TLV_NEEDS_COMPREHENSION);
    // Command number (01..FF), in theory specified by SIM
    simpletlv_add_value(simpletlv, STK_COMMAND_NUMBER);
    simpletlv_add_value(simpletlv, "10" "00");
    bertlv_add_tlv(bertlv, simpletlv);

    // Device identities
    simpletlv = create_simple_tlv(STK_TAG_DEVICE_IDENTITIES);
    simpletlv_add_value(simpletlv, STK_DEVID_SIM STK_DEVID_NETWORK);
    bertlv_add_tlv(bertlv, simpletlv);

    // Alphaid string
    simpletlv = create_simple_tlv(STK_TAG_ALPHA_IDENTIFIER);
    simpletlv_add_value(simpletlv, alphaid);
    bertlv_add_tlv(bertlv, simpletlv);

    result = bertlv_to_string(bertlv);
    bertlv_free(bertlv);
    return result;
invalid:
    if (bertlv) bertlv_free(bertlv);
    return NULL;
}

/*
 * MORE_TIME bertlv
 */
char *build_more_time(char *line, ATLine *lines) {
    Ber_Tlv *bertlv = NULL;
    SimpleTlv *simpletlv = NULL;
    char *result;

    bertlv = create_ber_tlv(STK_TAG_PROACTIVE_COMMAND);

    // Command details
    simpletlv = create_simple_tlv(STK_TAG_COMMAND_DETAILS |
        STK_TLV_NEEDS_COMPREHENSION);
    // Command number (01..FF), in theory specified by SIM
    simpletlv_add_value(simpletlv, STK_COMMAND_NUMBER);
    simpletlv_add_value(simpletlv, "02" "00");
    bertlv_add_tlv(bertlv, simpletlv);

    // Device identities
    simpletlv = create_simple_tlv(STK_TAG_DEVICE_IDENTITIES);
    simpletlv_add_value(simpletlv, STK_DEVID_SIM STK_DEVID_ME);
    bertlv_add_tlv(bertlv, simpletlv);

    result = bertlv_to_string(bertlv);
    bertlv_free(bertlv);
    return result;
}

/*
 * PLAY_TONE bertlv
 */
char *build_play_tone(char *line, ATLine *lines) {
    char *alphaId;
    int tone;
    char tone_str[3];
    int unit;
    char unit_str[3];
    int duration;
    char duration_str[3];
    int iconId;
    char icon[3];
    Ber_Tlv *bertlv = NULL;
    SimpleTlv *simpletlv = NULL;
    char *result;

    // Params reading
    if (at_tok_nextint(&line, &tone) < 0) goto end;
    if (at_tok_nextint(&line, &unit) < 0) goto end;
    if (at_tok_nextint(&line, &duration) < 0) goto end;
    if (at_tok_nextstr(&line, &alphaId) < 0) goto end;
    if (at_tok_nextint(&line, &iconId) < 0) goto end;

    LOGD(" tone= %d\r\n", tone);
    LOGD(" unit= %d\r\n", unit);
    LOGD(" duration= %d\r\n", duration);
    LOGD(" alpha= %s\r\n", alphaId);
    LOGD(" icon= %d\r\n", iconId);

    bertlv = create_ber_tlv(STK_TAG_PROACTIVE_COMMAND);

    // Command details
    simpletlv = create_simple_tlv(STK_TAG_COMMAND_DETAILS |
        STK_TLV_NEEDS_COMPREHENSION);
    // Command number (01..FF), in theory specified by SIM
    simpletlv_add_value(simpletlv, STK_COMMAND_NUMBER);
//    simpletlv_add_value(simpletlv, "20" "00");
    simpletlv_add_value(simpletlv, "00" "00");
    bertlv_add_tlv(bertlv, simpletlv);

    // Device identities
    simpletlv = create_simple_tlv(STK_TAG_DEVICE_IDENTITIES);
    simpletlv_add_value(simpletlv, STK_DEVID_SIM STK_DEVID_EARPIECE);
    bertlv_add_tlv(bertlv, simpletlv);

    // Alpha identifier (optional)
    if (alphaId)
    {
        LOGD(" add alpha identifer tlv\r\n");
        simpletlv = create_simple_tlv(STK_TAG_ALPHA_IDENTIFIER);
        simpletlv_add_value(simpletlv, alphaId);
        bertlv_add_tlv(bertlv, simpletlv);
    }

    // Tone (optional)
    if (tone)
    {
        LOGD(" add tone tlv\r\n");
        simpletlv = create_simple_tlv(STK_TAG_TONE);
        sprintf(tone_str, "%02X", tone);
        simpletlv_add_value(simpletlv, tone_str);
        bertlv_add_tlv(bertlv, simpletlv);
    }

    // Duration (optional)
    if (duration)
    {
        LOGD(" add duration tlv\r\n");
        simpletlv = create_simple_tlv(STK_TAG_DURATION);
        sprintf(unit_str, "%02X", unit);
        simpletlv_add_value(simpletlv, unit_str);
        sprintf(duration_str, "%02X", duration);
        simpletlv_add_value(simpletlv, duration_str);
        bertlv_add_tlv(bertlv, simpletlv);
    }

    // Icon identifier (optional)
    if (iconId)
    {
        LOGD(" add icon identifier tlv\r\n");
        simpletlv = create_simple_tlv(STK_TAG_ICON_IDENTIFIER);
        sprintf(icon, "%02X", iconId);
        simpletlv_add_value(simpletlv, "00");
        simpletlv_add_value(simpletlv, icon);
        bertlv_add_tlv(bertlv, simpletlv);
    }

end:
    result = bertlv_to_string(bertlv);
    bertlv_free(bertlv);
    return result;
}

/*
 * DISPLAY_TEXT bertlv
 */
char *build_display_text(char *line, ATLine *lines) {
    char* dcs;
    char* text;
    char qualifier[3];
    int qualifier_flags;
    int iconId;
    char icon[3];
    Ber_Tlv* bertlv = NULL;
    SimpleTlv* simpletlv = NULL;
    char* result;

    LOGD("build_display_text() - Enter\r\n");

    // Params reading
    if(at_tok_nextint(&line, &qualifier_flags) < 0) goto invalid;
    if(at_tok_nextstr(&line, &dcs) < 0) goto invalid;
    if(at_tok_nextstr(&line, &text) < 0) goto invalid;
    if(at_tok_nextint(&line, &iconId) < 0) goto invalid;

    LOGD(" qualifier_flags= %d\r\n", qualifier_flags);
    LOGD(" dcs= %s\r\n", dcs);
    LOGD(" text= %s\r\n", text);
    LOGD(" icon id= %d\r\n", iconId);

    bertlv = create_ber_tlv(STK_TAG_PROACTIVE_COMMAND);

    // Command details
    simpletlv = create_simple_tlv(STK_TAG_COMMAND_DETAILS |
        STK_TLV_NEEDS_COMPREHENSION);
    // Command number (01..FF), in theory specified by SIM
    simpletlv_add_value(simpletlv, STK_COMMAND_NUMBER);
    simpletlv_add_value(simpletlv, "21");
    sprintf(qualifier, "%02X", qualifier_flags);
    simpletlv_add_value(simpletlv, qualifier);
    bertlv_add_tlv(bertlv, simpletlv);

    // Device identities
    simpletlv = create_simple_tlv(STK_TAG_DEVICE_IDENTITIES);
    simpletlv_add_value(simpletlv, STK_DEVID_SIM STK_DEVID_DISPLAY);
    bertlv_add_tlv(bertlv, simpletlv);

    // Text string
    simpletlv = create_simple_tlv(STK_TAG_TEXT_STRING);
    simpletlv_add_value(simpletlv, dcs);
    simpletlv_add_value(simpletlv, text);
    bertlv_add_tlv(bertlv, simpletlv);

    // Icon identifier (optional)
    if (iconId)
    {
        LOGD(" add icon identifer tlv\r\n");
        simpletlv = create_simple_tlv(STK_TAG_ICON_IDENTIFIER);
        sprintf(icon, "%02X", iconId);
        simpletlv_add_value(simpletlv, "00");
        simpletlv_add_value(simpletlv, icon);
        bertlv_add_tlv(bertlv, simpletlv);
    }

    result = bertlv_to_string(bertlv);
    bertlv_free(bertlv);

    LOGD("build_display_text() - Exit\r\n");
    return result;
invalid:
    if (bertlv) bertlv_free(bertlv);
    LOGD("build_display_text() - Exit, invalid\r\n");
    return NULL;
}

/*
 * GET_INKEY bertlv
 */
char *build_get_inkey(char *line, ATLine *lines) {
    char *dcs;
    char *text;
    int response;
    int helpInfo;
    int qualifier_flags = 0;
    char qualifier[3];
    Ber_Tlv *bertlv = NULL;
    SimpleTlv *simpletlv = NULL;
    char *result;

    // Params reading
    if(at_tok_nextstr(&line, &dcs) < 0) goto invalid;
    if(at_tok_nextstr(&line, &text) < 0) goto invalid;
    if(at_tok_nextint(&line, &response) < 0) goto invalid;
    if(at_tok_nextint(&line, &helpInfo) < 0) goto invalid;

    bertlv = create_ber_tlv(STK_TAG_PROACTIVE_COMMAND);

    // Command details
    simpletlv = create_simple_tlv(STK_TAG_COMMAND_DETAILS |
        STK_TLV_NEEDS_COMPREHENSION);
    // Command number (01..FF), in theory specified by SIM
    simpletlv_add_value(simpletlv, STK_COMMAND_NUMBER);
    simpletlv_add_value(simpletlv, "22");
    if (response > 0) qualifier_flags |= 0x01;
    sprintf(qualifier, "%02X", qualifier_flags);
    simpletlv_add_value(simpletlv, qualifier);
    bertlv_add_tlv(bertlv, simpletlv);

    // Device identities
    simpletlv = create_simple_tlv(STK_TAG_DEVICE_IDENTITIES);
    simpletlv_add_value(simpletlv, STK_DEVID_SIM STK_DEVID_ME);
    bertlv_add_tlv(bertlv, simpletlv);

    // Text string
    simpletlv = create_simple_tlv(STK_TAG_TEXT_STRING);
    simpletlv_add_value(simpletlv, dcs);
    simpletlv_add_value(simpletlv, text);
    bertlv_add_tlv(bertlv, simpletlv);

    result = bertlv_to_string(bertlv);
    bertlv_free(bertlv);
    return result;
invalid:
    if (bertlv) bertlv_free(bertlv);
    return NULL;
}

/*
 * GET_INPUT bertlv
 */
char *build_get_input(char *line, ATLine *lines) {
    char* dcs;
    char* text;
    int minLength;
    int maxLength;
    int iconId;
    char icon[3];
    char* defaultText;
    char minLength_str[3];
    char maxLength_str[3];
    int qualifier_flags = 0;
    char qualifier[3];
    Ber_Tlv* bertlv = NULL;
    SimpleTlv* simpletlv = NULL;
    char* result;

    LOGD("build_get_input() - Enter\r\n");

    // Params reading
    if(at_tok_nextint(&line, &qualifier_flags) < 0) goto invalid;
    if(at_tok_nextstr(&line, &dcs) < 0) goto invalid;
    if(at_tok_nextstr(&line, &text) < 0) goto invalid;
    if(at_tok_nextint(&line, &maxLength) < 0) goto invalid;
    if(at_tok_nextint(&line, &minLength) < 0) goto invalid;
    if(at_tok_nextstr(&line, &defaultText) < 0) goto invalid;
    if(at_tok_nextint(&line, &iconId) < 0) goto invalid;

    LOGD(" qualifier_flags= %d\r\n", qualifier_flags);
    LOGD(" dcs= %s\r\n", dcs);
    LOGD(" text= %s\r\n", text);
    LOGD(" max resplen= %d\r\n", maxLength);
    LOGD(" min resplen= %d\r\n", minLength);
    LOGD(" default text= %s\r\n", defaultText);
    LOGD(" icon id= %d\r\n", iconId);

    bertlv = create_ber_tlv(STK_TAG_PROACTIVE_COMMAND);

    // Command details
    simpletlv = create_simple_tlv(STK_TAG_COMMAND_DETAILS |
        STK_TLV_NEEDS_COMPREHENSION);
    // Command number (01..FF), in theory specified by SIM
    simpletlv_add_value(simpletlv, STK_COMMAND_NUMBER);
    simpletlv_add_value(simpletlv, "23");
    //qualifier_flags |= 0x08; // User input in packed format
    sprintf(qualifier, "%02X", qualifier_flags);
    simpletlv_add_value(simpletlv, qualifier);
    bertlv_add_tlv(bertlv, simpletlv);

    // Device identities
    simpletlv = create_simple_tlv(STK_TAG_DEVICE_IDENTITIES);
    simpletlv_add_value(simpletlv, STK_DEVID_SIM STK_DEVID_ME);
    bertlv_add_tlv(bertlv, simpletlv);

    // Text string
    simpletlv = create_simple_tlv(STK_TAG_TEXT_STRING);
    simpletlv_add_value(simpletlv, dcs);
    simpletlv_add_value(simpletlv, text);
    bertlv_add_tlv(bertlv, simpletlv);

    // Response length
    simpletlv = create_simple_tlv(STK_TAG_RESPONSE_LENGTH);
    sprintf(minLength_str, "%02X", minLength);
    sprintf(maxLength_str, "%02X", maxLength);
    simpletlv_add_value(simpletlv, minLength_str);
    simpletlv_add_value(simpletlv, maxLength_str);
    bertlv_add_tlv(bertlv, simpletlv);

    // Default text (optional)
    if (defaultText)
    {
        LOGD(" add default text tlv\r\n");
        simpletlv = create_simple_tlv(STK_TAG_DEFAULT_TEXT);
        simpletlv_add_value(simpletlv, dcs);
        simpletlv_add_value(simpletlv, defaultText);
        bertlv_add_tlv(bertlv, simpletlv);
    }

    // Icon identifier (optional)
    if (iconId)
    {
        LOGD(" add icon identifier tlv\r\n");
        simpletlv = create_simple_tlv(STK_TAG_ICON_IDENTIFIER);
        sprintf(icon, "%02X", iconId);
        simpletlv_add_value(simpletlv, "00");
        simpletlv_add_value(simpletlv, icon);
        bertlv_add_tlv(bertlv, simpletlv);
    }

    result = bertlv_to_string(bertlv);
    bertlv_free(bertlv);

    LOGD("build_get_input() - Exit\r\n");
    return result;
invalid:
    if (bertlv) bertlv_free(bertlv);
    LOGD("build_get_input() - Exit, invalid\r\n");
    return NULL;
}

/*
 * SELECT_ITEM bertlv
 */
char *build_select_item(char *line, ATLine *lines) {
    int numItems;
    int selection;
    int helpInfo;
    char *alphaId;
    ATLine *p_cur;
    Ber_Tlv *bertlv = NULL;
    SimpleTlv *simpletlv = NULL;
    int itemId;
    char *itemIdHexa;
    char *itemText;
    char *result;
    int parsedItems = 0;

    // Params reading
    if(at_tok_nextint(&line, &numItems) < 0) goto invalid;
    if(at_tok_nextint(&line, &selection) < 0) goto invalid;
    if(at_tok_nextint(&line, &helpInfo) < 0) goto invalid;
    if(at_tok_nextstr(&line, &alphaId) < 0) goto invalid;

    bertlv = create_ber_tlv(STK_TAG_PROACTIVE_COMMAND);

    // Command details
    simpletlv = create_simple_tlv(STK_TAG_COMMAND_DETAILS |
        STK_TLV_NEEDS_COMPREHENSION);
    // Command number (01..FF), in theory specified by SIM
    simpletlv_add_value(simpletlv, STK_COMMAND_NUMBER);
    simpletlv_add_value(simpletlv, "24" "00");
    bertlv_add_tlv(bertlv, simpletlv);

    // Device identities
    simpletlv = create_simple_tlv(STK_TAG_DEVICE_IDENTITIES);
    simpletlv_add_value(simpletlv, STK_DEVID_SIM STK_DEVID_ME);
    bertlv_add_tlv(bertlv, simpletlv);

    // Alpha identifier
    simpletlv = create_simple_tlv(STK_TAG_ALPHA_IDENTIFIER);
    simpletlv_add_value(simpletlv, alphaId);
    bertlv_add_tlv(bertlv, simpletlv);

    for (p_cur = lines; p_cur; p_cur = p_cur->p_next) {
        char *cur_line = p_cur->line;

        at_tok_try_to_start(cur_line);
        if(at_tok_nextint(&cur_line, &itemId) < 0) continue;
        if(at_tok_nextstr(&cur_line, &itemText) < 0) continue;

        itemIdHexa = (char *)alloca(2 * sizeof(char));
        sprintf(itemIdHexa, "%02x", itemId);

        // Item
        simpletlv = create_simple_tlv(STK_TAG_ITEM);
        simpletlv_add_value(simpletlv, itemIdHexa);
        simpletlv_add_value(simpletlv, itemText);
        bertlv_add_tlv(bertlv, simpletlv);

        parsedItems++;
    }
    if (parsedItems != numItems) {
        LOGD("Expected %d items, %d found", numItems, parsedItems);
    }
    if (parsedItems != 0) {
        result = bertlv_to_string(bertlv);
        bertlv_free(bertlv);
        return result;
    } else {
        LOGD("No valid menu items found");
    }
invalid:
    if (bertlv) bertlv_free(bertlv);
    return NULL;
}

/*
 * SET_UP_MENU bertlv
 */
char *build_set_up_menu(char *line, ATLine *lines) {
    int qualifier_flags;
    char qualifier[3];
    int numItems;
//    int selection;
//    int helpInfo;
//    int removeMenu;
    char *alpha;
//    ATLine *p_cur;
    Ber_Tlv *bertlv = NULL;
    SimpleTlv *simpletlv = NULL;
    int itemId;
//    char *itemIdHexa;
    char *itemText;
    char item[3];
    char *result;
//    int parsedItems = 0;

    LOGD("build_set_up_menu() - Enter\r\n");

    // Params reading
    if(at_tok_nextint(&line, &qualifier_flags) < 0) goto invalid;
    if(at_tok_nextstr(&line, &alpha) < 0) goto invalid;
    if(at_tok_nextint(&line, &itemId) < 0) goto invalid;
    if(at_tok_nextint(&line, &numItems) < 0) goto invalid;
    if(at_tok_nextstr(&line, &itemText) < 0) goto invalid;
//    if(at_tok_nextint(&line, &selection) < 0) goto invalid;
//    if(at_tok_nextint(&line, &helpInfo) < 0) goto invalid;
//    if(at_tok_nextint(&line, &removeMenu) < 0) goto invalid;


    bertlv = create_ber_tlv(STK_TAG_PROACTIVE_COMMAND);

    // Command details
    simpletlv = create_simple_tlv(STK_TAG_COMMAND_DETAILS |
        STK_TLV_NEEDS_COMPREHENSION);
    // Command number (01..FF), in theory specified by SIM
    simpletlv_add_value(simpletlv, STK_COMMAND_NUMBER);
//    simpletlv_add_value(simpletlv, "25" "00");
    sprintf(qualifier, "%02X", qualifier_flags);
    simpletlv_add_value(simpletlv, qualifier);
    bertlv_add_tlv(bertlv, simpletlv);

    // Device identities
    simpletlv = create_simple_tlv(STK_TAG_DEVICE_IDENTITIES);
    simpletlv_add_value(simpletlv, STK_DEVID_SIM STK_DEVID_ME);
    bertlv_add_tlv(bertlv, simpletlv);

    // Alpha identifier
    simpletlv = create_simple_tlv(STK_TAG_ALPHA_IDENTIFIER);
    simpletlv_add_value(simpletlv, alpha);
    bertlv_add_tlv(bertlv, simpletlv);
/*
    for (p_cur = lines; p_cur; p_cur = p_cur->p_next) {
        char *cur_line = p_cur->line;

        at_tok_try_to_start(cur_line);
        if(at_tok_nextint(&cur_line, &itemId) < 0) continue;
        if(at_tok_nextstr(&cur_line, &itemText) < 0) continue;

        itemIdHexa = (char *)alloca(2 * sizeof(char));
        sprintf(itemIdHexa, "%02x", itemId);

        // Item
        simpletlv = create_simple_tlv(STK_TAG_ITEM);
        simpletlv_add_value(simpletlv, itemIdHexa);
        simpletlv_add_value(simpletlv, itemText);
        bertlv_add_tlv(bertlv, simpletlv);

        parsedItems++;
    }
    if (parsedItems != numItems) {
        LOGD("Expected %d items, %d found", numItems, parsedItems);
    }
    if (parsedItems != 0) {
        result = bertlv_to_string(bertlv);
        bertlv_free(bertlv);
        LOGD("build_set_up_menu() - Exit\r\n");
        return result;
    } else {
        LOGD("No valid menu items found");
    }
*/
    // Item
    simpletlv = create_simple_tlv(STK_TAG_ITEM);
    sprintf(item, "%02X", itemId);
    simpletlv_add_value(simpletlv, item);
    simpletlv_add_value(simpletlv, itemText);
    bertlv_add_tlv(bertlv, simpletlv);

    result = bertlv_to_string(bertlv);
    bertlv_free(bertlv);

    LOGD("build_set_up_menu() - Exit\r\n");
    return result;

invalid:
    if (bertlv) bertlv_free(bertlv);
    LOGD("build_set_up_menu() - Exit, Invalid\r\n");
    return NULL;
}

char *stk_at_to_hex(ATResponse *p_response) {
    int cmd;
    char *line = NULL;
    int err;
    char *result = NULL;

    LOGD("stk_at_to_hex() - Enter\r\n");

    line = p_response->p_intermediates->line;

    LOGD("line: %s", line);

    at_tok_try_to_start(line);
//    if (at_tok_nexthexint(&line, &cmd) < 0) {
    if (at_tok_nextint(&line, &cmd) < 0) {
        return NULL;
    }

    LOGD("Converting command %#2x", cmd);

    switch (cmd) {
        case STK_CMD_REFRESH: result = NULL; break;
        case STK_CMD_MORE_TIME:
            result = build_more_time(line, p_response->p_intermediates);
            break;
        case STK_CMD_POLL_INTERVAL: result = NULL; break;
        case STK_CMD_POLLING_OFF: result = NULL; break;
        case STK_CMD_SET_UP_CALL:
            result = build_set_up_call(line, p_response->p_intermediates);
            break;
        case STK_CMD_SEND_SS: result = NULL; break;
        case STK_CMD_SEND_USSD: result = NULL; break;
        case STK_CMD_SEND_SMS: result = NULL; break;
        case STK_CMD_PLAY_TONE:
            LOGD("STK_CMD_PLAY_TONE\r\n");
            result = build_play_tone(line, p_response->p_intermediates);
            break;
        case STK_CMD_DISPLAY_TEXT:
            LOGD("STK_CMD_DISPLAY_TEXT\r\n");
            result = build_display_text(line, p_response->p_intermediates);
            break;
        case STK_CMD_GET_INKEY: result = NULL; break;
            result = build_get_inkey(line, p_response->p_intermediates);
            break;
        case STK_CMD_GET_INPUT:
            LOGD("STK_CMD_GET_INPUT\r\n");
            result = build_get_input(line, p_response->p_intermediates);
            break;
        case STK_CMD_SELECT_ITEM:
            LOGD("STK_CMD_SELECT_ITEM\r\n");
            result = build_select_item(line, p_response->p_intermediates);
            break;
        case STK_CMD_SET_UP_MENU:
            LOGD("STK_CMD_SET_UP_MENU\r\n");
            result = build_set_up_menu(line, p_response->p_intermediates);
            break;
        case STK_CMD_PROVIDE_LOCAL_INFO: result = NULL; break;
        case STK_CMD_SET_UP_IDLE_TEXT: result = NULL; break;
        case STK_CMD_OPEN_CHANNEL: result = NULL; break;
        default:
            LOGD("Unknown command %#2x", cmd);
            return NULL;
    }
    if (!result) {
        LOGD("stk_at_to_hex: couldn't encode command: %#2x,%s", cmd, line);
    }
    LOGD("stk_at_to_hex() - Exit\r\n");
    return result;
}


typedef struct {
    int number;
    int type;
    int qualifier;
} StkCommandDetails;

typedef struct {
    /* 0x01 Keypad
     * 0x02 Display
     * 0x03 Earpiece
     * 0x81 SIM
     * 0x82 ME
     * 0x83 Network
     */
    int source;
    int destination;
} StkDeviceIdentities;

typedef struct {
    /* 0x00 Minutes
     * 0x01 Seconds
     * 0x02 Tenths of seconds
     */
    int unit;
    int interval;
} StkDuration;

typedef struct {
    int result;
    int additional_info;
} StkResult;

typedef struct {
    int identifier;
} StkItemIdentifier;

typedef struct {
    int mnc; // MCC & MNC
    int lac; // Location area code
    int cell_id;
} StkLocationInformation;

typedef struct {
    int dcs;
    char *string;
} StkTextString;

typedef union {
    StkCommandDetails command_details;
    StkDeviceIdentities device_identities;
    StkResult result;
    StkDuration duration;
    StkItemIdentifier item_identifier;
    StkLocationInformation location;
    StkTextString text_string;
} StkTlv;

int hex_to_stk_command_details(const char *value, StkTlv *tlv) {
    StkCommandDetails *stk = &tlv->command_details;
    if (sscanf(value, "%2X%2X%2X",
            &stk->number, &stk->type, &stk->qualifier) == 3) {
        return 1;
    } else {
        return 0;
    }
}

int hex_to_stk_result(const char *value, StkTlv *tlv) {
    StkResult *stk = &tlv->result;
    if (sscanf(value, "%2X%2X", &stk->result, &stk->additional_info) >= 1) {
        return 1;
    } else {
        return 0;
    }
}

int hex_to_stk_text_string(const char *value, StkTlv *tlv) {
    StkTextString *stk = &tlv->text_string;
    stk->string = (char *)(value + 2);
    if (sscanf(value, "%2X", &stk->dcs) == 1 && stk->string[0]) {
        return 1;
    } else {
        return 0;
    }
}

int hex_to_stk_item_identifier(const char *value, StkTlv *tlv) {
    StkItemIdentifier *stk = &tlv->item_identifier;
    if (sscanf(value, "%2X", &stk->identifier) == 1) {
        return 1;
    } else {
        return 0;
    }
}

int get_result_code(int cmd, int stk_result, int additional_info) {
    if (!stk_result) return 0; // Success
    switch (cmd) {
        case STK_CMD_SET_UP_CALL:
            switch (stk_result) {
                case 0x22:  return 0x01; // User-rejected call
                case 0x23:  return 0x02; // User-cleared call
            }
            break;
        case STK_CMD_LAUNCH_BROWSER:
            switch (stk_result) {
                case 0x01:  return 1; // Partial comprehension
                case 0x02:  return 2; // Missing info
                case 0x03:  return 3; // User rejected launch
                //case 0x??:  return 5; // Bearer unavailable
                //case 0x??:  return 6; // Browser unavailable
                case 0x20:  return 7; // ME cannot process command
                case 0x21:  return 8; // Network cannot process command
                case 0x30:  return 9; // Command beyond ME capabilities
                default:
                    return 4; // Error - No specific cause given
            }
            break;
        case STK_CMD_PLAY_TONE:
            switch (stk_result) {
                //case 0x??:  return 1; // Terminate proactive session
                //case 0x??:  return 2; // Tone not played
                //case 0x??:  return 3; // Specified tone not supported
                default:
                    return 3; // Specified tone not supported
            }
            break;
        case STK_CMD_DISPLAY_TEXT:
            switch (stk_result) {
                // case 0x??:  return 1; // Terminate proactive session
                // case 0x??:  return 2; // User cleared message
                // case 0x??:  return 4; // Backward move requested
                // case 0x??:  return 5; // No response from user
                case 0x20:
                    switch (additional_info) {
                       case 0x1:  return 3; // Screen is busy
                    }
            }
            break;
        case STK_CMD_GET_INKEY:
        case STK_CMD_GET_INPUT:
        case STK_CMD_SELECT_ITEM:
            switch (stk_result) {
                // case 0x??:  return 1; // Terminate proactive session
                // case 0x??:  return 2; // Help information requested
                // case 0x??:  return 3; // Backward move requested
                // case 0x??:  return 4; // No response from user
            }
            break;
        case STK_CMD_SET_UP_MENU:
            switch (stk_result) {
                case 0x20:  return 1; // User chosen menu item
                // case 0x??:  return 2; // Help information requested
                default:
                    return 3; // Problem with menu operation
            }
            break;
    }
    return 1;
}

char *hex_to_stk_at(const char *at)
{
    int i = 0;
    int cmd = 0;
    int tag, length, needs_comprehension = 0;
    int at_command = 0;
    int item_id = 0;
    char *text_string = NULL;
    int dcs = 0;
    char *value = NULL;
    char *result;
    StkTlv tlv;
    int result_code = 0;

    LOGD("hex_to_stk_at() - Enter\r\n");

    while (sscanf(at, "%2x%2x", &tag, &length) == 2) {
        at += 4;
        if (length == 0x81) {
            sscanf(at, "%2x", &length);
            at += 2;
        }

        value = strndup(at, length * 2);

        if (tag < STK_TLV_FIXED_DIRECTION) {
            needs_comprehension = tag & STK_TLV_NEEDS_COMPREHENSION;
            tag &= ~STK_TLV_NEEDS_COMPREHENSION;
            at += strlen(value);
        } else {
            // BerTLV
            needs_comprehension = 0;
        }

        LOGD("tag = %#x, i = %d, needs_comprehension = %d, value = %s",
             tag, i, needs_comprehension, value);

        switch (i) {
            case 0:
                // First tlv has to be a Command Details one (for terminal
                // response)
                switch (tag) {
                    case STK_TAG_COMMAND_DETAILS:
                        LOGD(" STK_TAG_COMMAND_DETAILS\r\n");
                        if (hex_to_stk_command_details(value, &tlv)) {
                            cmd = tlv.command_details.type;
                            break;
                        }
                        goto invalid;
                    case STK_TAG_MENU_SELECTION:
                        LOGD(" STK_TAG_MENU_SELECTION\r\n");
                        at_command = tag;
                        break;
                    default:
                        goto invalid;
                }
                break;
            case 1:
                // Second tlv has to be a Device Identities one
                switch (tag) {
                    case STK_TAG_DEVICE_IDENTITIES:
                        LOGD(" STK_TAG_DEVICE_IDENTITIES\r\n");
                        break;
                    default:
                        goto invalid;
                }
                break;
            default:
                switch (tag) {
                    case STK_TAG_RESULT:
                        LOGD(" STK_TAG_RESULT\r\n");
                        if (hex_to_stk_result(value, &tlv)) {
                            result_code = get_result_code(cmd,
                                tlv.result.result, tlv.result.additional_info);
                            break;
                        }
                        goto invalid;
                    case STK_TAG_DURATION:
                        LOGD(" STK_TAG_DURATION\r\n");
                        break;
                    case STK_TAG_TEXT_STRING:
                        LOGD(" STK_TAG_TEXT_STRING\r\n");
                        if (hex_to_stk_text_string(value, &tlv)) {
                            text_string = alloca(
                                (strlen(tlv.text_string.string) + 1) *
                                sizeof(char));
                            strcpy(text_string, tlv.text_string.string);
                            dcs = tlv.text_string.dcs;
                            break;
                        }
                        goto invalid;
                    case STK_TAG_ITEM_IDENTIFIER:
                        LOGD(" STK_TAG_ITEM_IDENTIFIER\r\n");
                        if (hex_to_stk_item_identifier(value, &tlv)) {
                            item_id = tlv.item_identifier.identifier;
                            break;
                        }
                        goto invalid;
                    case STK_TAG_LOCATION_INFORMATION:
                        LOGD(" STK_TAG_LOCATION_INFORMATION\r\n");
                        break;
                }
                break;
        }
        if (value) free(value);
        value = NULL;
        i++;
    }

    switch (at_command) {
        case STK_TAG_SMS_PP_DOWNLOAD:
            return NULL;
        case STK_TAG_CELL_DOWNLOAD:
            return NULL;
        case STK_TAG_MENU_SELECTION:
            asprintf(&result, "AT+STMS=%d", item_id);
            break;
        case STK_TAG_CALL_CONTROL:
            return NULL;
        default: // Terminal response
            LOGD(" send terminal response string...\r\n");
            switch (cmd) {
                case STK_CMD_GET_INKEY:
                case STK_CMD_GET_INPUT:
                case STK_CMD_SELECT_ITEM:
                    asprintf(&result, "AT+STKTR=%d,%d,0,0,%d,\"%s\"\r", cmd, result_code,
                        dcs, text_string);
                    break;
                default:
                    asprintf(&result, "AT+STKTR=%d,%d,0\r", cmd, result_code);
            }
    }
    LOGD("hex_to_stk_at() - Exit\r\n");
    return result;

invalid:
    if (value) {
        LOGD("Invalid TLV, current T:%#x L:%d V:%s rest:%s",
                tag, length, value, at);
        free(value);
    }
    LOGD("hex_to_stk_at() - Exit\r\n");
    return NULL;
}
