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
  struct file_entry entry1 = {0}, entry2 = {0}, entry3 = {0};
  strcpy(entry1.name, "readme.txt");
  strcpy(entry2.name, "hello.txt");
  strcpy(entry3.name, "demo.c");

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
  next_lba += (strlen(text) + 511) / 512;

  // FILE 2
  fseek(f, next_lba * 512, SEEK_SET);
  const char *text2 = "Hello from filesystem!\n";
  fwrite(text2, 1, strlen(text2), f);
  entry2.lba = next_lba;
  entry2.size = strlen(text2);
  next_lba += (strlen(text2) + 511) / 512;

  // FILE 3 (demo source for the on-device tcc compiler)
  fseek(f, next_lba * 512, SEEK_SET);
  const char *text3 =
      "/* tcc demo: print fib(10) when a shell argument starts with 'f',\n"
      "   otherwise print the argument count main() received. print() and\n"
      "   print_dec() exist in both the compile-and-run symbol table and the\n"
      "   flat -o image runtime, so the same file works either way. */\n"
      "int magic(int n) { return n < 2 ? n : magic(n - 1) + magic(n - 2); }\n"
      "int main(int argc, char **argv)\n"
      "{\n"
      "    if (argc >= 2 && argv[1][0] == 'f')\n"
      "    {\n"
      "        int r = magic(10);\n"
      "        print(\"demo: fib(10)=\");\n"
      "        print_dec(r);\n"
      "        print(\"\\n\");\n"
      "        return r;\n"
      "    }\n"
      "    print(\"demo: argc=\");\n"
      "    print_dec(argc);\n"
      "    print(\"\\n\");\n"
      "    return argc;\n"
      "}\n";
  fwrite(text3, 1, strlen(text3), f);
  entry3.lba = next_lba;
  entry3.size = strlen(text3);
  next_lba += (strlen(text3) + 511) / 512;

  // PROGRAMS from argv
  struct file_entry entries[FS_ENTRIES_PER_SECTOR];
  memset(entries, 0, sizeof entries);
  int n_entries = 2;
  entries[2] = entry3;
  n_entries = 3;
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
