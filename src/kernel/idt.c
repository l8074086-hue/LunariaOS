#include <stdint.h>
#include "port.h"
#include "keyboard.h"

extern void isr0x21(void);
extern void isr0x80(void);
extern void isrfault0x0D(void);
extern void isrfault0x0E(void);
extern void isr_dummy(void);

struct idt_entry {
  uint16_t offset_low;
  uint16_t selector;
  uint8_t zero;
  uint8_t type_attr;
  uint16_t offset_high;
} __attribute__((packed));

struct idt_ptr {
  uint16_t limit;
  uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[256];

void set_gate(int vector, uint32_t addr, int dpl, int trap)
{
  idt[vector].offset_low = addr & 0xFFFF;
  idt[vector].selector = 0x08;
  idt[vector].zero = 0;
  idt[vector].type_attr = (trap ? 0x8F : 0x8E) | (dpl << 5);
  idt[vector].offset_high = (addr >> 16) & 0xFFFF;
}

void isr_common_handler(int vector)
{
  if (vector == 0x21)
    keyboard_handler();
  __asm__ volatile("outb %0, %1" : : "a"((char)0x20), "Nd"((short)0x20));
}

void idt_init(void)
{
  outb(0x20, 0x11); outb(0xA0, 0x11);
  outb(0x21, 0x20); outb(0xA1, 0x28);
  outb(0x21, 0x04); outb(0xA1, 0x02);
  outb(0x21, 0x01); outb(0xA1, 0x01);
  outb(0x21, 0xFD); outb(0xA1, 0xFF);

  for (int i = 0; i < 256; i++)
    set_gate(i, (uint32_t)isr_dummy, 0, 0);

  set_gate(0x21, (uint32_t)isr0x21, 0, 0);
  set_gate(0x0D, (uint32_t)isrfault0x0D, 0, 0);
  set_gate(0x0E, (uint32_t)isrfault0x0E, 0, 0);
  set_gate(0x80, (uint32_t)isr0x80, 3, 1);

  struct idt_ptr ptr;
  ptr.limit = sizeof(idt) - 1;
  ptr.base = (uint32_t)&idt;

  __asm__ volatile("lidt %0" : : "m"(ptr));
}
