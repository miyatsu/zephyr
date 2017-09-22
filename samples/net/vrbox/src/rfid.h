#ifndef RFID_H
#define RFID_H

extern char rfid_list_string[4*8*2][27];

int rfid_init(void);
void rfid_scan(uint8_t times);

#endif /* RFID_H */

