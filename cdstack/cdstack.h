#ifndef CDSTACK_H
#define CDSTACK_H
/*
 * cdstack header
 */

#ifdef DEBUG
#  define CDSTACK_DEBUG 1
#else
#  define CDSTACK_DEBUG 0
#endif

/* gnu23 */
#define D(fmt, f, ...) \
    (void) ((f) == 1 || CDSTACK_DEBUG == 1 \
    ? fprintf(stderr, "[%s:%d]: " fmt "\n", __func__, __LINE__ __VA_OPT__(,) __VA_ARGS__) : 0)

#define CDSTACK_VERSION "0.3"
#define CDSTACK_ARRAY "CDSTACK"
#define CDSTACK_COLUMN 16
#define CDSTACK_SMAGIC 0xCDCD0001u
#define CDSTACK_EMAGIC 0xCDCDDEADu

/* ─ Store layout ─ */
#define MAX_NAME 200
#define MAX_PATH 1024

/* WARNING: changing MAX_ENTRIES or Stack invalidates any existing
 * store file. Delete ~/.cdstack.bin before reloading the builtin. */
#define MAX_ENTRIES 200

/* option flags cpis:u: */
#define CFLAG 0x01
#define PFLAG 0x02
#define IFLAG 0x04
#define SFLAG 0x08
#define UFLAG 0x10

#define UNUSED(x) x __attribute__((unused))
#define NSEC(st) (((int64_t) (st)->st_mtim.tv_sec) * 1000000000L + ((st)->st_mtim.tv_nsec))

typedef struct {
    char name[MAX_NAME];
    char path[MAX_PATH];
} Entry;

typedef struct {
    unsigned int count;
    Entry entries[MAX_ENTRIES];
} Stack;

typedef enum {
   R_SUCCESS = 0,
   R_FAILURE = 1,
} Ret;

extern char *save_lastarg(void);
static char *ptrim(char *) __attribute__((nonnull));
static Ret invalid_name(const char *);
static Ret final_path(char *, char *, size_t);
static Ret cdstack_store_path(void);
static Ret cdstack_store_set(const char *, const char *);
static Ret cdstack_store_write();
static Ret cdstack_store_init();
static Ret cdstack_store_read();
static Ret cdstack_cmd_init(void);
static Ret cdstack_cmd_set(char *, char *);
static Ret cdstack_cmd_unset(const char *);
static Ret cdstack_cmd_list(void);
static Ret cdstack_cmd_declare(void);
static SHELL_VAR *prompt_dynamic_value(SHELL_VAR *);
static SHELL_VAR *cdstack_dynamic_value(SHELL_VAR *);
static SHELL_VAR *cdstack_assign_func(SHELL_VAR *, char *, arrayind_t, char *);
static Ret cdstack_load(char *);
static void cdstack_unload (char *);

#endif /* CDSTACK_H */
