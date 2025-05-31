/*
 * Copyright (C) 2025  notweerdmonk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
/**
 * @file gdblint.c
 * @brief Lints GDB scripts
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>
#include <stdbool.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include <getopt.h>
#include <libgen.h>
#include <sys/utsname.h>
#include <errno.h>

/* Convenience */

/* Mark a function or variable or function parameter as unused */
#define UNUSED __attribute__((unused))

/* Set the cleanup function for a variable */
#define CLEANUP(f) __attribute__((cleanup (f)))

/* Logstuff */

#define LOG_LVL_NON 0
#define LOG_LVL_ERR (LOG_LVL_NON + 1)
#define LOG_LVL_WRN (LOG_LVL_ERR + 1)
#define LOG_LVL_LOG (LOG_LVL_WRN + 1)
#define LOG_LVL_DBG (LOG_LVL_LOG + 1)

#define base(fptr, typ, fmt, ...) \
  fprintf(fptr, "%s: %s: %d: "#typ": "fmt, __FILE__, __func__, __LINE__, \
    ##__VA_ARGS__)

#define err(fmt, ...)
#define ERR_ENABLED 0
#define err_enabled() (false)

#define wrn(fmt, ...)
#define WRN_ENABLED 0
#define wrn_enabled() (false)

#define log(fmt, ...)
#define LOG_ENABLED 0
#define log_enabled() (false)

#define dbg(fmt, ...)
#define DBG_ENABLED 0
#define dbg_enabled() (false)

#if !defined(__LOGEN) && defined(__LOG)
#define __LOGEN __LOG
#elif defined(__LOGEN) && !defined(__LOG)
#define __LOG __LOGEN
#endif

#ifdef __LOG

#if __LOG > (LOG_LVL_ERR - 1)

#undef err
#define err(fmt, ...) base(stderr, ERR, fmt, ##__VA_ARGS__)

#undef ERR_ENABLED
#define ERR_ENABLED 1

#undef err_enabled
#define err_enabled() (true)

#endif

#if __LOG > (LOG_LVL_WRN - 1)

#undef wrn
#define wrn(fmt, ...) base(stderr, WRN, fmt, ##__VA_ARGS__)

#undef WRN_ENABLED
#define WRN_ENABLED 1

#undef wrn_enabled
#define wrn_enabled() (true)

#endif

#if __LOG > (LOG_LVL_LOG - 1)

#undef log
#define log(fmt, ...) base(stderr, LOG, fmt, ##__VA_ARGS__)

#undef LOG_ENABLED
#define LOG_ENABLED 1

#undef log_enabled
#define log_enabled() (true)

#endif

#if __LOG > (LOG_LVL_DBG - 1)

#undef dbg
#define dbg(fmt, ...) base(stderr, DBG, fmt, ##__VA_ARGS__)

#undef DBG_ENABLED
#define DBG_ENABLED 1

#undef dbg_enabled
#define dbg_enabled() (true)

#endif

#endif /* if defined(__LOGEN) || defined(__LOG) */

/* Declarations */

enum constants {
  MAX_LEN = 1024,
  ARCH_LEN = 128,
  MAX_LINES = 2048,
  MAX_ARCHS = 16,
  HASH_SIZE = 1024
};

struct merged_line {
  char line[MAX_LEN];
  size_t orig_linenum;
};

struct lines_map {
  struct merged_line *lines;
  size_t capacity;
  size_t count;
  size_t max_linenum;
};

enum symbol_type {
  VAR,
  FUNC,
  NONE   // because a third value was needed for logic
};

enum action_type {
  LINT = 0,
  SCRIPTABLE,
  LIST_ARCHS,
  CLEAR_CACHE
};

struct symbol {
  char name[MAX_LEN];
  struct symbol *next;
  size_t linenum;
  enum symbol_type type;
};

/* Prefix tree */

#define MAX_COMMAND_LENGTH 100
#define MAX_CHILDREN 94

struct trie_node {
  struct trie_node* children[MAX_CHILDREN];
  int end;
};

static
struct trie_node*
create_node() {
  struct trie_node* node = (struct trie_node*)malloc(sizeof(struct trie_node));
  if (!node) {
    return NULL;
  }

  memset(&node->children, 0, sizeof(node->children));
  node->end = false;

  return node;
}

static
struct trie_node*
insert_command(struct trie_node **root, char* command) {
  if (!root || !command) {
    return NULL;
  }

  struct trie_node *node = *root;
  if (!node) {
    *root = node = create_node();
  }

  while (command && *command) {
    int c = *command++ - '!';

    if(!node->children[c]) {
      node->children[c] = create_node();

    }
    node = node->children[c];
  }

  node->end = true;

  return node;
}

static
struct trie_node*
find_command(struct trie_node *root, const char* command,
    char* result, size_t *len) {

  if (!root || !command) {
    return NULL;
  }

  size_t n = 0;
  struct trie_node* node = root;

  while (command && *command) {
    int c = *command - '!';

    if(!node->children[c]) {
      return NULL;
    }
    node = node->children[c];

    result[n++] = *command++;
  }
  result[n] = '\0';

  if (len) {
    *len = n;
  }

  return node;
}

static
size_t
store_trie(struct trie_node *root, FILE *fp, char *buffer, size_t buflen) {
  if (!root) {
    return 0;
  }

  size_t len = 0;

  for (
      size_t i = 0;
      i < sizeof(root->children) / sizeof(struct trie_node*);
      i++
    ) {
    if (root->children[i]) {
      if (fp) {
        char c = (char)(i + '!');
        size_t nwrite = fwrite(&c, 1, sizeof(c), fp);
        nwrite *= sizeof(c);
        if (nwrite < 1 * sizeof(c)) {
          wrn("error: fwrite failed ret: %lu error: %s\n", nwrite, strerror(errno));
        }
        len += nwrite;
      } else if (len < buflen - 1) {

        *buffer++ = (char)(i + '!');
        ++len;
        if (root->end) {
          *buffer++ = ' ';
          ++len;
        }
      }

      size_t len_children =
        store_trie(root->children[i], fp, buffer, buflen - len);

      len += len_children;
      if (buffer) {
        buffer += len_children;
      }
    }
  }
  
  if (buffer) {
    *buffer = '\0';
  }

  return len;
}

static
size_t
load_trie(struct trie_node **trie, FILE *fp, char* buffer,
    size_t *size) {

  if (!trie || !*trie || (!fp && (!buffer && !size))) {
    return 0;
  }

  size_t len = 0;
  struct trie_node* node = *trie = create_node();

  if (fp) {
    char c = '\0';

    size_t nread = 0;;
    while ((nread = fread(&c, 1, sizeof(c), fp)) == 1 * sizeof(c)) {
      if (c != ' ') {
        break;
      }
      len += load_trie(&node->children[c - '!'], fp, buffer, size);
    }
    if (nread < 1 * sizeof(c)) {
      wrn("error: fwrite failed ret: %lu error: %s\n", nread, strerror(errno));
    } else {
      node->children[c - '!']->end = 1;
    }

    len += nread;

  } else if (buffer) {
    while(size && *buffer != '\0') {
      int index = *buffer++ - '!';
      ++len;

      *size -= len;
      len += load_trie(&node->children[index], fp, buffer, size);

      ++len;
      node->children[index]->end = (*buffer++ == ' ') ? 1 : 0;
    }
  }

  return len;
}

UNUSED
static
size_t serialize_trie_from_file(struct trie_node *root, FILE* file) {
  return store_trie(root, file, NULL, 0);
}

UNUSED
static
size_t deserialize_trie_from_file(struct trie_node *root, FILE* file) {
  return load_trie(&root, file, NULL, 0);
}

struct hash_map {
/* 
 * Define this macro to enable checking of duplicate entries in the hash map.
 *
 * CFG_HASHMAP_CHK_DUPLICATES 
 *
 */
#ifdef CFG_HASHMAP_CHK_DUPLICATES 
#define __HASHMAP_CHK_DUPLICATES 1
#endif

  struct symbol *table[HASH_SIZE];
};

struct args {
  bool no_warn_unused;
  bool no_warn_undef;
  bool no_warn_unused_func;
  bool no_warn_unused_var;
  bool no_warn_undef_func;
  bool no_warn_undef_var;
  char *gdbfile;
  char *arch;
  enum action_type action;
};

struct progdata {
  char archlist[MAX_ARCHS][ARCH_LEN];
  struct lines_map linemap;
  struct hash_map defs;
  struct hash_map refs;
  struct trie_node *cmds;
  int linenum_width;
};

/* Safe functions */

static
char* indexn(const char *str, size_t len, const int c) {
  if (!str || !len) {
    return NULL;
  }

  for (; len && str && *str; ++str, --len) {
    int pc = *str;
    if (pc == c) {
      break;
    }
  }

  return len && str && *str ? (char*)str : NULL;
}

static
char* strntok(char *str, size_t len, const char* delim, size_t dlen) {
  static char *start = NULL;
  static size_t rem = 0;

  if (str) {
    start = str;
    rem = len;
  }

  char *ptr = start;
  char *ret = rem && ptr && *ptr ? ptr : NULL;

  while (rem) {
    if (!ptr || !*ptr) {
      rem = 0;
      break;
    }

    size_t didx = 0;
    while (didx < dlen) {
      if (!delim[didx]) {
        break;
      }

      if (*ptr == delim[didx]) {
        *ptr = '\0';
        if (ret == ptr++) {
          ret = ptr;
        } else {
          start = ptr;
          break;
        }
      }

      ++didx;
    }

    if (didx < dlen) {
      break;
    }

    ++ptr;
    --rem;
  }

  return ret;
}

static
char* strncatn(char *dest, size_t dlen, const char *src, size_t slen) {
  if (!dest || !dlen) {
    return NULL;
  }
  if (!src || !slen) {
    return dest;
  }

  char *destcpy = dest;
  size_t didx = 0, sidx = 0;

  while (didx++ < dlen && destcpy && *destcpy) {
    ++destcpy;
  }

  if (!destcpy) {
    return NULL;
  }

  for (; didx++ < dlen && sidx++ < slen && destcpy && src && *src;) {
    *destcpy++ = *src++;
  }

  destcpy && (*destcpy = '\0');

  return dest;
}

UNUSED
static
ssize_t getline_e(char **lineptr, size_t* len, FILE *stream,
    int *local_errno) {

  if (!len) {
    errno = EINVAL;
    return -1;
  }

  size_t n = *len;

  if (local_errno) {
    *local_errno = 0;
    errno = 0;
  }

  size_t ret = getline(lineptr, &n, stream);

  if (local_errno) {
    *local_errno = errno;
  }

  if (n > *len) {
    *len = n;
  }
  
  return ret;
}

static
char* fgets_e(char* s, int size, FILE *stream, int *local_errno) {

  if (!size) {
    return 0;
  }

  if (local_errno) {
    *local_errno = 0;
    errno = 0;
  }

  char *ret = fgets(s, size, stream);

  if (local_errno) {
    *local_errno = errno;
  }

  return ret;
}

/* Functions */

#if 0
static
const char* md5sum(const char *file) {
  if (!file) {
    return NULL;
  }

  char cmd[MAX_LEN];
  static char buffer[MAX_LEN];

  if (!realpath(file, buffer)) {
    return NULL;
  }

  int n = snprintf(cmd, sizeof(cmd), "md5sum %s", buffer);
  if (n < 0) {
    return NULL;
  }

  cmd[n] = '\0';

  FILE *fp = popen(cmd, "r");
  if (fp == NULL) {
    err("failed to run command: %s error: %s\n", cmd, strerror(errno));
    return NULL;
  }

  if (!fgets(buffer, sizeof(buffer), fp)) {
    buffer[0] = '\0';
  }

  pclose(fp);

  return strntok(buffer, strlen(buffer), " ", strlen(" "));
}
#endif

static
const char*
get_print_header(const char *progname) {
  if (!progname) {
    return NULL;
  }

  static char progheader[MAX_LEN] = "";

  snprintf(progheader, sizeof(progheader), "%s - lint GDB scripts\n", progname);

  return progheader;
}

static
void
print_arch(struct progdata *pdata) {
  if (!pdata) {
    return;
  }

  printf("ARCHITECTURES\n\tAvailabe GDB architectures\n\n");

  for (size_t i = 0; i < MAX_ARCHS; ++i) {
    (void)(
        pdata->archlist[i][0] != '\0' &&
        printf("\t%s\n", pdata->archlist[i])
      );
  }
}

static
inline
const char*
progname(const char *name) {
  static const char *__progname = NULL;
  return name ? (__progname = name) : __progname;
}

/* Hash map */

static
void
insert_line(struct lines_map *map, const char *current_line,
    size_t orig_linenum) {
 
  if (!map || !current_line || !orig_linenum) {
    return;
  }

  if (!map->lines) {
    map->lines = (struct merged_line*)malloc(
        (map->capacity = MAX_LINES) * sizeof(struct merged_line)
     );

    if (!map->lines) {
      err("malloc failed: error: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }
  }

  if (map->count >= map->capacity) {
    map->lines =
      (struct merged_line*)realloc(map->lines,
          (map->capacity <<= 1) * sizeof(struct merged_line));

    if (!map->lines) {
      err("realloc failed: error: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }
  }

  strncpy(map->lines[map->count].line, current_line,
      sizeof(map->lines[map->count].line));
  map->lines[map->count++].orig_linenum = orig_linenum;

  if (orig_linenum > map->max_linenum) {
    map->max_linenum = orig_linenum;
  }
}

static
unsigned int
fnv1a(const char *key) {
  unsigned int hash = 2166136261u;

  while (*key) {
    hash ^= (unsigned char)*key++;
    hash *= 16777619u;
  }

  return hash % HASH_SIZE;
}

static
void
init_map(struct hash_map *map) {
  if (!map) {
    return;
  }
  memset(map, 0, sizeof(struct hash_map));
}

static
void
destroy_map(struct hash_map *map) {
  if (!map) {
    return;
  }

  for (size_t i = 0; i < HASH_SIZE; ++i) {
    struct symbol *entry = map->table[i];

    while (entry) {
      struct symbol *tmp = entry;
      entry = entry->next;
      free(tmp);
    }

    map->table[i] = NULL;
  }
}

static
void
destroy_tree(struct trie_node *root) {
  if (!root) {
    return;
  }

  for (size_t i = 0; i < sizeof(root->children)/sizeof(root->children[0]); ++i) {
    root->children[i] && (destroy_tree(root->children[i]), 1);
  }

  free(root);
}

static
void
insert_symbol(struct hash_map *map, const char *name, size_t linenum,
    enum symbol_type type) {

  if (!map || !name || !*name) {
    return;
  }

  unsigned int index = fnv1a(name);

#ifdef __HASHMAP_CHK_DUPLICATES
  for (struct symbol *p = map->table[index]; p; p = p->next) {
    /* TODO: add name_len as parameter */
    if (!strncmp(name, p->name, strlen(name)) &&
        type == p->type &&
        linenum == p->linenum) {
      return;
    }
  }
#endif

  struct symbol *entry = (struct symbol*)malloc(sizeof(struct symbol));
  if (!entry) {
    return;
  }

  strncpy(entry->name, name, sizeof(entry->name) - 1);
  entry->type = type;
  entry->linenum = linenum;
  entry->next = map->table[index];

  map->table[index] = entry;
}

static
struct symbol*
find_symbol(struct hash_map *map, const char *name, enum symbol_type type) {
  unsigned int index = fnv1a(name);

  for (struct symbol *entry = map->table[index]; entry; entry = entry->next) {
    if (strcmp(entry->name, name) || (type != NONE && type != entry->type)) {
      continue;
    }
    return entry;
  }

  return NULL;
}

static
size_t
store_map(struct hash_map *map, const char *mapname, size_t mapname_len,
    FILE *fp, char *buffer, size_t len) {

  if (!map || (!fp && !buffer)) {
    return 0;
  }

  size_t n = 0;

  if (mapname) {
    int ret = 0;
    if (fp) {
      ret = fprintf(fp, "%.*s\n", (int)mapname_len, mapname);

    } else if (buffer) {
      ret = snprintf(buffer, len, "%.*s\n", (int)mapname_len, mapname);
    }

    if (ret < 0) {
      if (buffer) {
        wrn("error: %s ret: %d buffer: %s\n", strerror(errno), ret,
            buffer + n);
      } else {
        wrn("error: %s ret: %d\n", strerror(errno), ret);
      }
    }

    n += ret;
  }

  for (size_t i = 0; i < HASH_SIZE; ++i) {
    struct symbol *entry = map->table[i];
    if (!entry) {
      continue;
    }

    while (entry) {
      if (!entry->linenum) {
#if DBG_ENABLED
        size_t prev_n = n;
#endif

        int ret = 0;

        if (fp) {
          ret = fprintf(fp, "%lu,%s,%d,%lu\n", i, entry->name,
              entry->type, entry->linenum);

        } else if (buffer) {
          ret = snprintf(buffer + n, len - n, "%lu,%s,%d,%lu\n", i, entry->name,
              entry->type, entry->linenum);
        }

        if (ret < 0) {
          if (buffer) {
            dbg("error: %s ret: %d buffer: %s\n", strerror(errno), ret,
                buffer + n);
          } else {
            dbg("error: %s ret: %d\n", strerror(errno), ret);
          }
        }

        n += ret;

#if DBG_ENABLED
        if (buffer) {
          dbg("%lu,%s,%d,%lu: n %lu buffer: %.*s\n", i, entry->name,
              entry->type, entry->linenum, n, (int)(n - prev_n),
              buffer + prev_n);
        } else {
          dbg("map: %s entry: %p %lu,%s,%d,%lu: n %lu\n", mapname,
              (void*)entry, i, entry->name, entry->type, entry->linenum, n);
        }
#endif
      }

      entry = entry->next;
    }
  }

  dbg("ret n: %lu\n", n);
  return n;
}

static
size_t
load_map(struct hash_map *map, const char *mapname, size_t mapname_len,
    FILE *fp, char *buffer, size_t len) {

  if (!map || (!fp && !buffer)) {
    return 0;
  }

  size_t n = 0;
  int nread = 0;
  int ret = 0;

  if (mapname) {
    char read_mapname[MAX_LEN];
    if (fp) {
      ret = fscanf(fp, "%s\n%n", read_mapname, &nread);
    } else if (buffer) {
      ret = sscanf(buffer, "%s\n%n", read_mapname, &nread);
    }

    if (ret == EOF || ret < 1) {
      if (buffer) {
        wrn("error: %s ret: %d buffer: %s\n", strerror(errno), ret,
            buffer + n);
      } else {
        wrn("error: %s ret: %d\n", strerror(errno), ret);
      }
      return 0;
    }
    if ((size_t)nread != mapname_len ||
        (strncmp(read_mapname, mapname, mapname_len))) {
      wrn("error: nread: %d read_mapname: %s mapname_len: %lu mapname: %s\n",
          nread, read_mapname, mapname_len, mapname);
      return 0;
    }
  }

  while (len || fp) {
    size_t index;
    struct symbol *entry = (struct symbol*)malloc(sizeof(struct symbol));
    if (!entry) {
      break;
    }

    do {
      if (fp) {
        ret = fscanf(fp, "%lu,%[^,],%d,%lu\n%n", &index, &entry->name[0],
            (int*)&entry->type, &entry->linenum, &nread);

      } else if (buffer) {
        ret = sscanf(buffer + n, "%lu,%[^,],%d,%lu\n%n", &index, &entry->name[0],
            (int*)&entry->type, &entry->linenum, &nread);
      }

      if (ret == 4 && (size_t)nread <= len) {
        break;
      }

      /* More or less data has been read than expected, discard it */

      if (buffer) {
        dbg("error: %s ret: %d buffer: %s\n", strerror(errno), ret,
            buffer + n);
      } else {
        dbg("error: %s ret: %d index: %lu\n", strerror(errno), ret,
            index);
      }

      free(entry);

      dbg("ret n: %lu\n", n);
      return n;

    } while (0);

    len -= nread;   // integer arithmetic works just fine!
    n += nread;

    dbg("%lu,%s,%d,%lu\n", index, entry->name, entry->type, entry->linenum);

    /* Insert into hash table */
    entry->next = map->table[index];
    map->table[index] = entry;
  }

  dbg("ret n: %lu\n", n);
  return n;
}

static
void destroy_progdata(struct progdata *pdata) {
  if (!pdata) {
    return;
  }

  destroy_map(&pdata->defs);
  destroy_map(&pdata->refs);
  destroy_tree(pdata->cmds);
}

static
const char*
get_system_arch(char *hint) {
  if (!hint) {
    static struct utsname utsbuf = { 0 };
    if (utsbuf.machine[0] == 0 && uname(&utsbuf) != 0) {
      return NULL;
    }
    hint = utsbuf.machine;
  }

  for (char *ptr = hint; ptr && *ptr; ++ptr) {
    if (*ptr == '_') {
      *ptr = '-';
    }
  }

  return hint;
}

static
char*
parse_gdb_output(char *ptr_arg, const char *delim) {
  if (!ptr_arg) {
    return NULL;
  }

  static int state = 0;

  static char *ptr = NULL;

  size_t len = strlen(ptr_arg);
  switch (state) { case 0:

  ptr = strntok(ptr_arg, len, delim, strlen(delim));
  do {
    while (*ptr == ' ' || *ptr == '\t' || *ptr == '\n') {
      ++ptr;
    }
    if (!ptr || !*ptr) {
      continue;
    }
    state = __LINE__; return ptr; case __LINE__: ;
  }
  while ((ptr = strntok(NULL, len, delim, strlen(delim))) != NULL && !(*ptr == '-' &&
         *(ptr + 1) == '-'));

  }

  state = 0; return NULL;
}

/*
 * This is speculative because I have not found any other way to make gdb print
 * available target architectures.
 */
static
bool
load_gdb_arch(size_t n, char archlist[n][ARCH_LEN]) {
  FILE *fp = popen("gdb -batch -ex 'set architecture' 2>&1", "r");
  if (!fp) {
    err("popen failed: %s\n", strerror(errno));
    return false;
  }

  size_t buflen = sizeof(char) * MAX_LEN;
  char *buffer = (char*)malloc(buflen);
  if (!buffer) {
    return false;
  }

  size_t i = 0;
  ssize_t nread = 0;
  int localerrno = 0;

#if 0 && (_POSIX_C_SOURCE >= 200809L || defined(_GNU_SOURCE))
  while ((nread = getline_e(&buffer, &buflen, fp, &localerrno)) > 0) {
#else
  while (fgets_e(buffer, buflen, fp, &localerrno) && i < n) {
#endif
    char *ptr = strstr(buffer, "Valid arguments are ");
    if (!ptr) {
      continue;
    }

    ptr += strlen("Valid arguments are ");

    while ((ptr = parse_gdb_output(ptr, "., ")) && i < n) {
      strncpy(archlist[i++], ptr, sizeof(archlist[0]));
    }
  }
  if (nread < 0 && localerrno != 0) {
    err("getline failed: error: %s\n", strerror(errno));
  }

  buffer && (free(buffer), 1);
  pclose(fp);

  return i == 0 ? false : true;
}

static
bool
load_gdb_registers(struct progdata *pdata, const char *arch) {
  if (!pdata) {
    return false;
  }

  if (!arch) {
    arch = "auto";
  }

  char cmd[256];
  snprintf(
    cmd,
    sizeof(cmd),
    "gdb -batch -ex 'set architecture %s' -ex 'maintenance print registers' "
    "-ex 'maintenance print user-registers' 2>/dev/null",
    arch
  );

  FILE *fp = popen(cmd, "r");
  if (!fp) {
    err("popen failed: %s\n", strerror(errno));
    return false;
  }

  size_t buflen = sizeof(char) * MAX_LEN;
  char *buffer = (char*)malloc(buflen);
  if (!buffer) {
    return false;
  }
  ssize_t nread = 0;
  int localerrno = 0;

#if 0 && (_POSIX_C_SOURCE >= 200809L || defined(_GNU_SOURCE))
  while ((nread = getline_e(&buffer, &buflen, fp, &localerrno)) > 0) {
#else
  while (fgets_e(buffer, buflen, fp, &localerrno)) {
#endif
    char *ptr = buffer;
    while (*ptr == ' ') {
      ++ptr;
    }

    if (isupper(*ptr)) {
      continue;
    }

    ptr = strntok(ptr, strlen(ptr), ", \t\n", strlen(", \t\n"));
    if (ptr && ptr[0] != '\'' && ptr[1] != '\'') {
      dbg("register: %s\n", ptr);
      insert_symbol(&pdata->defs, ptr, 0, VAR);
    }
  }
  if (nread < 0 && localerrno != 0) {
    err("getline failed: error: %s\n", strerror(errno));
  }

  bool ret = !feof(fp) ? false : true;

  buffer && (free(buffer), 1);
  pclose(fp);

  return ret;
}

static
bool
load_gdb_commands(struct progdata *pdata) {
  if (!pdata) {
    return false;
  }

  FILE *fp = popen("gdb -batch -ex 'help all' 2>/dev/null", "r");
  if (!fp) {
    err("popen failed: %s\n", strerror(errno));
    return false;
  }

  size_t buflen = sizeof(char) * MAX_LEN;
  char *buffer = (char*)malloc(buflen);
  if (!buffer) {
    return false;
  }
  ssize_t nread = 0;
  int localerrno = 0;

#if 0 && (_POSIX_C_SOURCE >= 200809L || defined(_GNU_SOURCE))
  while ((nread = getline_e(&buffer, &buflen, fp, &localerrno)) > 0) {
#else
  while (fgets_e(buffer, buflen, fp, &localerrno)) {
#endif
    if (isalpha(buffer[0]) &&
        islower(buffer[0])) {

      char *ptr = buffer;

      dbg("gdb output: %s", buffer);
      ptr = indexn(ptr, ',', nread);
      dbg("ptr for ',': %s\n", ptr);
      if (!ptr) {
        ptr = strntok(buffer, strlen(buffer), " ", strlen(" "));
        dbg("parsed command: %s\n", ptr);
        //if (!find_symbol(&pdata->cmds, ptr, FUNC)) {
          //insert_symbol(&pdata->cmds, ptr, 0, FUNC);
        char cmd[MAX_COMMAND_LENGTH] = "";
        size_t cmdlen = 0;
        struct trie_node *found = find_command(pdata->cmds, ptr, cmd, &cmdlen);
        if (!found || (found && cmdlen != strlen(ptr))) {
          insert_command(&pdata->cmds, ptr);
        }

      } else {
        while ((ptr = parse_gdb_output(buffer, ","))) {
          char *space = index(ptr, ' ');
          if (space) {
            *space = '\0';
          }
          dbg("parsed command: %s\n", ptr);
          //if (!find_symbol(&pdata->cmds, ptr, FUNC)) {
            //insert_symbol(&pdata->cmds, ptr, 0, FUNC);
          char cmd[MAX_COMMAND_LENGTH] = "";
          size_t cmdlen = 0;
          struct trie_node *found = find_command(pdata->cmds, ptr, cmd, &cmdlen);
          if (!found || (found && cmdlen != strlen(ptr))) {
            insert_command(&pdata->cmds, ptr);
          }
        }

      }
    }
  }
  if (nread < 0 && localerrno != 0) {
    err("getline failed: error: %s\n", strerror(errno));
  }

  bool ret = !feof(fp) ? false : true;

  buffer && (free(buffer), 1);
  pclose(fp);

  //insert_symbol(&pdata->defs, "silent", 0, FUNC);
  insert_command(&pdata->cmds, "silent");

  return ret;
}

static
bool
load_gdb_convenience_vars(struct progdata *pdata) {
  if (!pdata) {
    return false;
  }

  FILE *fp = popen("gdb -batch -ex 'show convenience' 2>/dev/null", "r");
  if (!fp) {
    err("popen failed: %s\n", strerror(errno));
    return false;
  }

  size_t buflen = sizeof(char) * MAX_LEN;
  char *buffer = (char*)malloc(buflen);
  if (!buffer) {
    return false;
  }
  ssize_t nread = 0;
  int localerrno = 0;

#if 0 && (_POSIX_C_SOURCE >= 200809L || defined(_GNU_SOURCE))
  while ((nread = getline_e(&buffer, &buflen, fp, &localerrno)) > 0) {
#else
  while (fgets_e(buffer, buflen, fp, &localerrno)) {
#endif
    if (buffer[0] == '$') {
      if (strstr(buffer, "internal function")) {
        continue;
      }

      const char *ptr =
        strntok(buffer + 1, strlen(buffer + 1), ", \t\n", strlen(", \t\n"));
      if (ptr) {
        insert_symbol(&pdata->defs, ptr, 0, VAR);
      }
    }
  }
  if (nread < 0 && localerrno != 0) {
    err("getline failed: error: %s\n", strerror(errno));
  }

  bool ret = !feof(fp) ? false : true;

  buffer && (free(buffer), 1);
  pclose(fp);

  /* Other convenience variables */
  insert_symbol(&pdata->defs, "_", 0, VAR);
  insert_symbol(&pdata->defs, "__", 0, VAR);

  insert_symbol(&pdata->defs, "_exitcode", 0, VAR);
  insert_symbol(&pdata->defs, "_exitsignal", 0, VAR);
  insert_symbol(&pdata->defs, "_exception", 0, VAR);
  insert_symbol(&pdata->defs, "_ada_exception", 0, VAR);
  insert_symbol(&pdata->defs, "_probe_argc", 0, VAR);
  insert_symbol(&pdata->defs, "_probe_arg0…$_probe_arg11", 0, VAR);
  insert_symbol(&pdata->defs, "_sdata", 0, VAR);
  insert_symbol(&pdata->defs, "_siginfo", 0, VAR);
  insert_symbol(&pdata->defs, "_thread", 0, VAR);
  insert_symbol(&pdata->defs, "_gthread", 0, VAR);
  insert_symbol(&pdata->defs, "_inferior_thread_count", 0, VAR);
  insert_symbol(&pdata->defs, "_gdb_major", 0, VAR);
  insert_symbol(&pdata->defs, "_gdb_minor", 0, VAR);
  insert_symbol(&pdata->defs, "_shell_exitcode", 0, VAR);
  insert_symbol(&pdata->defs, "_shell_exitsignal", 0, VAR);

  insert_symbol(&pdata->defs, "bpnum", 0, VAR);
  insert_symbol(&pdata->defs, "cdir", 0, VAR);

  return ret;
}

static
bool
is_history_var(const char *word) {
  if (!word) {
    return false;
  }

  if (*word == '$') {
    ++word;
  }

  if (!word || !*word) {
    return true;

  } else if (*word == '$') {
    if (!++word || !*word) {
      return true;
    }
  }

  do {
    if (!isdigit(*word++)) {
      return false;
    }
  } while (word && *word);

  return true;
}

static
bool
is_func_arg(const char *word) {
  if (!word) {
    return false;
  }

  if (*word == '$') {
    ++word;
  }

  size_t len = strlen("arg");

  if (strncmp(word, "arg", len) != 0) {
    return false;
  }

  if ((word += len) == NULL) {
    return false;
  }

  do {
    if (!isdigit(*word++)) {
      return false;
    }
  } while (word && *word);

  return true;
}

static
bool
is_gdb_command(struct progdata *pdata, const char *word) {
  //return pdata ? find_symbol(&pdata->cmds, word, FUNC) : false;
  char cmd[MAX_COMMAND_LENGTH] = "";
  size_t cmdlen = 0;
  struct trie_node *found = find_command(pdata->cmds, word, cmd, &cmdlen);
  return pdata ? found || cmdlen > 0 : false;
}

static
bool
is_gdb_keyword(const char* token) {
  static const char* gdbkeywords[] = {
    "if", "else", "while", "for", "break", "continue", "end", "quit"
  };

  for (size_t i = 0; i < sizeof(gdbkeywords) / sizeof(gdbkeywords[0]); i++) {
    if (strcmp(gdbkeywords[i], token) == 0) {
      return true;
    }
  }

  return false;
}

static
bool
is_number(const char *token) {
  if (!token || !*token) {
    return false;
  }

  if (*token == '-' || *token == '+') {
    ++token;
  }

  while (token && *token) {
    if (!isdigit(*token++)) {
      return false;
    }
  }

  return true;
}

static
bool
is_floating_point(const char *token) {
  if (!token || !*token) {
    return false;
  }

  if (*token == '-' || *token == '+') {
    ++token;
  }

  bool dec = false;

  for (; token && *token; ++token) {
    if (!isdigit(*token) && (dec || !(dec = (*token == '.')))) {
      return false;
    }
  }

  return dec ? true : false;
}

static
bool
is_valid_reference(struct progdata *pdata, const char *token) {
  return !is_history_var(token) &&
         !is_func_arg(token) &&
         !is_gdb_command(pdata, token) &&
         !is_gdb_keyword(token) &&
         !is_number(token) &&
         !is_floating_point(token);
}

static
void
calc_linenum_width(struct progdata *pdata) {
  if (!pdata) {
    return;
  }

  size_t max_linenum = pdata->linemap.max_linenum;

  pdata->linenum_width = 1;
  while (max_linenum) {
    max_linenum /= 10;
    ++pdata->linenum_width;
  }
}

static
void
parse_gdbfile(struct progdata *pdata, FILE *fp) {
  if (!pdata) {
    return;
  }

  size_t buflen = sizeof(char) * MAX_LEN;
  char *buffer = (char*)malloc(buflen);
  if (!buffer) {
    return;
  }
  char current_line[MAX_LEN] = "";
  size_t orig_linenum = 1;
  ssize_t nread = 0;
  int localerrno = 0;

#if 0 && (_POSIX_C_SOURCE >= 200809L || defined(_GNU_SOURCE))
  while ((nread = getline_e(&buffer, &buflen, fp, &localerrno)) > 0) {
    size_t len = nread;
#else
  while (fgets_e(buffer, buflen, fp, &localerrno)) {
    size_t len = strlen(buffer);
#endif

    if (len > 0 && buffer[len - 1] == '\n') {
      buffer[len - 1] = '\0';
    }

    if (len > 1 && buffer[len - 2] == '\\') {
      buffer[len - 2] = '\0';
      strncatn(current_line, sizeof(current_line), buffer, len);

    } else {
      strncatn(current_line, sizeof(current_line), buffer, len);
      insert_line(&pdata->linemap, current_line, orig_linenum);
      current_line[0] = '\0';
    }

    orig_linenum++;
  }
  if (nread < 0 && localerrno != 0) {
    err("getline failed: error: %s\n", strerror(errno));
  }

  buffer && (free(buffer), 1);

  calc_linenum_width(pdata);
}

static
void
extract_defs(struct progdata *pdata) {
  if (!pdata) {
    return;
  }

  regex_t def_regex, set_regex, py_setvar;

  regcomp(&def_regex, "^\\s*define\\s+([a-zA-Z0-9_-]+)", REG_EXTENDED);
  regcomp(&set_regex, "^\\s*set\\s+\\$([a-zA-Z0-9_-]+)", REG_EXTENDED);
  regcomp(
    &py_setvar,
    "^\\s*python.*set_convenience_variable\\(\"?([a-zA-Z0-9_-]+)\"?,",
    REG_EXTENDED
  );

  for (size_t i = 0; i < pdata->linemap.count; i++) {
    char *ptr = index(pdata->linemap.lines[i].line, '#');
    if (ptr) {
      *ptr = '\0';
    }

    regmatch_t matches[2];
    enum symbol_type type = NONE;

    if (regexec(&def_regex, pdata->linemap.lines[i].line, 2, matches, 0) == 0) {
      type = FUNC;

    } else if (
      regexec(&set_regex, pdata->linemap.lines[i].line, 2, matches, 0) == 0 ||
      regexec(&py_setvar, pdata->linemap.lines[i].line, 2, matches, 0) == 0
    ) {
      type = VAR;
    }

    if (type != NONE) {
      size_t length = matches[1].rm_eo - matches[1].rm_so;
      char name[MAX_LEN - 1];

      dbg("definition : [%.*s]\n", (int)length,
          pdata->linemap.lines[i].line + matches[1].rm_so);

      strncpy(name, pdata->linemap.lines[i].line + matches[1].rm_so, length);
      name[length] = '\0';

      insert_symbol(&pdata->defs, name,
          pdata->linemap.lines[i].orig_linenum, type);;
    }
  }

  regfree(&def_regex);
  regfree(&set_regex);
  regfree(&py_setvar);
}

static
void
extract_refs(struct progdata *pdata) {
  if (!pdata) {
    return;
  }

  regex_t var_regex, func_regex;

  regcomp(
    &var_regex,
    "(\\s*|\\b)\\$([a-zA-Z0-9_-]+)(\\s*|\\b|$)",
    REG_EXTENDED
  );
  regcomp(
    &func_regex,
    "(^\\s*|;\\s*)([a-zA-Z0-9_-]+)(\\s+[$a-zA-Z0-9_-]+)*\\s*(;|$)",
    REG_EXTENDED
  );

  for (size_t i = 0; i < pdata->linemap.count; i++) {
    char *ptr = index(pdata->linemap.lines[i].line, '#');
    if (ptr) {
      *ptr = '\0';
    }

    //if (strstr(pdata->linemap.lines[i].line, "set ") ||
    if (strstr(pdata->linemap.lines[i].line, "define ")) {
      continue;
    }

    ptr = strstr(pdata->linemap.lines[i].line, "set ");
    if (ptr) {
      ptr = strstr(ptr, "=");
      if (!ptr) {
        continue;
      }
    } else {
      ptr = pdata->linemap.lines[i].line;
    }

    regmatch_t matches[4];
    const char *cursor = ptr;

    dbg("cursor: %s\n", cursor);

    while (*cursor && regexec(&func_regex, cursor, 3, matches, 0) == 0) {
      size_t length = matches[2].rm_eo - matches[2].rm_so;

      dbg("func reference: [%.*s]\n", (int)length, cursor + matches[2].rm_so);

      char name[MAX_LEN - 1];
      strncpy(name, cursor + matches[2].rm_so, length);
      name[length] = '\0';

      if (is_valid_reference(pdata, name)) {
        insert_symbol(&pdata->refs, name,
            pdata->linemap.lines[i].orig_linenum, FUNC);
      }

      cursor += matches[0].rm_eo;
    }

    cursor = ptr;

    dbg("cursor: %s\n", cursor);

    while (*cursor && regexec(&var_regex, cursor, 3, matches, 0) == 0) {

      size_t length = matches[2].rm_eo - matches[2].rm_so;
      char name[MAX_LEN - 1];

      dbg("var reference: [%.*s]\n", (int)length, cursor + matches[2].rm_so);

      strncpy(name, cursor + matches[2].rm_so, length);
      name[length] = '\0';

      if (is_valid_reference(pdata, name)) {
        insert_symbol(&pdata->refs, name,
            pdata->linemap.lines[i].orig_linenum, VAR);
      }

      cursor += matches[0].rm_eo;
    }
  }

  regfree(&var_regex);
  regfree(&func_regex);
}

static
int
report_unused(struct progdata *pdata, struct args *pargs) {
  if (!pdata || !pargs || pargs->no_warn_unused) {
    return 0;
  }

  int count = 0;

  for (size_t i = 0; i < HASH_SIZE; i++) {
    for (struct symbol *def = pdata->defs.table[i]; def != NULL &&
         def->linenum != 0; def = def->next) {
      if (pargs->no_warn_unused_func && def->type == FUNC) {
        continue;
      }

      if (pargs->no_warn_unused_var && def->type == VAR) {
        continue;
      }

      if (!find_symbol(&pdata->refs, def->name, def->type)) {
        if (pargs->action == SCRIPTABLE) {
          printf("  \"");
        }

        printf(
          "%s:%.*ld: "
          "Unused %s: '%s' defined at line %ld is never used",
          pargs->gdbfile ? basename(pargs->gdbfile) : "STDIN",
          pdata->linenum_width, def->linenum,
          def->type == FUNC ? "func" : def->type == VAR ? "var" : NULL,
          def->name, def->linenum
        );

        if (pargs->action == SCRIPTABLE) {
          printf("\\n\"\\\n");
        } else {
          putchar('\n');
        }

        ++count;
      }
    }
  }

  return count;
}

static
int
report_undefined(struct progdata *pdata, struct args *pargs) {
  if (!pdata || !pargs || pargs->no_warn_undef) {
    return 0;
  }

  int count = 0;

  for (size_t i = 0; i < HASH_SIZE; i++) {
    for (struct symbol *ref = pdata->refs.table[i]; ref != NULL;
         ref = ref->next) {
      if (pargs->no_warn_undef_func && ref->type == FUNC) {
        continue;
      }

      if (pargs->no_warn_undef_var && ref->type == VAR) {
        continue;
      }

      if (!find_symbol(&pdata->defs, ref->name, ref->type)) {
        if (pargs->action == SCRIPTABLE) {
          printf("  \"");
        }

        printf(
          "%s:%.*ld: "
          "Undefined %s: '%s' is referenced at line %ld but never defined",
          pargs->gdbfile ? basename(pargs->gdbfile) : "STDIN",
          pdata->linenum_width, ref->linenum,
          ref->type == FUNC ? "func" : ref->type == VAR ? "var" : NULL,
          ref->name, ref->linenum
        );

        if (pargs->action == SCRIPTABLE) {
          printf("\\n\"\\\n");
        } else {
          putchar('\n');
        }

        ++count;
      }
    }
  }

  return count;
}

static
int
report_issues(struct progdata *pdata, struct args *pargs) {
  if(!pargs) {
    return 0;
  }

  if (pargs->action == SCRIPTABLE) {
    printf("export GDBLINT_REPORTS=(\\\n");
  }

  int ret = report_undefined(pdata, pargs) + report_unused(pdata, pargs);

  if (pargs->action == SCRIPTABLE) {
    printf(");\n");
  }

  return ret;
}

static
const char*
set_arch(struct progdata *pdata, char *arch) {
  if (!pdata) {
    return arch;
  }

  arch = (char*)get_system_arch(arch);

  for (size_t i = 0; arch && i < MAX_ARCHS; ++i) {
    if (strstr(pdata->archlist[i], arch)) {
      arch = pdata->archlist[i];
      break;
    }
  }

  return arch;
}

static
FILE*
get_gdbfp(const char *gdbfile) {
  FILE *gdbfp = gdbfile ? fopen(gdbfile, "r") : stdin;
  if (!gdbfp) {
    err("fopen failed: %s\n", strerror(errno));
    return NULL;
  }
  return gdbfp;
}

static
FILE*
setup_cache(bool clear) {
  static FILE *cachefp = NULL;
  static char cachefile[PATH_MAX] = { 0 };

  if (*cachefile == 0) {
    int length = snprintf(cachefile, sizeof(cachefile), "/home/%s/.cache/",
        getenv("USER"));

    if (access(cachefile, R_OK | W_OK | X_OK)) {
      if (errno != ENOENT) {
        err("access failed for path: %s error: %s\n", cachefile, strerror(errno));
        exit(EXIT_FAILURE);
      }
      if (mkdir(cachefile, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
        err("mkdir failed for path: %s error: %s\n", cachefile, strerror(errno));
        exit(EXIT_FAILURE);
      }
    }

    snprintf(cachefile, sizeof(cachefile) - length, "%s", progname(NULL));
  }

  if (clear) {
    if (remove(cachefile) && errno != ENOENT) {
      err("remove failed for path: %s error: %s\n", cachefile, strerror(errno));
      exit(EXIT_FAILURE);
    }

    printf("Definitions and commands cache has been removed\n");

    return cachefp = NULL;
  }

  if (cachefp) {
    return cachefp;
  }

  cachefp = fopen(cachefile, "r+");
  if (!cachefp && errno == ENOENT) {
    cachefp = fopen(cachefile, "w+");
  }
  if (!cachefp) {
    err("fopen failed: %s\n", strerror(errno));
    return NULL;
  }

  return cachefp;
}

static
bool
load_gdb_data(struct progdata *pdata, char *arch, bool list) {
  if (!pdata) {
    return false;
  }

  bool loaded = false;

  if (!(loaded = load_gdb_arch(MAX_ARCHS, pdata->archlist))) {
    return loaded;
  }

  if (list) {
    printf("%s\n", get_print_header(progname(NULL)));
    print_arch(pdata);
    fputc('\n', stdout);

    exit(EXIT_SUCCESS);
  }

  arch = (char*)set_arch(pdata, arch);
  dbg("arch: %s\n", arch);

  if (!(loaded = load_gdb_commands(pdata))) {
    return loaded;
  }
  if (!(loaded = load_gdb_convenience_vars(pdata))) {
    return loaded;
  }
  return (loaded = load_gdb_registers(pdata, arch));
}

static
size_t
store_maps(struct progdata *pdata, FILE *fp, size_t *pndefs, size_t *pnrefs,
    size_t *pncmds) {

  if (!pdata || !fp) {
    return 0;
  }

  size_t ndefs = 0, nrefs = 0, ncmds = 0;

  size_t nstore =
    (ndefs = store_map(&pdata->defs, "defs", sizeof("defs"), fp, NULL, 0));
  dbg("nstore: %lu\n", nstore);
  (void)(pndefs && (*pndefs = ndefs));

  nstore +=
    (nrefs = store_map(&pdata->refs, "refs", sizeof("refs"), fp, NULL, 0));
  dbg("nstore: %lu\n", nstore);
  (void)(pnrefs && (*pnrefs = nrefs));

  //nstore +=
  //  (ncmds = store_map(&pdata->cmds, "cmds", sizeof("cmds"), fp, NULL, 0));
  //dbg("nstore: %lu\n", nstore);
  //(void)(pncmds && (*pncmds = ncmds));
  nstore +=
    (ncmds = store_trie(pdata->cmds, fp, NULL, 0));
  dbg("nstore: %lu\n", nstore);
  (void)(pncmds && (*pncmds = ncmds));

  return nstore;
}

static
size_t
load_maps(struct progdata *pdata, FILE *fp, size_t ndefs, size_t nrefs,
    size_t ncmds UNUSED) {

  if (!pdata || !fp) {
    return 0;
  }

  size_t nload =
    load_map(&pdata->defs, "defs", sizeof("defs"), fp, NULL, ndefs);
  dbg("nload: %lu\n", nload);

  nload += load_map(&pdata->refs, "refs", sizeof("refs"), fp, NULL, nrefs);
  dbg("nload: %lu\n", nload);

  //nload += load_map(&pdata->cmds, "cmds", sizeof("cmds"), fp, NULL, ncmds);
  //dbg("nload: %lu\n", nload);

  return nload;
}

static
void
print_help(FILE *file, struct progdata *pdata, const char *progname) {
  if (!file || !pdata || !progname) {
    return;
  }

  fprintf(
    file,
    "%s"
    "\nUSAGE\n"
    "\t%s [OPTIONS] [FILE]\n"
    "\nDESCRIPTION\n"
    "\tLints a GDB script. Reads file contents from the standard input if \n"
    "\tfile path is not provided as final argument.\n"
    "\nOPTIONS\n"
    "\t-s, --script\n"
    "\t\tEnable bash friendly output\n"
    "\t-c, --clear\n"
    "\t\tClear the defs and commands cache\n"
    "\t-l, --list\n"
    "\t\tList architectures available with GDB\n"
    "\t-a, --arch\n"
    "\t\tSpecify the architecture to use\n"
    "\t--wno-unused\n"
    "\t\tDisable warnings for unused functions and variables\n"
    "\t--wno-unused-function\n"
    "\t\tDisable warnings for unused functions\n"
    "\t--wno-unused-variable\n"
    "\t\tDisable warnings for unused variables\n"
    "\t--wno-undefined\n"
    "\t\tDisable warnings for undefined functions and variables\n"
    "\t--wno-undefined-function\n"
    "\t\tDisable warnings for undefined functions\n"
    "\t--wno-undefined-variable\n"
    "\t\tDisable warnings for undefined variables\n",
    get_print_header(progname), progname
  );

  print_arch(pdata);

  fputc('\n', stdout);
}

static
int
parse_args(int argc, char *argv[], struct progdata *pdata, struct args *pargs) {
  if (!pdata || !pargs) {
    return EXIT_FAILURE;
  }

  /* non-static data as parse_args shall be called only once */
  const struct option long_options[] = {
    {"help", no_argument, NULL, 'h'},
    {"script", no_argument, NULL, 's'},
    {"clear", no_argument, NULL, 'c'},
    {"list", no_argument, NULL, 'l'},
    {"arch", required_argument, NULL, 'a'},
    {"wno-unused", no_argument, NULL, 1 << 1},
    {"wno-undefined", no_argument, NULL, 1 << 2},
    {"wno-unused-function", no_argument, NULL, 1 << 3},
    {"wno-unused-variable", no_argument, NULL, 1 << 4},
    {"wno-undefined-function", no_argument, NULL, 1 << 5},
    {"wno-undefined-variable", no_argument, NULL, 1 << 6},
    {0, 0, 0, 0}
  };

  int opt;

  pargs->action = LINT;

  while ((opt = getopt_long(argc, argv, "hscla:", long_options, NULL)) != -1) {
    switch (opt) {
      case 'a': {
        pargs->arch = optarg;
        break;
      }

      case 'c': {
        pargs->action = CLEAR_CACHE;
        break;
      }

      case 'l': {
        pargs->action = LIST_ARCHS;
        break;
      }

      case 's': {
        pargs->action = SCRIPTABLE;
        break;
      }

      case 1 << 1: {
        pargs->no_warn_unused = true;
        break;
      }

      case 1 << 2: {
        pargs->no_warn_undef = true;
        break;
      }

      case 1 << 3: {
        pargs->no_warn_unused_func = true;
        break;
      }

      case 1 << 4: {
        pargs->no_warn_unused_var = true;
        break;
      }

      case 1 << 5: {
        pargs->no_warn_undef_func = true;
        break;
      }

      case 1 << 6: {
        pargs->no_warn_undef_var = true;
        break;
      }

      default: {
        fputc('\n', stderr);
      }

      case 'h': {
        load_gdb_arch(MAX_ARCHS, pdata->archlist);
        print_help(stderr, pdata, progname(NULL));
      }

      return EXIT_FAILURE;
    }
  }

  if (optind < argc) {
    if ((pargs->gdbfile = realpath(argv[optind], NULL)) == 0) {
      return EXIT_FAILURE;
    }
  } else {
    pargs->gdbfile = NULL;
  }

  return EXIT_SUCCESS;
}

#ifndef CFG_UNIT_TESTS

/* Main */

int
main(int argc, char *argv[]) {
  struct args args = { 0 };
  struct progdata CLEANUP(destroy_progdata) data = { 0 };

  progname(basename(argv[0]));

  if (parse_args(argc, argv, &data, &args) == EXIT_FAILURE) {
    return EXIT_FAILURE;
  }

  FILE *gdbfp = get_gdbfp(args.gdbfile);
  if (!gdbfp) {
    return EXIT_FAILURE;
  }

  init_map(&data.defs);
  init_map(&data.refs);
  //init_map(&data.cmds);

  //if (system("command -v gdb > /dev/null 2>&1")) {
  //  err("gdb not found\n");
  //  return EXIT_FAILURE;
  //}

  FILE *cachefp = setup_cache((args.action == CLEAR_CACHE));
  if (!cachefp) {
    return EXIT_FAILURE;
  }

  bool loaded = load_gdb_data(&data, args.arch, (args.action == LIST_ARCHS));
  dbg("loaded: %d\n", loaded);
  (void)( !loaded && (loaded = (load_maps(&data, cachefp, 0, 0, 0) > 0)) );

  /***********************************/
  if (loaded && cachefp) {
    //char buffer[12288]; // 12K
    //char buffer[16384]; // 16K
    //size_t buflen = 12288; // 12K
    //size_t buflen = 16384; // 16K
    //char *buffer = (char*)malloc(buflen * sizeof(char));
    //if (!buffer) {
    //  break;
    //}
    //memset(buffer, 0, 16384);

    //size_t nstore = 0;
    //size_t ndefs = 0, nrefs = 0, ncmds = 0;

    fseek(cachefp, 0L, SEEK_SET);
    store_maps(&data, cachefp, NULL, NULL, NULL);

    //destroy_map(&data.defs);
    //destroy_map(&data.refs);
    //destroy_map(&data.cmds);

    //fseek(cachefp, 0L, SEEK_SET);

    //loaded = (load_maps(&data, cachefp) > 0);

    //buffer && (free(buffer), 1);
  }
  /***********************************/

  if (cachefp) {
    fclose(cachefp);
    cachefp = NULL;
  }

  parse_gdbfile(&data, gdbfp);

  extract_defs(&data);
  extract_refs(&data);

  int issues = report_issues(&data, &args);

  char numbuf[16] = { '0', '\0' };
  size_t w = 0;

  for (int copy = issues; copy; ++w, copy /= 10);
  if (w < sizeof(numbuf) - 1) {
    numbuf[w] = '\0';

    int copy = issues;
    for (ssize_t i = w - 1; i >= 0; --i) {
      numbuf[i] = (char)(copy % 10 + '0');
      copy /= 10;
    }
  }

  if (args.action == SCRIPTABLE) {
    printf("export GDBLINT_NREPORTS=%s;\n", numbuf);
  } else if (issues) {
    printf("File: %s\nFound: %s issue(s)\n", args.gdbfile, numbuf);
  }

  if (args.gdbfile) {
    if (gdbfp != stdin) {
      fclose(gdbfp);
    }

    free(args.gdbfile);
    args.gdbfile = NULL;
  }

  free(data.linemap.lines);

  destroy_map(&data.defs);
  destroy_map(&data.refs);
  //destroy_map(&data.cmds);

  return !issues ? EXIT_SUCCESS : EXIT_FAILURE;
}

#endif
