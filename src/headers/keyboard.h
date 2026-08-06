#ifndef KEYBOARD_H
#define KEYBOARD_H

#define KEY_UP 0xE0
#define KEY_DOWN 0xE1
#define KEY_LEFT 0xE2
#define KEY_RIGHT 0xE3

#define MOD_SHIFT 0x01
#define MOD_CTRL  0x02
#define MOD_ALT   0x04
#define MOD_SUPER 0x08

int kbd_getc(void);
int kbd_mods(void);
int caps_active(void);
void keyboard_handler(void);
void kbd_flush(void);

#endif
