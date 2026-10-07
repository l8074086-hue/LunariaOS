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

/* --- open file handles ------------------------------------------------- */
#define FS_MAX_OPEN 8
#define FS_FD_BASE  3        /* fds 0-2 are reserved for a future stdio */

/* open() flags */
#define FS_O_RDONLY 0x0001
#define FS_O_WRONLY 0x0002
#define FS_O_RDWR   0x0004
#define FS_O_CREAT  0x0008
#define FS_O_TRUNC  0x0010
#define FS_O_APPEND 0x0020

/* lseek() whence */
#define FS_SEEK_SET 0
#define FS_SEEK_CUR 1
#define FS_SEEK_END 2

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

/* Read handles stream straight from the disk. A writable handle keeps the
   whole file in a kernel buffer and writes it back on close(). Only one
   writable handle can be open at once, since they share that buffer. */
int  fs_open(const char *name, int flags);
int  fs_read_fd(int fd, char *buf, unsigned int n);
int  fs_write_fd(int fd, const char *buf, unsigned int n);
int  fs_seek(int fd, int off, int whence);
int  fs_close(int fd);
void fs_close_all(void);
int  fs_unlink(const char *name);

#endif
