#include "keyboard.h"
#include "port.h"

#define BUF_SIZE 256
#define KBD_DATA 0x60

static volatile char buffer[BUF_SIZE];
static volatile int head = 0;
static volatile int tail = 0;

static const char scancodes[256] = {
    [0x01] = 0x1b,
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
    [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
    [0x0a] = '9', [0x0b] = '0', [0x0c] = '-', [0x0d] = '=',
    [0x0e] = '\b',
    [0x0f] = '\t',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r',
    [0x14] = 't', [0x15] = 'y', [0x16] = 'u', [0x17] = 'i',
    [0x18] = 'o', [0x19] = 'p', [0x1a] = '[', [0x1b] = ']',
    [0x1c] = '\n',
    [0x1e] = 'a', [0x1f] = 's', [0x20] = 'd', [0x21] = 'f',
    [0x22] = 'g', [0x23] = 'h', [0x24] = 'j', [0x25] = 'k',
    [0x26] = 'l', [0x27] = ';', [0x28] = '\'',
    [0x29] = '`',
    [0x2b] = '\\',
    [0x2c] = 'z', [0x2d] = 'x', [0x2e] = 'c', [0x2f] = 'v',
    [0x30] = 'b', [0x31] = 'n', [0x32] = 'm', [0x33] = ',',
    [0x34] = '.', [0x35] = '/',
    [0x37] = '*',
    [0x39] = ' ',
};

static const char scancodes_shift[256] = {
    [0x01] = 0x1b,
    [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$',
    [0x06] = '%', [0x07] = '^', [0x08] = '&', [0x09] = '*',
    [0x0a] = '(', [0x0b] = ')', [0x0c] = '_', [0x0d] = '+',
    [0x0e] = '\b',
    [0x0f] = '\t',
    [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R',
    [0x14] = 'T', [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I',
    [0x18] = 'O', [0x19] = 'P', [0x1a] = '{', [0x1b] = '}',
    [0x1c] = '\n',
    [0x1e] = 'A', [0x1f] = 'S', [0x20] = 'D', [0x21] = 'F',
    [0x22] = 'G', [0x23] = 'H', [0x24] = 'J', [0x25] = 'K',
    [0x26] = 'L', [0x27] = ':', [0x28] = '"',
    [0x29] = '~',
    [0x2b] = '|',
    [0x2c] = 'Z', [0x2d] = 'X', [0x2e] = 'C', [0x2f] = 'V',
    [0x30] = 'B', [0x31] = 'N', [0x32] = 'M', [0x33] = '<',
    [0x34] = '>', [0x35] = '?',
    [0x37] = '*',
    [0x39] = ' ',
};

static int modifiers = 0;
static int caps_lock = 0;
static int extended = 0;

void keyboard_handler(void)
{
    char sc = inb(KBD_DATA);

    int next = (head + 1) % BUF_SIZE;
    if (next != tail)
    {
        buffer[head] = sc;
        head = next;
    }
}

int kbd_getc(void)
{
    while (head == tail)
        ;

    unsigned char scancode = buffer[tail];
    tail = (tail + 1) % BUF_SIZE;

    if (scancode == 0xe0)
    {
        extended = 1;
        return 0;
    }

    if (scancode & 0x80)
    {
        unsigned char key = scancode & 0x7f;
        if (extended)
        {
            extended = 0;
            switch (key)
            {
                case 0x1d: modifiers &= ~MOD_CTRL;  break;
                case 0x38: modifiers &= ~MOD_ALT;   break;
                case 0x5b:
                case 0x5c: modifiers &= ~MOD_SUPER; break;
            }
        }
        else
        {
            switch (key)
            {
                case 0x2a:
                case 0x36: modifiers &= ~MOD_SHIFT; break;
                case 0x1d: modifiers &= ~MOD_CTRL;  break;
                case 0x38: modifiers &= ~MOD_ALT;   break;
            }
        }
        return 0;
    }

    if (extended)
    {
        extended = 0;
        switch (scancode)
        {
            case 0x1d: modifiers |= MOD_CTRL;  return 0;
            case 0x38: modifiers |= MOD_ALT;   return 0;
            case 0x5b:
            case 0x5c: modifiers |= MOD_SUPER; return 0;
            case 0x48: return KEY_UP;
            case 0x50: return KEY_DOWN;
            case 0x4b: return KEY_LEFT;
            case 0x4d: return KEY_RIGHT;
            default:   return 0;
        }
    }

    switch (scancode)
    {
        case 0x2a:
        case 0x36: modifiers |= MOD_SHIFT; return 0;
        case 0x1d: modifiers |= MOD_CTRL;  return 0;
        case 0x38: modifiers |= MOD_ALT;   return 0;
        case 0x3a: caps_lock = !caps_lock; return 0;
        default: break;
    }

    char base = scancodes[scancode];
    char shifted = scancodes_shift[scancode];

    if (base >= 'a' && base <= 'z')
    {
        int want_upper = ((modifiers & MOD_SHIFT) != 0) ^ (caps_lock != 0);
        return want_upper ? shifted : base;
    }

    return (modifiers & MOD_SHIFT) ? shifted : base;
}

int kbd_mods(void)
{
    return modifiers;
}

void kbd_flush(void)
{
    while (head != tail)
        tail = (tail + 1) % BUF_SIZE;
    extended = 0;
}

int caps_active(void)
{
    return caps_lock;
}
