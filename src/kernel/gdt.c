#include <stdint.h>
#include "gdt.h"

#define GDT_ENTRIES 6

#define KERNEL_CODE 0x08
#define KERNEL_DATA 0x10
#define USER_CODE   0x18
#define USER_DATA   0x20
#define TSS_SELECTOR 0x28

struct gdt_entry
{
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  flags_limit_high;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr
{
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

struct tss
{
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax, ecx, edx, ebx;
    uint32_t esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap;
} __attribute__((packed));

static struct gdt_entry gdt[GDT_ENTRIES];
static struct tss tss;

static void set_gdt_entry(int index, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags)
{
    gdt[index].limit_low = limit & 0xFFFF;
    gdt[index].base_low = base & 0xFFFF;
    gdt[index].base_mid = (base >> 16) & 0xFF;
    gdt[index].access = access;
    gdt[index].flags_limit_high = ((limit >> 16) & 0x0F) | (flags & 0xF0);
    gdt[index].base_high = (base >> 24) & 0xFF;
}

void gdt_init(void)
{
    set_gdt_entry(0, 0, 0, 0, 0);
    set_gdt_entry(1, 0, 0xFFFFF, 0x9A, 0xC0); /* kernel code, DPL 0 */
    set_gdt_entry(2, 0, 0xFFFFF, 0x92, 0xC0); /* kernel data, DPL 0 */
    set_gdt_entry(3, 0, 0xFFFFF, 0xFA, 0xC0); /* user code,   DPL 3 */
    set_gdt_entry(4, 0, 0xFFFFF, 0xF2, 0xC0); /* user data,   DPL 3 */

    tss.esp0 = 0x90000;
    tss.ss0 = KERNEL_DATA;
    tss.iomap = sizeof(struct tss);
    set_gdt_entry(5, (uint32_t)&tss, sizeof(struct tss) - 1, 0x89, 0x00);

    struct gdt_ptr ptr;
    ptr.limit = sizeof(gdt) - 1;
    ptr.base = (uint32_t)&gdt;

    __asm__ volatile(
        "lgdt %0\n\t"
        "ljmp $0x08, $1f\n\t"
        "1:\n\t"
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss\n\t"
        "mov $0x28, %%ax\n\t"
        "ltr %%ax"
        : : "m"(ptr) : "ax");
}
