#include "ata.h"
#include "port.h"

#define ATA_DATA      0x1F0
#define ATA_SECTOR_CNT 0x1F2
#define ATA_LBA_LO     0x1F3
#define ATA_LBA_MID    0x1F4
#define ATA_LBA_HI     0x1F5
#define ATA_DRIVE      0x1F6
#define ATA_STATUS     0x1F7
#define ATA_CMD_READ   0x20
#define ATA_CMD_WRITE  0x30

#define ATA_STATUS_BSY 0x80
#define ATA_STATUS_DRQ 0x08
#define ATA_STATUS_ERR 0x01

static int ata_wait(void)
{
    unsigned char status;

    for (int i = 0; i < 100000; i++)
    {
        status = inb(ATA_STATUS);
        if (!(status & ATA_STATUS_BSY))
        {
            if (status & ATA_STATUS_ERR)
                return -1;
            return 0;
        }
    }

    return -1;
}

static int ata_wait_drq(void)
{
    unsigned char status;

    for (int i = 0; i < 100000; i++)
    {
        status = inb(ATA_STATUS);
        if (status & ATA_STATUS_ERR)
            return -1;
        if (!(status & ATA_STATUS_BSY) && (status & ATA_STATUS_DRQ))
            return 0;
    }

    return -1;
}

int ata_write_sectors(unsigned int lba, unsigned int count, const void *buf)
{
    const unsigned char *ptr = (const unsigned char *)buf;

    while (count > 0)
    {
        if (ata_wait() < 0)
            return -1;

        outb(ATA_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
        outb(ATA_SECTOR_CNT, 1);
        outb(ATA_LBA_LO, lba & 0xFF);
        outb(ATA_LBA_MID, (lba >> 8) & 0xFF);
        outb(ATA_LBA_HI, (lba >> 16) & 0xFF);
        outb(ATA_STATUS, ATA_CMD_WRITE);

        if (ata_wait_drq() < 0)
            return -1;

        for (int i = 0; i < 256; i++)
        {
            unsigned short w = ptr[0] | (ptr[1] << 8);
            outw(ATA_DATA, w);
            ptr += 2;
        }

        if (ata_wait() < 0)
            return -1;

        lba++;
        count--;
    }

    return 0;
}

int ata_read_sectors(unsigned int lba, unsigned int count, void *buf)
{
    unsigned char *ptr = (unsigned char *)buf;

    while (count > 0)
    {
        if (ata_wait() < 0)
            return -1;

        outb(ATA_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
        outb(ATA_SECTOR_CNT, 1);
        outb(ATA_LBA_LO, lba & 0xFF);
        outb(ATA_LBA_MID, (lba >> 8) & 0xFF);
        outb(ATA_LBA_HI, (lba >> 16) & 0xFF);
        outb(ATA_STATUS, ATA_CMD_READ);

        if (ata_wait() < 0)
            return -1;

        for (int i = 0; i < 256; i++)
        {
            unsigned short w = inw(ATA_DATA);
            *ptr++ = w & 0xFF;
            *ptr++ = (w >> 8) & 0xFF;
        }

        lba++;
        count--;
    }

    return 0;
}
