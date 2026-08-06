#include "fs.h"
#include "string.h"
#include "ata.h"
#include "vga.h"
#include "wm.h"

static struct superblock sb;

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
        slot_sec = sec;
        slot_i = i;
      }
    }
  }

  if (slot_sec < 0)
    return -1;

  unsigned int sectors = (size + 511) / 512;
  for (unsigned int k = 0; k < sectors; k++)
  {
    char pad[512];
    memset(pad, 0, sizeof pad);
    unsigned int chunk = size - k * 512;
    if (chunk > 512)
      chunk = 512;
    memcpy(pad, data + k * 512, chunk);
    if (ata_write_sectors(next_free + k, 1, pad) != 0)
      return -1;
  }

  struct file_entry new_entry;
  memset(&new_entry, 0, sizeof new_entry);
  strcpy(new_entry.name, name);
  new_entry.type = type;
  new_entry.lba = next_free;
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
