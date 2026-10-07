#include "fs.h"
#include "string.h"
#include "ata.h"
#include "vga.h"
#include "wm.h"

static struct superblock sb;

/* --- open file handles -------------------------------------------------- */

#define FD_BUF_SIZE 65536

struct fs_handle
{
  int used;
  int writable;
  int dirty;
  unsigned char type;
  char name[FS_MAX_NAME];
  unsigned int lba;       /* read-only: first sector on disk            */
  unsigned int size;      /* read-only: file size in bytes              */
  unsigned int wlen;      /* writable: bytes held in wbuf               */
  unsigned int offset;    /* current file position                      */
};

/* Only one writable handle may be open at a time, so it gets the whole 64K
   buffer to itself; read handles need only the bookkeeping above. */
static struct fs_handle handles[FS_MAX_OPEN];
static char wbuf[FD_BUF_SIZE];

static struct fs_handle *fd_lookup(int fd)
{
  int i = fd - FS_FD_BASE;
  if (i < 0 || i >= FS_MAX_OPEN || !handles[i].used)
    return 0;
  return &handles[i];
}

int fs_mount(void)
{
  char sec[512];
  if (ata_read_sectors(FS_SUPER_LBA, 1, sec) != 0)
    return -1;
  memcpy(&sb, sec, sizeof sb);

  if (strcmp(sb.magic, FS_MAGIC) != 0)
    return -1;

  return 0;
}

#define DIR_BUF_SIZE 2048

static void parent_of(const char *path, char *out)
{
  const char *slash = 0;
  for (const char *p = path; *p; p++)
    if (*p == '/')
      slash = p;
  if (!slash)
  {
    out[0] = '\0';
    return;
  }
  unsigned int n = slash - path;
  memcpy(out, path, n);
  out[n] = '\0';
}

int fs_append(const char *dir, const char *path)
{
  struct file_entry e;
  if (fs_find(dir, &e) != 0)
    return -1;

  char buf[DIR_BUF_SIZE];
  if (e.size >= sizeof buf)
    return -1;

  unsigned int off = 0;
  unsigned int remaining = e.size;
  while (remaining > 0)
  {
    char sec[512];
    if (ata_read_sectors(e.lba + off / 512, 1, sec) != 0)
      return -1;
    unsigned int n = remaining < 512 ? remaining : 512;
    memcpy(buf + off, sec, n);
    off += n;
    remaining -= n;
  }

  unsigned int path_len = strlen(path);
  if (off + path_len + 1 >= sizeof buf)
    return -1;
  for (unsigned int k = 0; k < path_len; k++)
    buf[off++] = path[k];
  buf[off++] = '\n';

  return fs_write(dir, FS_DIR, buf, off);
}

static int has_slash(const char *s)
{
  for (; *s; s++)
    if (*s == '/')
      return 1;
  return 0;
}

int fs_ls(const char *name)
{
  if (name && name[0])
  {
    struct file_entry e;
    if (fs_find(name, &e) != 0)
      return -1;

    unsigned int skip = strlen(name) + 1;
    unsigned int skip_left = skip;
    char sec[512];
    unsigned int remaining = e.size;
    while (remaining > 0)
    {
      if (ata_read_sectors(e.lba++, 1, sec) != 0)
        return -1;
      unsigned int n = remaining < 512 ? remaining : 512;
      for (unsigned int k = 0; k < n; k++)
      {
        char c = sec[k];
        if (skip_left > 0)
        {
          skip_left--;
          continue;
        }
        term_putchar(wm_current(), c, VGA_COLOR(BLACK, WHITE));
        if (c == '\n')
          skip_left = skip;
      }
      remaining -= n;
    }
    return 0;
  }

  char buf[512];

  for (unsigned int sec = 0; sec < sb.dir_sectors; sec++)
  {
    if (ata_read_sectors(sb.dir_lba + sec, 1, buf) != 0)
      return -1;

    for (int i = 0; i < FS_ENTRIES_PER_SECTOR; i++)
    {
      struct file_entry *e = (struct file_entry *)&buf[i * sizeof(struct file_entry)];
      if (e->name[0] == '\0')
        continue;
      if (has_slash(e->name))
        continue;

      term_print_color(wm_current(), e->name, VGA_COLOR(BLACK, WHITE));
      term_print_color(wm_current(), "  ", VGA_COLOR(BLACK, WHITE));
      char size_buf[12];
      itoa(e->size, size_buf);
      term_print_color(wm_current(), size_buf, VGA_COLOR(BLACK, WHITE));
      term_print_color(wm_current(), " bytes\n", VGA_COLOR(BLACK, WHITE));
    }
  }
  return 0;
}

int fs_ls_buf(const char *name, char *buf, unsigned int max)
{
  unsigned int o = 0;

  if (name && name[0])
  {
    struct file_entry e;
    if (fs_find(name, &e) != 0)
      return -1;

    unsigned int skip = strlen(name) + 1;
    unsigned int skip_left = skip;
    char sec[512];
    unsigned int remaining = e.size;
    while (remaining > 0)
    {
      if (ata_read_sectors(e.lba++, 1, sec) != 0)
        return -1;
      unsigned int n = remaining < 512 ? remaining : 512;
      for (unsigned int k = 0; k < n; k++)
      {
        char c = sec[k];
        if (skip_left > 0)
        {
          skip_left--;
          continue;
        }
        if (o < max - 1)
          buf[o++] = c;
        if (c == '\n')
          skip_left = skip;
      }
      remaining -= n;
    }
    buf[o] = '\0';
    return (int)o;
  }

  char dirbuf[512];
  for (unsigned int sec = 0; sec < sb.dir_sectors; sec++)
  {
    if (ata_read_sectors(sb.dir_lba + sec, 1, dirbuf) != 0)
      return -1;

    for (int i = 0; i < FS_ENTRIES_PER_SECTOR; i++)
    {
      struct file_entry *e = (struct file_entry *)&dirbuf[i * sizeof(struct file_entry)];
      if (e->name[0] == '\0')
        continue;
      if (has_slash(e->name))
        continue;

      char size_buf[12];
      itoa(e->size, size_buf);
      unsigned int l = strlen(e->name);
      unsigned int sl = strlen(size_buf);
      if (o + l + sl + 2 >= max)
      {
        buf[o] = '\0';
        return (int)o;
      }
      for (unsigned int k = 0; k < l; k++)
        buf[o++] = e->name[k];
      buf[o++] = ' ';
      for (unsigned int k = 0; k < sl; k++)
        buf[o++] = size_buf[k];
      buf[o++] = '\n';
    }
  }
  buf[o] = '\0';
  return (int)o;
}

int fs_find(const char *name, struct file_entry *out)
{
  char buf[512];

  for (unsigned int sec = 0; sec < sb.dir_sectors; sec++)
  {
    if (ata_read_sectors(sb.dir_lba + sec, 1, buf) != 0)
      return -1;

    for (int i = 0; i < FS_ENTRIES_PER_SECTOR; i++)
    {
      struct file_entry *e = (struct file_entry *)&buf[i * sizeof(struct file_entry)];
      if (e->name[0] == '\0')
        continue;
      if (strcmp(e->name, name) == 0)
      {
        *out = *e;
         return 0;
      }
    }
  }
  return -1;
}

int fs_cat(const char *name)
{
  struct file_entry e;
  if (fs_find(name, &e) != 0)
    return -1;
  char buf[512];
  unsigned int remaining = e.size;
  while (remaining > 0)
  {
    if (ata_read_sectors(e.lba++, 1, buf) != 0)
      return -1;
    unsigned int n = remaining < 512 ? remaining : 512;
    buf[n] = '\0';
    term_print_color(wm_current(), buf, VGA_COLOR(BLACK,WHITE));
    remaining -= n;
  }
  term_print_color(wm_current(), "\n", VGA_COLOR(BLACK, WHITE));
  return 0;
}

int fs_read(const char *name, char *buf, unsigned int max)
{
  struct file_entry e;
  if (fs_find(name, &e) != 0)
    return -1;
  if (e.size > max)
    return -1;

  unsigned int off = 0;
  unsigned int remaining = e.size;
  while (remaining > 0)
  {
    char sec[512];
    if (ata_read_sectors(e.lba + off / 512, 1, sec) != 0)
      return -1;
    unsigned int n = remaining < 512 ? remaining : 512;
    memcpy(buf + off, sec, n);
    off += n;
    remaining -= n;
  }
  return (int)e.size;
}

int fs_mkdir(const char *name) 
{
  return fs_write(name, FS_DIR, "", 0);
}

int fs_delete(const char *name)
{
  char buf[512];

  for (unsigned int sec = 0; sec < sb.dir_sectors; sec++)
  {
    if (ata_read_sectors(sb.dir_lba + sec, 1, buf) != 0)
      return -1;

    for (int i = 0; i < FS_ENTRIES_PER_SECTOR; i++)
    {
      struct file_entry *e = (struct file_entry *)&buf[i * sizeof(struct file_entry)];
      if (e->name[0] == '\0')
        continue;
      if (strcmp(e->name, name) == 0)
      {
        e->name[0] = '\0';
        if (ata_write_sectors(sb.dir_lba + sec, 1, buf) != 0)
          return -1;
        return 0;
      }
    }
  }
  return -1;
}

int fs_write(const char *name, unsigned char type, const char *data, unsigned int size)
{
  if (strlen(name) >= FS_MAX_NAME)
    return -1;

  if (has_slash(name))
  {
    char parent[FS_MAX_NAME];
    parent_of(name, parent);
    if (parent[0])
    {
      struct file_entry pe;
      if (fs_find(parent, &pe) != 0)
        return -1;
    }
  }

  char buf[512];
  int slot_sec = -1, slot_i = -1;
  int found = 0;
  int old_exists = 0;
  unsigned int old_lba = 0, old_cap = 0;
  unsigned int next_free = sb.data_lba;

  for (unsigned int sec = 0; sec < sb.dir_sectors; sec++)
  {
    if (ata_read_sectors(sb.dir_lba + sec, 1, buf) != 0)
      return -1;

    for (int i = 0; i < FS_ENTRIES_PER_SECTOR; i++)
    {
      struct file_entry *e = (struct file_entry *)&buf[i * sizeof(struct file_entry)];

      if (e->name[0] == '\0')
      {
        if (slot_sec < 0)
        {
          slot_sec = sec;
          slot_i = i;
        }
        continue;
      }

      unsigned int end = e->lba + (e->size + 511) / 512;
      if (end > next_free)
        next_free = end;

      if (!found && strcmp(e->name, name) == 0)
      {
        found = 1;
        old_exists = 1;
        old_lba = e->lba;
        old_cap = (e->size + 511) / 512;
        slot_sec = sec;
        slot_i = i;
      }
    }
  }

  if (slot_sec < 0)
    return -1;

  unsigned int sectors = (size + 511) / 512;

  /* Rewriting a file in place when the new contents still fit its old
     sectors avoids leaking a fresh region on every save. Growing past the
     old allocation falls back to a new region at the end of the disk, which
     leaves the old sectors as a hole (there is no free-space allocator yet). */
  unsigned int write_lba = (old_exists && sectors <= old_cap) ? old_lba : next_free;

  for (unsigned int k = 0; k < sectors; k++)
  {
    char pad[512];
    memset(pad, 0, sizeof pad);
    unsigned int chunk = size - k * 512;
    if (chunk > 512)
      chunk = 512;
    memcpy(pad, data + k * 512, chunk);
    if (ata_write_sectors(write_lba + k, 1, pad) != 0)
      return -1;
  }

  struct file_entry new_entry;
  memset(&new_entry, 0, sizeof new_entry);
  strcpy(new_entry.name, name);
  new_entry.type = type;
  new_entry.lba = write_lba;
  new_entry.size = size;

  if (ata_read_sectors(sb.dir_lba + slot_sec, 1, buf) != 0)
    return -1;
  struct file_entry *slot = (struct file_entry *)&buf[slot_i * sizeof(struct file_entry)];
  *slot = new_entry;
  if (ata_write_sectors(sb.dir_lba + slot_sec, 1, buf) != 0)
    return -1;

  if (has_slash(name))
  {
    char parent[FS_MAX_NAME];
    parent_of(name, parent);
    if (parent[0])
      fs_append(parent, name);
  }

  return 0;
}

/* --- open file handles -------------------------------------------------- */

int fs_open(const char *name, int flags)
{
  if (strlen(name) >= FS_MAX_NAME)
    return -1;

  struct file_entry e;
  int exists = (fs_find(name, &e) == 0);
  int want_write = (flags & (FS_O_WRONLY | FS_O_RDWR)) != 0;

  if (!exists && !(flags & FS_O_CREAT))
    return -1;
  if (exists && e.type == FS_DIR)
    return -1;

  struct fs_handle *h = 0;
  for (int i = 0; i < FS_MAX_OPEN; i++)
  {
    if (!handles[i].used)
    {
      h = &handles[i];
      break;
    }
  }
  if (!h)
    return -1;

  memset(h, 0, sizeof *h);
  h->used = 1;
  h->writable = want_write;
  strcpy(h->name, name);

  if (!want_write)
  {
    if (exists)
    {
      h->type = e.type;
      h->lba = e.lba;
      h->size = e.size;
    }
    return FS_FD_BASE + (int)(h - handles);
  }

  /* a writable handle owns wbuf, so only one may exist at a time */
  for (int i = 0; i < FS_MAX_OPEN; i++)
  {
    if (&handles[i] != h && handles[i].used && handles[i].writable)
    {
      h->used = 0;
      return -1;
    }
  }

  h->type = FS_FILE;
  if (exists && !(flags & FS_O_TRUNC))
  {
    if (e.size > FD_BUF_SIZE || fs_read(name, wbuf, FD_BUF_SIZE) < 0)
    {
      h->used = 0;
      return -1;
    }
    h->wlen = e.size;
  }
  h->offset = (flags & FS_O_APPEND) ? h->wlen : 0;
  return FS_FD_BASE + (int)(h - handles);
}

int fs_read_fd(int fd, char *buf, unsigned int n)
{
  struct fs_handle *h = fd_lookup(fd);
  if (!h)
    return -1;

  if (h->writable)
  {
    unsigned int avail = h->offset < h->wlen ? h->wlen - h->offset : 0;
    if (n > avail)
      n = avail;
    memcpy(buf, wbuf + h->offset, n);
    h->offset += n;
    return (int)n;
  }

  unsigned int avail = h->offset < h->size ? h->size - h->offset : 0;
  if (n > avail)
    n = avail;
  if (n == 0)
    return 0;

  unsigned int done = 0;
  while (done < n)
  {
    char sec[512];
    unsigned int pos = h->offset + done;
    unsigned int in = pos % 512;
    if (ata_read_sectors(h->lba + pos / 512, 1, sec) != 0)
      return done ? (int)done : -1;
    unsigned int take = 512 - in;
    if (take > n - done)
      take = n - done;
    memcpy(buf + done, sec + in, take);
    done += take;
  }
  h->offset += done;
  return (int)done;
}

int fs_write_fd(int fd, const char *buf, unsigned int n)
{
  struct fs_handle *h = fd_lookup(fd);
  if (!h || !h->writable)
    return -1;
  if (n > FD_BUF_SIZE || h->offset > FD_BUF_SIZE - n)
    return -1;

  if (h->offset + n > h->wlen)
  {
    /* a seek past the end leaves a zero-filled gap */
    if (h->offset > h->wlen)
      memset(wbuf + h->wlen, 0, h->offset - h->wlen);
    h->wlen = h->offset + n;
  }
  memcpy(wbuf + h->offset, buf, n);
  h->offset += n;
  h->dirty = 1;
  return (int)n;
}

int fs_seek(int fd, int off, int whence)
{
  struct fs_handle *h = fd_lookup(fd);
  if (!h)
    return -1;

  int base;
  if (whence == FS_SEEK_SET)
    base = 0;
  else if (whence == FS_SEEK_CUR)
    base = (int)h->offset;
  else if (whence == FS_SEEK_END)
    base = (int)(h->writable ? h->wlen : h->size);
  else
    return -1;

  int pos = base + off;
  if (pos < 0)
    return -1;
  h->offset = (unsigned int)pos;
  return pos;
}

int fs_close(int fd)
{
  struct fs_handle *h = fd_lookup(fd);
  if (!h)
    return -1;

  int rc = 0;
  if (h->writable)
    rc = fs_write(h->name, h->type, wbuf, h->wlen);
  h->used = 0;
  return rc;
}

void fs_close_all(void)
{
  for (int i = 0; i < FS_MAX_OPEN; i++)
    if (handles[i].used)
      fs_close(FS_FD_BASE + i);
}

int fs_unlink(const char *name)
{
  return fs_delete(name);
}
