#ifndef ATA_H
#define ATA_H

int ata_read_sectors(unsigned int lba, unsigned int count, void *buf);
int ata_write_sectors(unsigned int lba, unsigned int count, const void *buf);

#endif
