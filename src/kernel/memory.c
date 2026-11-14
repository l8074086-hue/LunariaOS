#include "memory.h"
#include "string.h"
#include "user.h"

/* E820 table placed by the bootloader in real mode */
#define E820_COUNT_ADDR 0x4FFC
#define E820_BUF_ADDR   0x5000
#define E820_ENTRY_SIZE 24
#define E820_USABLE     1

#define PAGE_SIZE       4096

/* regions we must never hand out or page out */
#define LOW_END         0x100000UL  /* kernel, stack @0x90000, VGA, boot data */

struct e820_entry
{
    uint32_t base_lo, base_hi;
    uint32_t len_lo, len_hi;
    uint32_t type;
    uint32_t attrs;
};

extern char _kernel_end[];

static uint32_t mem_top;        /* highest usable physical address, capped at 4G */
static uint32_t nframes;
static uint32_t frames_free;
static uint32_t reserved_end;   /* first frame past kernel + bitmap + low memory */
static unsigned char *bitmap;   /* 1 bit per 4K frame, placed right after kernel */
static uint32_t e820_mismatches; /* frames whose bitmap state disagrees with E820 */
static uint32_t page_dir[1024] __attribute__((aligned(4096))); /* kernel page dir (.bss) */

static uint32_t e820_count(void)
{
    return *(volatile uint16_t *)E820_COUNT_ADDR;
}

/* End of an E820 entry, clamped to the 32-bit address space. A non-zero
   len_hi means the range crosses 4G, which this 32-bit kernel cannot use. */
static uint32_t region_end(const struct e820_entry *e)
{
    uint32_t end = e->base_lo + e->len_lo;
    if (e->len_hi || end < e->base_lo)
        return 0xFFFFFFFF;
    return end;
}

static uint32_t top_usable(void)
{
    struct e820_entry *e = (struct e820_entry *)E820_BUF_ADDR;
    uint32_t top = 0;
    for (uint32_t i = 0; i < e820_count(); i++)
    {
        if (e[i].type != E820_USABLE || e[i].base_hi != 0)
            continue;
        uint32_t end = region_end(&e[i]);
        if (end > top)
            top = end;
    }
    return top;
}

/* A frame is usable only when the whole 4K is covered by E820 type-1 ranges.
   Checking just the first byte would hand out frames that straddle a hole. */
static int frame_usable(uint32_t addr)
{
    struct e820_entry *e = (struct e820_entry *)E820_BUF_ADDR;
    uint32_t end = addr + PAGE_SIZE;
    uint32_t covered = 0;

    for (uint32_t i = 0; i < e820_count(); i++)
    {
        if (e[i].type != E820_USABLE || e[i].base_hi != 0)
            continue;

        uint32_t base = e[i].base_lo;
        uint32_t r_end = region_end(&e[i]);
        uint32_t lo = addr > base ? addr : base;
        uint32_t hi = end < r_end ? end : r_end;

        if (lo < hi)
            covered += hi - lo;
    }
    return covered >= PAGE_SIZE;
}

static int frame_used(uint32_t idx)
{
    return (bitmap[idx >> 3] >> (idx & 7)) & 1;
}

static void frame_set(uint32_t idx)
{
    bitmap[idx >> 3] |= (unsigned char)(1 << (idx & 7));
}

static void frame_clear(uint32_t idx)
{
    bitmap[idx >> 3] &= (unsigned char)~(1 << (idx & 7));
}

static uint32_t alloc_frame(void)
{
    for (uint32_t i = 0; i < nframes; i++)
        if (!frame_used(i))
        {
            frame_set(i);
            frames_free--;
            return i * PAGE_SIZE;
        }
    return 0;
}

static void free_frame(uint32_t addr);

static void free_frame(uint32_t addr)
{
    uint32_t i = addr / PAGE_SIZE;
    if (i < nframes && frame_used(i))
    {
        frame_clear(i);
        frames_free++;
    }
}

/* Re-derive the expected used/free state from the BIOS map and compare it
   with the bitmap that the allocator will actually use. The count check also
   catches a frame being freed twice or lost entirely. */
static void verify_e820_bitmap(void)
{
    uint32_t mismatches = 0;
    uint32_t free_count = 0;

    for (uint32_t idx = 0; idx < nframes; idx++)
    {
        uint32_t addr = idx * PAGE_SIZE;
        int expected_used = addr < reserved_end
                         || (addr >= USER_BASE && addr < USER_TOP)
                         || !frame_usable(addr);
        int actual_used = frame_used(idx);

        if (expected_used != actual_used)
            mismatches++;
        if (!actual_used)
            free_count++;
    }

    if (free_count != frames_free)
        mismatches++;

    e820_mismatches = mismatches;
}

void memory_init(void)
{
    mem_top = top_usable();
    nframes = mem_top / PAGE_SIZE;

    /* bitmap lives just past the kernel image, page aligned */
    uint32_t bm_phys = (((uint32_t)&_kernel_end[0]) + PAGE_SIZE - 1) &
                       ~(PAGE_SIZE - 1);
    uint32_t bm_bytes = (nframes + 7) / 8;
    bitmap = (unsigned char *)bm_phys;
    memset(bitmap, 0xFF, bm_bytes);   /* everything reserved by default */

    /* Keep low memory, the kernel image and the bitmap itself out of the
       allocator even if the kernel grows past the low 1 MB one day. */
    reserved_end = bm_phys + bm_bytes;
    if (reserved_end < LOW_END)
        reserved_end = LOW_END;
    reserved_end = (reserved_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    frames_free = 0;
    for (uint32_t idx = 0; idx < nframes; idx++)
    {
        uint32_t addr = idx * PAGE_SIZE;
        int used = 0;
        if (addr < reserved_end)                  /* kernel, stack, VGA, bitmap */
            used = 1;
        else if (addr >= USER_BASE && addr < USER_TOP)  /* flat user region */
            used = 1;
        else if (!frame_usable(addr))             /* E820 holes (ACPI, etc.) */
            used = 1;
        if (!used)
        {
            frame_clear(idx);
            frames_free++;
        }
    }

    verify_e820_bitmap();
}

/* Identity-map [from,to) with 4K pages; allocates page tables on demand.
   The U/S bit lives in the page directory, so it can only be changed while we
   are the ones creating the entry. Supervisor requests are always safe to
   apply to an existing entry; a user request needs a page directory that is
   dedicated to the user window, which map_supervisor() guarantees. */
static void map_range(uint32_t from, uint32_t to, unsigned int user)
{
    for (uint32_t v = from & ~(PAGE_SIZE - 1); v < to; v += PAGE_SIZE)
    {
        uint32_t pdi = v >> 22;
        if (!(page_dir[pdi] & 1))
        {
            uint32_t pt = alloc_frame();
            if (!pt)
                return;
            memset((void *)(uint32_t)pt, 0, PAGE_SIZE);
            page_dir[pdi] = pt | 0x3 | (user ? 0x4 : 0);
        }
        else if (!user)
        {
            page_dir[pdi] &= ~0x4u;       /* keep user pages out of ring 0 */
        }
        uint32_t *pte = (uint32_t *)(page_dir[pdi] & 0xFFFFF000);
        pte[(v >> 12) & 0x3FF] = v | 0x3 | (user ? 0x4 : 0);
    }
}

/* Identity-map a supervisor range, punching out the ring-3 window so that
   the map_range() call for the window is the one that creates those page
   directory entries, and can therefore set U/S on them. */
static void map_supervisor(uint32_t from, uint32_t to)
{
    if (from >= to)
        return;
    if (from < USER_BASE)                      /* everything below the window */
        map_range(from, to < USER_BASE ? to : USER_BASE, 0);
    if (to > USER_TOP)                         /* everything above the window */
        map_range(from > USER_TOP ? from : USER_TOP, to, 0);
}

static void map_usable(void)
{
    struct e820_entry *e = (struct e820_entry *)E820_BUF_ADDR;
    for (uint32_t i = 0; i < e820_count(); i++)
    {
        if (e[i].type != E820_USABLE || e[i].base_hi != 0)
            continue;
        uint32_t base = e[i].base_lo;
        uint32_t len = e[i].len_lo;
        if (base + len < base || base + len <= LOW_END)
            continue;
        if (base < LOW_END)
            base = LOW_END;
        map_supervisor(base, base + len);
    }
}

void paging_init(void)
{
    memset(page_dir, 0, sizeof page_dir);

    /* low 1 MB: kernel code, kernel stack, VGA text buffer, bootloader */
    map_range(0, LOW_END, 0);

    /* identity-map all usable RAM as supervisor, minus the ring-3 window */
    map_usable();

    /* The ring-3 window is deliberately left unmapped here. vm_create()
       gives each process its own page directory entries for it, so two
       programs can never share pages. */

#ifdef PAGER_TEST
    {
        extern int ata_write_sectors(unsigned int, unsigned int, const void *);
        unsigned int dbg[16];
        unsigned int *pdi0 = (page_dir[0] & 1) ? (unsigned int *)(page_dir[0] & 0xFFFFF000) : 0;
        dbg[0] = e820_count();
        dbg[1] = mem_top;
        dbg[2] = nframes;
        dbg[3] = frames_free;
        dbg[4] = page_dir[0];
        dbg[5] = pdi0 ? pdi0[(0x899d >> 12) & 0x3FF] : 0;
        dbg[6] = pdi0 ? pdi0[(0x7e00 >> 12) & 0x3FF] : 0;
        dbg[7] = 0xCC00;
        dbg[8] = (unsigned int)pdi0;
        dbg[9]  = page_dir[USER_BASE >> 22];   /* 0: kernel dir has no user pages */
        dbg[10] = pdi0 ? pdi0[(0xB8000 >> 12) & 0x3FF] : 0;   /* VGA, must stay kernel */
        dbg[11] = pdi0 ? pdi0[(0x90000 >> 12) & 0x3FF] : 0;
        /* round-trip a process directory: fresh user PDE, US=1, no pages */
        {
            uint32_t d = vm_create();
            dbg[12] = d;
            if (d)
            {
                dbg[13] = ((uint32_t *)d)[USER_BASE >> 22];
                dbg[14] = d ? (((uint32_t *)(dbg[13] & 0xFFFFF000))[(USER_BASE >> 12) & 0x3FF]) : 0;
                vm_destroy(d);
            }
        }
        dbg[15] = frames_free;   /* must match dbg[3] after the round trip */
        ata_write_sectors(700, 1, dbg);
    }
#endif

    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000 | 0x00010000;   /* PG | WP */
    __asm__ volatile("mov %0, %%cr3" : : "r"((uint32_t)page_dir));
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));
    __asm__ volatile("mov %0, %%cr3" : : "r"((uint32_t)page_dir));  /* flush TLB */
}

uint32_t memory_total_kb(void)
{
    return mem_top / 1024;
}

uint32_t memory_free_frames(void)
{
    return frames_free;
}

uint32_t memory_total_frames(void)
{
    return nframes;
}

uint32_t memory_e820_mismatches(void)
{
    return e820_mismatches;
}

/* ------------------------------------------------------------------ *
 * Per-process address spaces
 *
 * The kernel runs identity mapped out of the low 1 MB and all of usable
 * RAM, and every process needs that same map underneath it. So a process
 * directory starts life as a byte-for-byte copy of the kernel directory,
 * and only the ring-3 window is replaced with fresh, empty page tables.
 * Nothing in the user window is mapped until vm_fault() is asked for it.
 * ------------------------------------------------------------------ */

#define USER_PDE_FIRST (USER_BASE >> 22)
#define USER_PDE_LAST  ((USER_TOP - 1) >> 22)

static uint32_t cur_dir;        /* process dir, 0 while only the kernel runs */
static uint32_t brk;            /* program break for the current process     */
static uint32_t fault_count;

void vm_switch(uint32_t dir)
{
    __asm__ volatile("mov %0, %%cr3" : : "r"(dir));
}

uint32_t vm_create(void)
{
    uint32_t d = alloc_frame();
    if (!d)
        return 0;

    /* identity mapped, so the frame address is also its physical address */
    memcpy((void *)d, page_dir, sizeof page_dir);

    for (uint32_t i = USER_PDE_FIRST; i <= USER_PDE_LAST; i++)
    {
        uint32_t pt = alloc_frame();
        if (!pt)
        {
            free_frame(d);
            return 0;
        }
        memset((void *)pt, 0, PAGE_SIZE);
        ((uint32_t *)d)[i] = pt | 0x3 | 0x4;   /* present, RW, user */
    }

    brk = 0;
    return d;
}

void vm_destroy(uint32_t dir)
{
    if (!dir || dir == (uint32_t)page_dir)
        return;

    uint32_t *pd = (uint32_t *)dir;
    for (uint32_t i = USER_PDE_FIRST; i <= USER_PDE_LAST; i++)
    {
        if (!(pd[i] & 1))
            continue;
        uint32_t *pte = (uint32_t *)(pd[i] & 0xFFFFF000);
        for (int j = 0; j < 1024; j++)
            if (pte[j] & 1)
                free_frame(pte[j] & 0xFFFFF000);
        free_frame(pd[i] & 0xFFFFF000);
        pd[i] = 0;
    }
    free_frame(dir);
}

void vm_attach(uint32_t dir)
{
    cur_dir = dir;
    vm_switch(dir);
}

void vm_detach(void)
{
    uint32_t old = cur_dir;
    if (!old)
        return;
    /* back to the kernel map first: it has to stay valid while we free */
    cur_dir = 0;
    vm_switch((uint32_t)page_dir);
    vm_destroy(old);
}

int vm_fault(uint32_t addr)
{
    /* only the ring-3 window is ours to fill in */
    if (addr < USER_BASE || addr >= USER_TOP)
        return 0;

    uint32_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    uint32_t *pd = (uint32_t *)(cr3 & ~0xFFFu);

    uint32_t pdi = addr >> 22;
    if (!(pd[pdi] & 1))
        return 0;

    uint32_t *pte = &((uint32_t *)(pd[pdi] & 0xFFFFF000))[(addr >> 12) & 0x3FF];

    /* already present means a protection problem, not a missing page:
       reading a supervisor page from ring 3 lands here and must stay fatal */
    if (*pte & 1)
        return 0;

    uint32_t frame = alloc_frame();
    if (!frame)
        return 0;

    /* zero fill, so a program can never read leftover kernel memory */
    memset((void *)frame, 0, PAGE_SIZE);
    *pte = frame | 0x3 | 0x4;                    /* present, RW, user */
    fault_count++;

    /* the faulting lookup left a stale entry in the TLB: invalidate the
       one page we just added, then the CPU will retry the instruction */
    __asm__ volatile("invlpg (%0)" : : "r"(addr) : "memory");
    return 1;
}

void vm_note_load(uint32_t size)
{
    brk = USER_BASE + ((size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));
    if (brk < USER_BASE)
        brk = USER_BASE;
}

uint32_t vm_sbrk(uint32_t inc)
{
    if (!inc)
        return brk;
    if (inc > USER_TOP - brk)          /* also catches overflow */
        return brk;
    if (brk + inc > USER_STACK)        /* never grow into the stack */
        return brk;
    brk += inc;
    return brk;
}

uint32_t vm_faults(void)
{
    return fault_count;
}