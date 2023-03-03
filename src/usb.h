#ifndef _USB_H_
#define _USB_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void reset_usb(void);
int is_usb_enumerated(void);
void await_usb_enumeration(void);
int write_usb(const char *s, int n, bool flush);
int interrupt_usb(uint8_t *buffer, size_t n);
void set_usb_data_in_callback(void (*new_callback)(uint8_t *data, size_t n));

int is_usb_dtr(void);
void await_usb_dtr(void);
int is_usb_rts(void);
void await_usb_rts(void);
int set_usb_serial_state(uint16_t state);

int uprintf(const char *format, ...);

#endif
