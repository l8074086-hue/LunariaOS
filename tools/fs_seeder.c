#include <stdio.h>
#include <string.h>
#include "../src/headers/fs.h"

struct superblock sb;

int main(int argc, char **argv)
{
  strcpy(sb.magic, FS_MAGIC);
  sb.dir_lba = FS_DIR_LBA;
  sb.dir_sectors = FS_DIR_SECTORS;
  sb.data_lba = FS_DATA_LBA;
  FILE *f = fopen("bin/disk.img", "r+b");
  if (!f)
    return 1;
  char zeros[512] = {0};
  struct file_entry entry1 = {0}, entry2 = {0};
  strcpy(entry1.name, "readme.txt");
  strcpy(entry2.name, "hello.txt");

  fseek(f, FS_DIR_LBA * 512, SEEK_SET);
  for (int i = 0; i < FS_DIR_SECTORS; i++)
    fwrite(zeros, 1, sizeof zeros, f);
  fseek(f, FS_SUPER_LBA * 512, SEEK_SET);
  fwrite(&sb, sizeof sb, 1, f);
  int next_lba = FS_DATA_LBA;
  // FILE 1
  fseek(f, next_lba * 512, SEEK_SET);
  const char *text = "Welcome to LunariaOS!\n";
  fwrite(text, 1, strlen(text), f);
  entry1.lba = next_lba;
  entry1.size = strlen(text);
  next_lba += 1;

  // FILE 2
  fseek(f, next_lba * 512, SEEK_SET);
  const char *text2 = "Hello from filesystem!\n";
  fwrite(text2, 1, strlen(text2), f);
  entry2.lba = next_lba;
  entry2.size = strlen(text2);
  next_lba += 1;

  // PROGRAMS from argv
  struct file_entry entries[FS_ENTRIES_PER_SECTOR];
  memset(entries, 0, sizeof entries);
  int n_entries = 2;
  for (int i = 1; i < argc; i++)
  {
    FILE *prog = fopen(argv[i], "rb");
    if (!prog)
    {
      fprintf(stderr, "fs_seeder: cannot open %s\n", argv[i]);
      continue;
    }
    fseek(prog, 0, SEEK_END);
    long psize = ftell(prog);
    fseek(prog, 0, SEEK_SET);

    const char *base = strrchr(argv[i], '/');
    base = base ? base + 1 : argv[i];
    if (strncmp(base, "prog_", 5) == 0)
      base += 5;
    char name[23];
    memset(name, 0, sizeof name);
    strncpy(name, base, sizeof name - 1);
    char *dot = strrchr(name, '.');
    if (dot && dot != name)
      *dot = '\0';

    entries[n_entries].lba = next_lba;
    entries[n_entries].size = psize;
    strcpy(entries[n_entries].name, name);
    entries[n_entries].type = 1;

    fseek(f, next_lba * 512, SEEK_SET);
    char pbuf[512];
    long remaining = psize;
    while (remaining > 0)
    {
      memset(pbuf, 0, sizeof pbuf);
      size_t chunk = remaining > 512 ? 512 : remaining;
      fread(pbuf, 1, chunk, prog);
      fwrite(pbuf, 1, 512, f);
      remaining -= chunk;
      next_lba++;
    }
    fclose(prog);
    n_entries++;
  }

  fseek(f, FS_DIR_LBA * 512, SEEK_SET);
  fwrite(&entry1, sizeof entry1, 1, f);
  fwrite(&entry2, sizeof entry2, 1, f);
  fwrite(entries + 2, sizeof(struct file_entry), n_entries - 2, f);

  fclose(f);
}
