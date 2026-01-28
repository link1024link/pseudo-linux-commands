/* =========================================================
 * Linux コマンド簡易シミュレータ（学習用）
 *
 * 目的:
 *  - Linuxコマンドの内部処理を理解する
 *  - ツリー構造・ポインタ・メモリ管理の学習
 *
 * 特徴:
 *  - 実ファイルシステムは使用しない
 *  - メモリ上に仮想ディレクトリツリーを構築
 *  - 完全再現ではなく仕組み理解を優先
 * ========================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== 定数定義 ===== */
#define NAME_LEN     32
#define LINE_LEN     128
#define MAX_FILES    16
#define MAX_SUBDIRS  16
#define MAX_CONTENT  512

/* ===== ファイル構造体 ===== */
struct File {
    char name[NAME_LEN];
    int  size;
    char perm[8];
    char content[MAX_CONTENT];
};

/* ===== ディレクトリ構造体 ===== */
struct Dir {
    char name[NAME_LEN];
    struct Dir *parent;
    struct File files[MAX_FILES];
    int file_count;
    struct Dir *subdirs[MAX_SUBDIRS];
    int subdir_count;
};

/* ===== ユーティリティ ===== */

static void trim_newline(char *s) {
    size_t n = strlen(s);
    if (n > 0 && s[n - 1] == '\n') {
        s[n - 1] = '\0';
    }
}

static int find_file_index(const struct Dir *d, const char *name) {
    for (int i = 0; i < d->file_count; i++) {
        if (strcmp(d->files[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int find_subdir_index(const struct Dir *d, const char *name) {
    for (int i = 0; i < d->subdir_count; i++) {
        if (strcmp(d->subdirs[i]->name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static struct Dir *create_dir(const char *name, struct Dir *parent) {
    struct Dir *d = malloc(sizeof(struct Dir));
    if (!d) return NULL;

    strncpy(d->name, name, NAME_LEN - 1);
    d->name[NAME_LEN - 1] = '\0';
    d->parent = parent;
    d->file_count = 0;
    d->subdir_count = 0;

    return d;
}

/* ===== コマンド実装 ===== */

static void pwd_cmd(struct Dir *cwd) {
    if (cwd->parent == NULL) {
        puts("/");
        return;
    }

    const char *parts[64];
    int depth = 0;

    while (cwd) {
        parts[depth++] = cwd->name;
        cwd = cwd->parent;
    }

    for (int i = depth - 1; i >= 0; i--) {
        if (i == 0 && strcmp(parts[i], "/") == 0) {
            printf("/");
        } else {
            printf("%s", parts[i]);
            if (i > 0) printf("/");
        }
    }
    putchar('\n');
}

static void ls_cmd(struct Dir *cwd, const char *opt) {
    int longfmt = (opt && strcmp(opt, "-l") == 0);

    for (int i = 0; i < cwd->subdir_count; i++) {
        if (longfmt) {
            printf("drwx ---- %s/\n", cwd->subdirs[i]->name);
        } else {
            printf("%s/\n", cwd->subdirs[i]->name);
        }
    }

    for (int i = 0; i < cwd->file_count; i++) {
        struct File *f = &cwd->files[i];
        if (longfmt) {
            printf("-%s %4d %s\n", f->perm, f->size, f->name);
        } else {
            printf("%s\n", f->name);
        }
    }
}

static void touch_cmd(struct Dir *cwd, const char *name) {
    if (!name) {
        puts("usage: touch <name>");
        return;
    }

    if (find_file_index(cwd, name) != -1 ||
        find_subdir_index(cwd, name) != -1) {
        puts("name already exists");
        return;
    }

    if (cwd->file_count >= MAX_FILES) {
        puts("file limit reached");
        return;
    }

    struct File *f = &cwd->files[cwd->file_count++];
    strncpy(f->name, name, NAME_LEN - 1);
    f->name[NAME_LEN - 1] = '\0';
    strcpy(f->perm, "rw-");
    f->size = 0;
    f->content[0] = '\0';

    printf("file '%s' created\n", name);
}

static void rm_cmd(struct Dir *cwd, const char *name) {
    if (!name) {
        puts("usage: rm <name>");
        return;
    }

    int idx = find_file_index(cwd, name);
    if (idx < 0) {
        puts("no such file");
        return;
    }

    for (int i = idx; i < cwd->file_count - 1; i++) {
        cwd->files[i] = cwd->files[i + 1];
    }
    cwd->file_count--;

    printf("file '%s' removed\n", name);
}

static void mv_cmd(struct Dir *cwd, const char *src, const char *dst) {
    if (!src || !dst) {
        puts("usage: mv <old> <new>");
        return;
    }

    int idx = find_file_index(cwd, src);
    if (idx < 0) {
        puts("source not found");
        return;
    }

    if (find_file_index(cwd, dst) != -1 ||
        find_subdir_index(cwd, dst) != -1) {
        puts("destination already exists");
        return;
    }

    strncpy(cwd->files[idx].name, dst, NAME_LEN - 1);
    cwd->files[idx].name[NAME_LEN - 1] = '\0';

    printf("renamed '%s' -> '%s'\n", src, dst);
}

static void mkdir_cmd(struct Dir *cwd, const char *name) {
    if (!name) {
        puts("usage: mkdir <name>");
        return;
    }

    if (cwd->subdir_count >= MAX_SUBDIRS) {
        puts("subdir limit reached");
        return;
    }

    if (find_file_index(cwd, name) != -1 ||
        find_subdir_index(cwd, name) != -1) {
        puts("name already exists");
        return;
    }

    struct Dir *d = create_dir(name, cwd);
    if (!d) {
        puts("memory error");
        return;
    }

    cwd->subdirs[cwd->subdir_count++] = d;
    printf("directory '%s' created\n", name);
}

static struct Dir *cd_cmd(struct Dir *cwd, const char *arg, struct Dir *root) {
    if (!arg) {
        puts("usage: cd <dir>");
        return cwd;
    }

    if (strcmp(arg, "/") == 0) return root;
    if (strcmp(arg, ".") == 0) return cwd;
    if (strcmp(arg, "..") == 0) return cwd->parent ? cwd->parent : cwd;

    int idx = find_subdir_index(cwd, arg);
    if (idx >= 0) {
        return cwd->subdirs[idx];
    }

    puts("no such directory");
    return cwd;
}

static void free_dir(struct Dir *d) {
    if (!d) return;

    for (int i = 0; i < d->subdir_count; i++) {
        free_dir(d->subdirs[i]);
    }
    free(d);
}

/* ===== メイン ===== */

int main(void) {
    struct Dir *root = create_dir("/", NULL);
    struct Dir *cwd = root;
    char line[LINE_LEN];

    while (1) {
        printf("pseudo-linux:%s> ", cwd == root ? "/" : cwd->name);

        if (!fgets(line, sizeof(line), stdin)) break;
        trim_newline(line);

        if (line[0] == '\0') continue;

        char *cmd = strtok(line, " ");
        char *arg = strtok(NULL, " ");

        if (strcmp(cmd, "exit") == 0) break;
        else if (strcmp(cmd, "pwd") == 0 || strcmp(cmd, "pwt") == 0) pwd_cmd(cwd);
        else if (strcmp(cmd, "ls") == 0) ls_cmd(cwd, arg);
        else if (strcmp(cmd, "touch") == 0) touch_cmd(cwd, arg);
        else if (strcmp(cmd, "rm") == 0) rm_cmd(cwd, arg);
        else if (strcmp(cmd, "mv") == 0) {
            char *dst = strtok(NULL, " ");
            mv_cmd(cwd, arg, dst);
        }
        else if (strcmp(cmd, "mkdir") == 0) mkdir_cmd(cwd, arg);
        else if (strcmp(cmd, "cd") == 0) cwd = cd_cmd(cwd, arg, root);
        else {
            puts("command not found");
        }
    }

    free_dir(root);
    return 0;
}
