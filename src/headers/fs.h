#ifndef FS_H
#define FS_H
#define FS_MAGIC "LUNFS1"
#define FS_MAGIC_LEN 8
#define FS_SUPER_LBA 128   /* above the kernel region; kernel must stay < 128 sectors */
#define FS_DIR_LBA 200
#define FS_DIR_SECTORS 64
#define FS_DATA_LBA 300
#define FS_MAX_NAME 23
#define FS_ENTRIES_PER_SECTOR 16
#define FS_DIR 1
#define FS_FILE 0
struct superblock
{
    char magic[FS_MAGIC_LEN];     /* "LUNFS1" — lets us verify the disk */
    unsigned int dir_lba;         /* where the directory lives */
    unsigned int dir_sectors;
    unsigned int data_lba;        /* where file contents go */
};
struct file_entry
{
    char name[FS_MAX_NAME];
    unsigned char type;
    unsigned int lba;
    unsigned int size;
};

int fs_mount(void);
int fs_ls(const char *name);
int fs_find(const char *name, struct file_entry *out);
int fs_write(const char *name, unsigned char type, const char *data, unsigned int size);
int fs_mkdir(const char *name);
int fs_delete(const char *name);
int fs_cat(const char *name);
int fs_read(const char *name, char *buf, unsigned int max);
int fs_ls_buf(const char *name, char *buf, unsigned int max);
int fs_append(const char *dir, const char *path);

#endif
