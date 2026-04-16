/*
 * cdstack.c - Bash loadable builtin
 *
 * Dynamic assoc array, with persistent binary file store.
 *
 */

#include <errno.h>
#include <libgen.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <time.h>

/* Bash headers */
#include "loadables.h"
#include "cdstack.h"

static int g_ready;
static int64_t g_mtime;
static char array_name[] = CDSTACK_ARRAY;
static char prompt_command[] = "PROMPT_COMMAND";
static char g_store_path[MAX_PATH];
static Stack g_stack;


static char *ptrim(char *s)
{
    if (!*s)
        return s;
    char *e = s + strlen(s) - 1;
    while (e > s && *e == '/') { --e; } *(e + 1) = '\0';
    return s;
}


/* R_SUCCESS = 0, R_FAILURE = 1 */

static Ret
invalid_name(const char *s)
{
    if (!s || !*s || strlen(s) >= MAX_NAME) {
        builtin_error("invalid name");
        return R_FAILURE;
    }
    for (const char *p = s; *p; p++) {
        if (!isalnum((unsigned char) *p)
                && *p != '@'
                && *p != ':'
                && *p != '-'
                && *p != '_'
                && *p != '.'
                && *p != '+') {
            builtin_error("invalid character '%c' in name '%s', allowed characters: [0-9a-Z.@:_+-]", *p, s);
            return R_FAILURE;
        }
    }
    return R_SUCCESS;
}


/* Empty path will try to resolve to PWD */
static Ret
final_path(char *buf, char *path, size_t len)
{
    if (!path || !*path) {
        path = getenv("PWD");
        if (!path) {
            builtin_error("env PWD not set");
            return R_FAILURE;
        }
    }

    if (strlcpy(buf, ptrim(path), len) < len) {
        return R_SUCCESS;
    }

    builtin_error("Could not create entry, path too long");
    return R_FAILURE;
}


/* Store functions: path, init, set, read, write */
static Ret
cdstack_store_path(void)
{
    if (*g_store_path)
        return R_SUCCESS;

    int n = 0;
    const char *home, *file;
    size_t len = sizeof(g_store_path);

    file = getenv("CDSTACK_FILE");
    if (file) {
        n = snprintf(g_store_path, len, "%s", file);
    } else {
        home = getenv("HOME");
        if (home) {
            n = snprintf(g_store_path, len, "%s/.cdstack.bin", home);
        } else {
            builtin_error("Pre-defined location not found, could not create '.cdstack.bin'");
            return R_FAILURE;
        }
    }

    if (n >= 0 && (size_t) n < len) {
        return R_SUCCESS;
    }

    builtin_error("Could not create file, path too long");
    return R_FAILURE;
}


static Ret
cdstack_store_init()
{
    /* Build a fresh, empty store and write it to file */
    /* we force write even if g_stack is empty (maybe not necessary?). */
    memset(&g_stack, 0, sizeof(Stack));
    return cdstack_store_write();
}


static Ret
cdstack_store_set(const char *name, const char *path)
{
    /* update: Update only if entry exists and path differ */
    for (unsigned int i = 0; i < g_stack.count; i++) {
        if (strcmp(g_stack.entries[i].name, name) == 0) {
            if (strcmp(g_stack.entries[i].path, path) != 0) {
                strcpy(g_stack.entries[i].path, path);
                return cdstack_store_write();
            }
            return R_SUCCESS;
        }
    }

    /* full: */
    if (g_stack.count >= MAX_ENTRIES) {
        builtin_error("max number of entries reached");
        return R_FAILURE;
    }

    /* insert: */
    strcpy(g_stack.entries[g_stack.count].name, name);
    strcpy(g_stack.entries[g_stack.count].path, path);
    g_stack.count++;
    return cdstack_store_write();
}


static Ret
cdstack_store_read()
{
    if (cdstack_store_path() == 1)
        return R_FAILURE;

    /* If file havn't changed
       we stick with what we have (g_ready necessary?) */
    struct stat rinfo;
    if (stat(g_store_path, &rinfo) != -1) {
        if (g_ready == 1 && g_mtime == NSEC(&rinfo)) {
            return R_SUCCESS;
        }
    }

    /* If file doesn't exist, we try to create a new */
    errno = 0;
    int fd = open(g_store_path, O_RDONLY);
    if (fd < 0) {
        if (errno == ENOENT) {
            return cdstack_store_init();
        }
        builtin_error("%s", strerror(errno));
        return R_FAILURE;
    }

    Stack t_stack;
    uint32_t e_magic, s_magic;
    ssize_t nb, ne, ns;
    size_t us = sizeof(uint32_t), ss = sizeof(Stack);

    ns = read(fd, &s_magic, us);
    nb = read(fd, &t_stack, ss);
    ne = read(fd, &e_magic, us);
    if (ns < 0 || nb < 0 || ne < 0) {
        builtin_error("%s", strerror(errno));
        close(fd);
        return R_FAILURE;
    } else if ((size_t)(ns + nb + ne) != (2 * us) + ss ||
                e_magic != CDSTACK_EMAGIC || s_magic != CDSTACK_SMAGIC) {
        builtin_error("Could not read from file, magic or size mismatch");
        close(fd);
        return R_FAILURE;
    }

    if (fstat(fd, &rinfo) < 0) {
        builtin_error("%s", strerror(errno));
        close(fd);
        return R_FAILURE;
    }

    close(fd);
    if (t_stack.count > MAX_ENTRIES) {
        builtin_error("Stack entry count out of bounds.");
        return R_FAILURE;
    }

    g_ready = 1;
    g_stack = t_stack;
    g_mtime = NSEC(&rinfo);
    return R_SUCCESS;
}


static Ret
cdstack_store_write()
{
    /* Let's keep track of modify time */
    struct stat winfo;
    char tmpf[MAX_PATH];
    size_t len = sizeof(tmpf);

    int n = snprintf(tmpf, len, "%s.XXXXXX", g_store_path);
    if (n < 0 || (size_t) n >=  len) {
        builtin_error("Could not create tmp file, path too long");
        return R_FAILURE;
    }

    errno = 0;
    int fd = mkstemp(tmpf);
    if (fd < 0) {
        builtin_error("%s", strerror(errno));
        return R_FAILURE;
    }

    uint32_t e_magic = CDSTACK_EMAGIC, s_magic = CDSTACK_SMAGIC;
    ssize_t nb, ne, ns;
    size_t us = sizeof(uint32_t), ss = sizeof(Stack);

    ns = write(fd, &s_magic, us);
    nb = write(fd, &g_stack, ss);
    ne = write(fd, &e_magic, us);
    if (nb < 0 || ne < 0 || ns < 0) {
        builtin_error("%s", strerror(errno));
        close(fd);
        return R_FAILURE;
    } else if ((size_t)(ns + nb + ne) != (2 * us) + ss) {
        builtin_error("Could not write to file, magic or size mismatch");
        close(fd);
        return R_FAILURE;
    }

    if (fstat(fd, &winfo) < 0) {
        builtin_error("%s", strerror(errno));
        close(fd);
        return R_FAILURE;
    }

    close(fd);
    if (rename(tmpf, g_store_path) < 0) {
        builtin_error("%s", strerror(errno));
        unlink(tmpf);
        return R_FAILURE;
    }

    g_ready = 0;
    g_mtime = NSEC(&winfo);
    return R_SUCCESS;
}


/* cmd function: init, set, unset, list, declare */
static Ret
cdstack_cmd_init(void)
{
   /* No need to flush CDSTACK array explicitly
    * dynamic_value hook reloads from store on next reference */
    int ch = 'N';
    char line[4];
    for (;;) {
        printf("Erase current data and write a new binary file? [y/N]: ");
        fflush(stdout);
        if (fgets(line, sizeof(line), stdin) == NULL)
            break;
        if (line[0] == '\n') {
            ch = 'N';
            break;
        }
        ch = toupper((unsigned char) line[0]);
        if ((ch == 'Y' || ch == 'N') && line[1] == '\n')
            break;
    }
    if (ch != 'Y') {
        builtin_error("Aborting...");
        return R_FAILURE;
    }
    return cdstack_store_init();
}


static Ret
cdstack_cmd_set(char *name, char *value)
{
    char path[MAX_PATH];
    return (invalid_name(name) ||
            final_path(path, value, sizeof(path)) ||
            cdstack_store_set(name, path));
}


static Ret
cdstack_cmd_unset(const char *name)
{
    /* Shift remaining entries down */
    for (unsigned int i = 0; i < g_stack.count; i++) {
        if (strncmp(g_stack.entries[i].name, name, MAX_NAME) == 0) {
            memmove(&g_stack.entries[i],
                    &g_stack.entries[i + 1], (g_stack.count - i - 1) * sizeof(Entry));
            g_stack.count--;
            memset(&g_stack.entries[g_stack.count], 0, sizeof(Entry));
            return cdstack_store_write();
        }
    }

    builtin_error("could not delete '%s', name not found", name);
    return R_FAILURE;
}


/*
 * List stack entries from binary store */
static Ret
cdstack_cmd_list(void)
{
    /* On empty store return silently */
    if (g_stack.count == 0)
        return R_SUCCESS;

    char *q;
    for (unsigned int i = 0; i < g_stack.count; i++) {
        q = ansic_shouldquote(g_stack.entries[i].path)
            ? ansic_quote(g_stack.entries[i].path, 0, (int *) 0)
            : sh_contains_shell_metas(g_stack.entries[i].path)
            ? sh_double_quote(g_stack.entries[i].path)
            : g_stack.entries[i].path;

        printf("%-*s %s\n", CDSTACK_COLUMN, g_stack.entries[i].name, q);
        if (q != g_stack.entries[i].path)
             FREE(q);
    }

    return R_SUCCESS;
}


static Ret
cdstack_cmd_declare(void)
{
    if (show_name_attributes(array_name, 0) == 0)
        return R_SUCCESS;

    sh_notfound(array_name);
    return R_FAILURE;
}


/*
 * TODO: Unset members.
 *       Assign assoc array value in compound form.
 */

/* assign_func hook - called on bind_variable, CDSTACK[]= */
static SHELL_VAR *
cdstack_assign_func(SHELL_VAR *v, char *value, UNUSED(arrayind_t ind), char *name)
{
    return cdstack_cmd_set(name, value) == 0 ? v : (SHELL_VAR *) NULL;
}


/* dynamic_value hook - called on find_variable,
   basically everytime CDSTACK is referenced, returns a tempvar */
static SHELL_VAR *
cdstack_dynamic_value(SHELL_VAR *v)
{
    if (cdstack_store_read() == 1)
        return v;

    HASH_TABLE *hash;
    hash = assoc_cell(v);
    assoc_flush(hash);

    for (unsigned int i = 0; i < g_stack.count; i++)
        assoc_insert(hash, savestring(g_stack.entries[i].name), savestring(g_stack.entries[i].path));

    return v;
}


/* dynamic_value hook for prompt command */
static SHELL_VAR *
prompt_dynamic_value(SHELL_VAR *v)
{
    char *larg = save_lastarg();
    if (!larg)
        return v;

    struct stat sb;
    if (stat(larg, &sb) == 0) {
        char *rp = NULL;
        if (S_ISDIR(sb.st_mode)) {
            rp = realpath(larg, NULL);
        } else if (S_ISREG(sb.st_mode)) {
            char *ndir = strdup(dirname(larg));
            rp = realpath(ndir, NULL);
            free(ndir);
        }
        if (rp)
            bind_variable("__last__", rp, 0);
        free(rp);
    }
    free(larg);
    return v;
}

/* load, unload */
static Ret
cdstack_load(UNUSED(char *name))
{
    /* If we fail reading, there's no reason keep going */
    if (cdstack_store_read() == 1)
        return R_FAILURE;

    SHELL_VAR *v, *p;
    v = find_or_make_array_variable(array_name, 2);
    p = find_or_make_array_variable(prompt_command, 0);
    if (!v || !p) {
        return R_FAILURE;
    }
    /* Don't allowed unsetting */
    VSETATTR(v, att_nounset);
    v->assign_func = cdstack_assign_func;
    v->dynamic_value = cdstack_dynamic_value;
    p->dynamic_value = prompt_dynamic_value;
    return R_SUCCESS;
}


static void
cdstack_unload(UNUSED(char *name))
{
    /* Strip hooks before unload */
    SHELL_VAR *v = find_variable_noref(array_name);
    if (v) {
        v->assign_func = (sh_var_assign_func_t *) NULL;
        v->dynamic_value = (sh_var_value_func_t *) NULL;
    }
    SHELL_VAR *p = find_variable_noref(prompt_command);
    if (p) {
        p->dynamic_value = (sh_var_value_func_t *) NULL;
    }
    return;
}

/*
 * find_or_make_array_variable() flags (arrayfunc.c):
 *   1  check the existing variable for the readonly or noassign attribute
 *      before assignment (as the 'read' builtin does)
 *   2  create an associative array instead of an indexed array
 *   4  return noassign variables as themselves instead of NULL, so the
 *      caller can handle them directly rather than treating it as an error
 */

/* Main builtin entry point */
int
cdstack_builtin(WORD_LIST *list)
{
    int f = 0, opt = 0, r = 0;
    char *Sarg = NULL, *Uarg = NULL;

    /* This should trigger the dynamic_value function... */
    SHELL_VAR *v = find_or_make_array_variable (array_name, 3);
    if (!v || g_ready != 1) {
        return EXECUTION_FAILURE;
    }

    /* ...just to be sure */
   if (!v->assign_func || !v->dynamic_value) {
        builtin_error("CDSTACK hooks not attached, try re-enable 'cdstack' module");
        return EXECUTION_FAILURE;
    }

    if (list == NULL)
        return cdstack_cmd_list();

    char options[] = "pis:u:";
    reset_internal_getopt ();
    while ((opt = internal_getopt (list, options)) != -1) {
        // UFLAG and SFLAG are mutually exclusive, last assigned wins.
        switch (opt)
        {
            case 'p': /* cdstack_cmd_declare */
                f |= PFLAG;
                break;
            case 'i': /* cdstack_cmd_init */
                f |= IFLAG;
                break;
            case 's': /* cdstack_cmd_set */
                f |= SFLAG; f &= ~UFLAG;
                Sarg = list_optarg;
                break;
            case 'u': /* cdstack_cmd_unset */
                f |= UFLAG; f &= ~SFLAG;
                Uarg = list_optarg;
                break;
            CASE_HELPOPT;
            default:
                builtin_usage();
                return EX_USAGE;
         }
    }

    list = loptend;
    char *word = (list) ? list->word->word : NULL;
    if (!f) {
        sh_invalidopt(word);
        return EXECUTION_FAILURE;
    }

    if (list)
        list = list->next;

    if (f & IFLAG)
        r = cdstack_cmd_init();

    if (!r && f & SFLAG) {
        if (!list)
            r = cdstack_cmd_set(Sarg, word);
        else {
            builtin_error("too many arguments");
            return EXECUTION_FAILURE;
        }
    }

    //XXX: unset multiple names.
    if (!r && f & UFLAG) {
        if (!word)
            r = cdstack_cmd_unset(Uarg);
        else {
            builtin_error("too many arguments");
            return EXECUTION_FAILURE;
        }
    }

    if (!r && f & PFLAG)
        r = cdstack_cmd_declare();

    return r;
}


/* Called when builtin is enabled and loaded from the
 * shared object. If this function returns --> 0 <--, the load fails. */
int
cdstack_builtin_load(char *name)
{
    return cdstack_load(name) == 0;
}


/* Called when builtin is disabled. */
void
cdstack_builtin_unload(char *name)
{
    cdstack_unload(name);
}


/* Documentation for the builtin */
const char * const cdstack_doc[] = {
    "Persistent named directory manager.",
    "",
    "Options:",
    "  cdstack                   List all entries",
    "  cdstack -i                Create a new binary file",
    "  cdstack -p                Alias for declare -p",
    "  cdstack -s <name> [path]  Set name, path pair (defaults to $PWD)",
    "  cdstack -u <name>         Unset an entry",
    "",
    "Entries can be assigned or referenced using the CDSTACK array:",
    "  $ CDSTACK[doc]=/usr/share/doc",
    "  $ echo $\"{CDSTACK[doc]}\"",
    "",
    "Exit Status:",
    "  Returns success unless an error occurs.",
    "",
    (char *) NULL
};

char doc_name[] = "cdstack";

/* Struct definition for the builtin */
struct builtin cdstack_struct = {
    doc_name,                                           /* The name the user types */
    cdstack_builtin,                                    /* The function address */
    BUILTIN_ENABLED,                                    /* This builtin is enabled. */
    .long_doc = (char * const *) cdstack_doc,           /* Long documentation */
    "cdstack [-i | -p | -s <name> [path] | -u <name>]", /* Short documentation */
    0                                                   /* Handle, unused for now */
};
