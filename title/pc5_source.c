/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* PC5: game folders on USB drives (pc5_source.h). */
#include "pc5_source.h"
#include "pc5_cover.h"

#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Folders a drive keeps for itself. */
static int system_folder(const char *name)
{
    static const char *const names[] = { "System Volume Information", "$RECYCLE.BIN", "RECYCLER", "PS5", "PS4",
                                         "LOST.DIR", "FOUND.000", ".Trashes", ".Spotlight-V100", ".fseventsd" };
    if (name[0] == '.') return 1;
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        if (!pc5_casecmp(name, names[i])) return 1;
    return 0;
}

/* Is there a .exe somewhere in path, depth levels down at most? */
static int holds_program(const char *path, int depth)
{
    DIR *dir = opendir(path);
    struct dirent *entry;
    char inner[PC5_PATH];
    int found = 0;

    if (!dir) return 0;
    while (!found && (entry = readdir(dir))) {
        if (entry->d_name[0] == '.') continue;
        if (pc5_join(inner, sizeof(inner), path, entry->d_name)) continue;
        if (pc5_ends_with_ci(entry->d_name, ".exe") && pc5_is_file(inner)) found = 1;
    }
    if (!found && depth > 0) {
        rewinddir(dir);
        while (!found && (entry = readdir(dir))) {
            if (entry->d_name[0] == '.' || pc5_join(inner, sizeof(inner), path, entry->d_name)) continue;
            if (pc5_is_dir(inner)) found = holds_program(inner, depth - 1);
        }
    }
    closedir(dir);
    return found;
}

static int by_name(const void *a, const void *b)
{
    return pc5_casecmp(((const Pc5Source *)a)->name, ((const Pc5Source *)b)->name);
}

static void scan_folder(const char *path, int drive, Pc5Source *out, int max, int *count)
{
    DIR *dir = opendir(path);
    struct dirent *entry;
    char inner[PC5_PATH];

    if (!dir) return;
    while (*count < max && (entry = readdir(dir))) {
        if (system_folder(entry->d_name) || pc5_join(inner, sizeof(inner), path, entry->d_name) || !pc5_is_dir(inner))
            continue;
        if (!pc5_casecmp(entry->d_name, "Games")) continue;   /* gone into below */
        if (strlen(entry->d_name) >= PC5_NAME || !holds_program(inner, PC5_DEPTH)) continue;
        Pc5Source *source = &out[(*count)++];
        pc5_copy(source->name, sizeof(source->name), entry->d_name);
        pc5_copy(source->path, sizeof(source->path), inner);
        source->drive = drive;
    }
    closedir(dir);
}

int pc5_source_scan(const char *const roots[], Pc5Source *out, int max)
{
    int count = 0;

    for (int i = 0; roots[i]; i++) {
        char games[PC5_PATH];
        if (!pc5_is_dir(roots[i])) continue;
        scan_folder(roots[i], i, out, max, &count);
        /* A Games folder at the top holds game folders too. */
        DIR *dir = opendir(roots[i]);
        struct dirent *entry;
        if (!dir) continue;
        while ((entry = readdir(dir)))
            if (!pc5_casecmp(entry->d_name, "Games") && !pc5_join(games, sizeof(games), roots[i], entry->d_name) &&
                pc5_is_dir(games))
                scan_folder(games, i, out, max, &count);
        closedir(dir);
    }
    qsort(out, (size_t)count, sizeof(out[0]), by_name);
    return count;
}

/* ---- one folder ----------------------------------------------------------------------- */

static int contains_ci(const char *text, const char *word)
{
    size_t n = strlen(word);
    for (; *text; text++) {
        size_t k = 0;
        while (k < n && text[k] && pc5_lower((unsigned char)text[k]) == word[k]) k++;
        if (k == n) return 1;
    }
    return 0;
}

/* Folders with installers and runtimes, not the game. */
static int helper_folder(const char *relative)
{
    static const char *const words[] = { "_commonredist", "redist", "directx", "vcredist", "dotnetfx",
                                         "__installer", "installers", "easyanticheat", "battleye", "prereq",
                                         "thirdparty", "support" };
    for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); i++)
        if (contains_ci(relative, words[i])) return 1;
    return 0;
}

/* Programs that are never the game. */
static int helper_program(const char *name)
{
    static const char *const words[] = { "unins", "setup", "install", "update", "patch", "redist", "dxsetup",
                                         "dxweb", "dotnet", "crash", "report", "easyanticheat", "battleye",
                                         "beservice", "errorreport", "prereq", "physx", "oalinst", "editor",
                                         "dedicated", "server", "uploader", "cefprocess", "subprocess",
                                         "notification", "touchup", "activation", "register", "quicksfv",
                                         "7z", "unrar", "cleanup", "repair", "diagnos", "benchmark", "helper" };
    for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); i++)
        if (contains_ci(name, words[i])) return 1;
    return 0;
}

/* A name Windows (or the profile format) can't take. */
static int bad_windows_name(const char *name)
{
    static const char *const reserved[] = { "con", "prn", "aux", "nul", "com1", "com2", "com3", "com4", "com5",
                                            "com6", "com7", "com8", "com9", "lpt1", "lpt2", "lpt3", "lpt4",
                                            "lpt5", "lpt6", "lpt7", "lpt8", "lpt9" };
    size_t n = strlen(name);
    char base[8];
    size_t k = 0;

    if (!n || name[n - 1] == '.' || name[n - 1] == ' ') return 1;
    for (const unsigned char *p = (const unsigned char *)name; *p; p++)
        if (*p < 0x20 || strchr("<>:\"|?*\\", *p)) return 1;
    while (k < n && k + 1 < sizeof(base) && name[k] != '.') { base[k] = (char)pc5_lower((unsigned char)name[k]); k++; }
    base[k] = 0;
    if (k == n || name[k] == '.')
        for (size_t i = 0; i < sizeof(reserved) / sizeof(reserved[0]); i++)
            if (!strcmp(base, reserved[i])) return 1;
    return 0;
}

static int by_score(const void *a, const void *b)
{
    const Pc5Exe *x = a, *y = b;
    if (x->score != y->score) return y->score - x->score;
    return strcmp(x->path, y->path);
}

/* Libraries beside the programs: what they draw with, by folder. */
typedef struct DllDir { char dir[PC5_PATH]; uint32_t imports; } DllDir;

typedef struct Walk {
    Pc5Folder *folder;
    Pc5Exe *all;               /* every program found, before ranking */
    int all_count, all_max;
    DllDir *dlls;
    int dll_count, dll_max;
} Walk;

static void add_dll_imports(Walk *walk, const char *dir, uint32_t imports)
{
    for (int i = 0; i < walk->dll_count; i++)
        if (!strcmp(walk->dlls[i].dir, dir)) { walk->dlls[i].imports |= imports; return; }
    if (walk->dll_count < walk->dll_max) {
        pc5_copy(walk->dlls[walk->dll_count].dir, PC5_PATH, dir);
        walk->dlls[walk->dll_count++].imports = imports;
    }
}

static int score_program(const Pc5Folder *folder, const Pc5Exe *exe, const char *relative)
{
    const char *slash = strrchr(relative, '/'), *name = slash ? slash + 1 : relative;
    char base[PC5_NAME];
    int score = 100, depth = 0;

    snprintf(base, sizeof(base), "%.*s", (int)(strlen(name) > 4 ? strlen(name) - 4 : strlen(name)), name);
    for (const char *p = relative; *p; p++) depth += *p == '/';
    if (exe->pe.dll) return -1000;
    if (helper_folder(relative) || helper_program(name)) score -= 1000;
    if (contains_ci(name, "launch") || contains_ci(name, "config") || contains_ci(name, "settings")) score -= 40;
    if (contains_ci(name, "-shipping")) score += 60;
    if (!exe->pe.gui) score -= 30;
    if (exe->pe.bits == 64) score += 10;
    score -= depth * 5;
    score += pc5_cover_similarity(folder->name, base) / 2;
    if (exe->pe.product[0]) score += pc5_cover_similarity(folder->name, exe->pe.product) / 4;
    if (exe->size > 1024 * 1024) score += (int)(log2((double)exe->size / (1024 * 1024)) * 6);
    else score -= 20;   /* a stub that starts the real program */
    return score;
}

/* 1 when program a starts program b: PlayX.exe, LaunchX.exe, StartX.exe or RunX.exe (with or
 * without a space, - or _) beside X.exe. Such a starter runs the game as a second process,
 * which Wine on the console doesn't do, so X.exe is the one to start. */
static int starter_of(const char *a, const char *b)
{
    static const char *const words[] = { "play", "launch", "launcher", "start", "run", "go" };
    const char *sa = strrchr(a, '/'), *sb = strrchr(b, '/');
    size_t da = sa ? (size_t)(sa - a) : 0, db = sb ? (size_t)(sb - b) : 0;
    const char *na = sa ? sa + 1 : a, *nb = sb ? sb + 1 : b;
    size_t la = strlen(na), lb = strlen(nb);

    if (da != db || strncmp(a, b, da) || la <= lb || la - lb > 10) return 0;
    if (pc5_casecmp(na + (la - lb), nb)) return 0;      /* ends with the other's whole name */
    size_t prefix = la - lb;
    while (prefix && (na[prefix - 1] == ' ' || na[prefix - 1] == '-' || na[prefix - 1] == '_')) prefix--;
    for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); i++) {
        size_t n = strlen(words[i]), k = 0;
        while (k < n && k < prefix && (na[k] | 0x20) == words[i][k]) k++;
        if (n == prefix && k == n) return 1;
    }
    return 0;
}

int pc5_folder_inspect(const char *path, const char *name, Pc5Folder *folder)
{
    enum { STACK = 4096 };
    char (*stack)[PC5_PATH] = pc5_alloc(sizeof(*stack) * STACK);
    Walk walk = { folder, pc5_alloc(sizeof(Pc5Exe) * 256), 0, 256, pc5_alloc(sizeof(DllDir) * 256), 0, 256 };
    int top = 0, status = 0;

    memset(folder, 0, sizeof(*folder));
    pc5_copy(folder->path, sizeof(folder->path), path);
    pc5_copy(folder->name, sizeof(folder->name), name);
    if (!stack || !walk.all || !walk.dlls || !pc5_is_dir(path)) { status = -1; goto out; }
    stack[top++][0] = 0;   /* "": the folder itself */
    while (top > 0) {
        char relative[PC5_PATH], dir_path[PC5_PATH], entry_path[PC5_PATH], child[PC5_PATH];
        DIR *dir;
        struct dirent *entry;
        int depth = 0;

        memcpy(relative, stack[--top], PC5_PATH);
        for (const char *p = relative; *p; p++) depth += *p == '/';
        if (relative[0] ? pc5_join(dir_path, sizeof(dir_path), path, relative)
                        : pc5_format(dir_path, sizeof(dir_path), "%s", path)) {
            snprintf(folder->problem, sizeof(folder->problem), "A path in the folder is too long.");
            continue;
        }
        if (!(dir = opendir(dir_path))) {
            snprintf(folder->problem, sizeof(folder->problem), "PC5 can't read %.200s.", relative[0] ? relative : name);
            continue;
        }
        while ((entry = readdir(dir))) {
            struct stat st;
            if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
            if ((relative[0] ? pc5_format(child, sizeof(child), "%s/%s", relative, entry->d_name)
                             : pc5_format(child, sizeof(child), "%s", entry->d_name)) ||
                pc5_join(entry_path, sizeof(entry_path), path, child)) {
                snprintf(folder->problem, sizeof(folder->problem), "A path in the folder is too long.");
                continue;
            }
            if (lstat(entry_path, &st)) continue;
            if (bad_windows_name(entry->d_name) && !folder->problem[0])
                snprintf(folder->problem, sizeof(folder->problem), "Windows can't use the name \"%.180s\".", child);
            if (S_ISDIR(st.st_mode)) {
                folder->folders++;
                if (depth + 1 > 40 || top >= STACK) {
                    snprintf(folder->problem, sizeof(folder->problem), "The folder goes too many levels deep.");
                    continue;
                }
                memcpy(stack[top++], child, PC5_PATH);
            } else if (S_ISREG(st.st_mode)) {
                folder->files++;
                folder->bytes += (uint64_t)st.st_size;
                int is_exe = pc5_ends_with_ci(entry->d_name, ".exe"), is_dll = pc5_ends_with_ci(entry->d_name, ".dll");
                if ((is_exe || is_dll) && !helper_folder(child)) {
                    Pc5PeInfo pe;
                    if (pc5_pe_read(entry_path, &pe)) continue;
                    if (is_dll || pe.dll) {
                        add_dll_imports(&walk, relative, pe.imports);
                    } else if (depth <= PC5_DEPTH && walk.all_count < walk.all_max) {
                        Pc5Exe *exe = &walk.all[walk.all_count++];
                        pc5_copy(exe->path, sizeof(exe->path), child);
                        exe->size = (uint64_t)st.st_size;
                        exe->pe = pe;
                        exe->score = score_program(folder, exe, child);
                    }
                    folder->imports |= pe.imports;
                } else if (!relative[0] && !pc5_casecmp(entry->d_name, "steam_appid.txt")) {
                    Pc5Buf text = { 0 };
                    if (!pc5_read_file(entry_path, &text, 64)) folder->steam_appid = (uint32_t)strtoul((char *)text.data, NULL, 10);
                    pc5_buf_free(&text);
                } else if (!relative[0] && !folder->picture[0] &&
                           (!pc5_casecmp(entry->d_name, "cover.jpg") || !pc5_casecmp(entry->d_name, "cover.jpeg") ||
                            !pc5_casecmp(entry->d_name, "cover.png"))) {
                    pc5_copy(folder->picture, sizeof(folder->picture), entry_path);
                }
            }
            /* links and devices are left out: exFAT and FAT32 have none */
        }
        closedir(dir);
    }
    for (int i = 0; i < walk.all_count; i++)
        for (int k = 0; k < walk.all_count; k++)
            if (i != k && starter_of(walk.all[i].path, walk.all[k].path)) { walk.all[i].score -= 80; break; }
    qsort(walk.all, (size_t)walk.all_count, sizeof(Pc5Exe), by_score);
    for (int i = 0; i < walk.all_count && folder->exe_count < PC5_EXES; i++)
        if (walk.all[i].score > -500 || folder->exe_count == 0) folder->exes[folder->exe_count++] = walk.all[i];
    /* Each program's own imports, joined by its folder's libraries. */
    for (int i = 0; i < folder->exe_count; i++) {
        char dir[PC5_PATH];
        char *slash;
        pc5_copy(dir, sizeof(dir), folder->exes[i].path);
        slash = strrchr(dir, '/');
        if (slash) *slash = 0; else dir[0] = 0;
        for (int k = 0; k < walk.dll_count; k++)
            if (!strcmp(walk.dlls[k].dir, dir)) folder->exes[i].beside |= walk.dlls[k].imports;
    }
    /* A game whose programs and libraries import nothing they draw with loads it at run
     * time: what its likeliest programs name (pc5_pe_named_libraries), read only then. */
    if (!(folder->imports & PC5_IMPORT_DRAWING)) {
        for (int i = 0; i < folder->exe_count && i < 3; i++) {
            char file[PC5_PATH * 2];
            if (pc5_format(file, sizeof(file), "%s/%s", path, folder->exes[i].path)) continue;
            folder->exes[i].pe.imports |= pc5_pe_named_libraries(file);
        }
    }
    if (!folder->exe_count && !folder->problem[0])
        snprintf(folder->problem, sizeof(folder->problem), "There is no Windows program in this folder.");
    else if (folder->exe_count && folder->exes[0].score <= -500 && !folder->problem[0])
        snprintf(folder->problem, sizeof(folder->problem), "This folder only has installers and tools, not a game.");
out:
    pc5_free(stack);
    pc5_free(walk.all);
    pc5_free(walk.dlls);
    return status;
}

Pc5Api pc5_folder_api(const Pc5Folder *folder, int index)
{
    const uint32_t drawing = PC5_IMPORT_DRAWING;
    uint32_t own, beside;

    if (index < 0 || index >= folder->exe_count) return PC5_API_GDI;
    own = folder->exes[index].pe.imports;
    beside = folder->exes[index].beside;
    if (own & drawing) return pc5_api_from_imports(own);
    if (beside & drawing) return pc5_api_from_imports(beside);
    return pc5_api_from_imports(folder->imports);
}
