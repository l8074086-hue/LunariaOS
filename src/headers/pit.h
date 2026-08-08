#ifndef PIT_H
#define PIT_H

void pit_init(unsigned int hz);
void pit_handler(void);
unsigned int pit_ticks(void);
unsigned int pit_uptime_sec(void);
void pit_sleep(unsigned int ms);
unsigned int pit_uptime_sec(void);

#endif
