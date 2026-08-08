#include "pit.h"
#include "port.h"

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_FREQ     1193182

static volatile unsigned int ticks;
static unsigned int pit_hz;

void pit_init(unsigned int hz)
{
    pit_hz = hz;
    unsigned int divisor = PIT_FREQ / hz;

    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);
}

void pit_handler(void)
{
    ticks++;
}

unsigned int pit_ticks(void)
{
    return ticks;
}

unsigned int pit_uptime_sec(void) { return ticks / pit_hz; }

void pit_sleep(unsigned int ms)
{
    unsigned int add = (pit_hz / 1000) * ms;
    unsigned int rem = (pit_hz % 1000) * ms;
    unsigned int target = ticks + add + rem / 1000;

    while (ticks < target)
        __asm__ volatile("hlt");
}
