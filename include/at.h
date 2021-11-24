#ifndef _AT_H
#define _AT_H

#include <twr_atci.h>
#include <twr_cmwx1zzabz.h>
#include <twr_led.h>

#define AT_LORA_COMMANDS {"$DEVEUI", NULL, at_deveui_set, at_deveui_read, NULL, ""},\
                         {"$DEVADDR", NULL, at_devaddr_set, at_devaddr_read, NULL, ""},\
                         {"$NWKSKEY", NULL, at_nwkskey_set, at_nwkskey_read, NULL, ""},\
                         {"$APPSKEY", NULL, at_appskey_set, at_appskey_read, NULL, ""},\
                         {"$APPKEY", NULL, at_appkey_set, at_appkey_read, NULL, ""},\
                         {"$APPEUI", NULL, at_appeui_set, at_appeui_read, NULL, ""},\
                         {"$BAND", NULL, at_band_set, at_band_read, NULL, "0:AS923, 1:AU915, 5:EU868, 6:KR920, 7:IN865, 8:US915"},\
                         {"$MODE", NULL, at_mode_set, at_mode_read, NULL, "0:ABP, 1:OTAA"},\
                         {"$NWK", NULL, at_nwk_set, at_nwk_read, NULL, "Network type 0:private, 1:public"},\
                         {"$ADR", NULL, at_adr_set, at_adr_read, NULL, "Automatic data rate 0:disabled, 1:enabled"},\
                         {"$DR", NULL, at_dr_set, at_dr_read, NULL, "Data rate 0-15"},\
                         {"$REPU", NULL, at_repu_set, at_repu_read, NULL, "Repeat of unconfirmed transmissions 1-15"},\
                         {"$REPC", NULL, at_repc_set, at_repc_read, NULL, "Repeat of confirmed transmissions 1-8"},\
                         {"$JOIN", at_join, NULL, NULL, NULL, "Send OTAA Join packet"},\
                         {"$FRMCNT", at_frmcnt, NULL, NULL, NULL, "Get frame counters"},\
                         {"$LNCHECK", at_link_check, NULL, NULL, NULL, "MAC Link Check"},\
                         {"$RFQ", at_rfq, NULL, NULL, NULL, "Get RSSI/SNR of last RX packet"},\
                         {"$REBOOT", at_reboot, NULL, NULL, NULL, "Firmware reboot"},\
                         {"$FRESET", at_freset, NULL, NULL, NULL, "LoRa Module factory reset"},\
                         {"$FWVER", at_ver_read, NULL, NULL, NULL, "Show LoRa Module firmware version"}


#define AT_LED_COMMANDS {"$BLINK", at_blink, NULL, NULL, NULL, "LED blink 3 times"},\
                        {"$LED", NULL, at_led_set, NULL, at_led_help, "LED on/off"}

void at_init(twr_led_t *led, twr_cmwx1zzabz_t *lora);

bool at_deveui_read(void);
bool at_deveui_set(twr_atci_param_t *param);

bool at_devaddr_read(void);
bool at_devaddr_set(twr_atci_param_t *param);

bool at_nwkskey_read(void);
bool at_nwkskey_set(twr_atci_param_t *param);

bool at_appskey_read(void);
bool at_appskey_set(twr_atci_param_t *param);

bool at_appkey_read(void);
bool at_appkey_set(twr_atci_param_t *param);

bool at_appeui_read(void);
bool at_appeui_set(twr_atci_param_t *param);

bool at_band_read(void);
bool at_band_set(twr_atci_param_t *param);

bool at_mode_read(void);
bool at_mode_set(twr_atci_param_t *param);

bool at_nwk_read(void);
bool at_nwk_set(twr_atci_param_t *param);

bool at_adr_read(void);
bool at_adr_set(twr_atci_param_t *param);

bool at_dr_read(void);
bool at_dr_set(twr_atci_param_t *param);

bool at_repu_read(void);
bool at_repu_set(twr_atci_param_t *param);

bool at_repc_read(void);
bool at_repc_set(twr_atci_param_t *param);

bool at_ver_read(void);

bool at_reboot(void);
bool at_freset(void);
bool at_frmcnt(void);
bool at_link_check(void);
bool at_rfq(void);

bool at_join(void);
bool at_blink(void);
bool at_led_set(twr_atci_param_t *param);
bool at_led_help(void);

#endif // _AT_H
