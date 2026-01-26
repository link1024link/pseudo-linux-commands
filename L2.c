/* ========== プログラム全体の仕組みと構造 ==========
 * 
 * 【プログラムの目的】
 *   Linux コマンドラインの簡易シミュレータ
 *   メモリ上に仮想ファイルシステムを構築し、基本的なファイル操作を体験
 * 
 * 【データ構造の設計】
 *   ツリー構造を使用:
 *   - struct Dir: ディレクトリノード
 *     ├ parent: 親ディレクトリへのポインタ（NULLならルート）
 *     ├ files[]: ファイル配列（最大 MAX_FILES 個）
 *     ├ file_count: 実際のファイル数
 *     ├ subdirs[]: サブディレクトリ配列（最大 MAX_SUBDIRS 個）
 *     └ subdir_count: 実際のサブディレクトリ数
 * 
 *   - struct File: ファイルノード
 *     ├ name: ファイル名
 *     ├ size: ファイルサイズ
 *     ├ perm: パーミッション（"rw-" など）
 *     └ content: ファイル内容
 * 
 * 【階層構造の例】
 *   root (/)
 *   ├─ file1.txt
 *   ├─ subdir1/
 *   │  ├─ file2.txt
 *   │  └─ nested_dir/
 *   └─ subdir2/
 *      └─ file3.txt
 * 
 * 【実装コマンド一覧】
 *   touch <name>     : ファイル作成（新規、空ファイル）
 *   ls [-l]          : ファイル/ディレクトリ一覧表示
 *   rm <name>        : ファイル削除（配列から除外）
 *   mv <old> <new>   : ファイルをリネーム（同一ディレクトリ内）
 *   mkdir <name>     : サブディレクトリ作成（新規ノード生成）
 *   cd <dir>         : ディレクトリ移動（特殊パス /, .., . 対応）
 *   pwd / pwt        : 現在のパス表示（ルートから現在地までをトレース）
 *   exit             : プログラム終了（メモリ解放）
 * 
 * 【エラーチェックの設計思想】
 *   各操作の前に必ず以下をチェック:
 *   1. 引数の存在確認: NULL チェック（ユーザーが引数を指定したか）
 *   2. 対象の存在確認: find_file_index/find_subdir_index で検索
 *   3. 制限チェック: file_count >= MAX_FILES などの上限確認
 *   4. 重複確認: 同じ名前が既に存在しないか
 *   5. 型チェック: ファイル名がディレクトリと重複していないか
 * 
 *   これにより、メモリ破損や予期しない動作を防止
 * 
 * 【プログラム実行フロー】
 *   1. メイン処理
 *      └─ ルートディレクトリを作成、現在地を初期化
 *   2. 無限ループ（ユーザー入力待機）
 *      ├─ プロンプト表示
 *      ├─ ユーザー入力を受け取り（fgets）
 *      ├─ コマンドと引数に分割（strtok）
 *      ├─ コマンド分岐処理
 *      │  └─ 各コマンド関数を実行
 *      └─ "exit" で無限ループを抜ける
 *   3. クリーンアップ
 *      └─ ツリー全体をメモリ解放（メモリリーク防止）
 * 
 * ========== */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* マクロ定数の定義 */
#define NAME_LEN  32        /* ファイル・ディレクトリ名の最大長 */
#define LINE_LEN  128       /* コマンド入力行の最大長 */
#define MAX_FILES 16        /* 1つのディレクトリに保持できる最大ファイル数 */
#define MAX_SUBDIRS 16      /* 1つのディレクトリに保持できる最大サブディレクトリ数 */
#define MAX_CONTENT 512     /* ファイル内容の最大バイト数 */

/* ファイルの属性を管理する構造体 */
struct File {
    char name[NAME_LEN];        /* ファイル名 */
    int size;                   /* ファイルサイズ（バイト） */
    char perm[8];               /* ファイルパーミッション（例: "rw-" ） */
    char content[MAX_CONTENT];  /* ファイル内容 */
};

/* ディレクトリノードを管理する構造体（ツリー構造） */
struct Dir {
    char name[NAME_LEN];              /* ディレクトリ名 */
    struct Dir *parent;               /* 親ディレクトリへのポインタ（NULLならルート） */
    struct File files[MAX_FILES];     /* このディレクトリ内のファイル配列 */
    int file_count;                   /* このディレクトリ内のファイル数 */
    struct Dir *subdirs[MAX_SUBDIRS]; /* このディレクトリ内のサブディレクトリ配列 */
    int subdir_count;                 /* このディレクトリ内のサブディレクトリ数 */
};

/* ========== ヘルパー関数群 ========== */

/* 文字列末尾の改行文字を削除（fgetsで読み込んだ入力から改行を除去するため） */
static void trim_newline(char *s) {
    size_t n = strlen(s);
    if (n > 0 && s[n - 1] == '\n') s[n - 1] = '\0';
}

/* 指定ディレクトリ内でファイルを検索し、そのインデックスを返す
   戻り値: ファイルが見つかったインデックス（0以上）、見つからない場合は-1 */
static int find_file_index(const struct Dir *d, const char *name) {
    for (int i = 0; i < d->file_count; i++) {
        if (strcmp(d->files[i].name, name) == 0) return i;
    }
    return -1;
}

/* 指定ディレクトリ内でサブディレクトリを検索し、そのインデックスを返す
   戻り値: サブディレクトリが見つかったインデックス（0以上）、見つからない場合は-1 */
static int find_subdir_index(const struct Dir *d, const char *name) {
    for (int i = 0; i < d->subdir_count; i++) {
        if (strcmp(d->subdirs[i]->name, name) == 0) return i;
    }
    return -1;
}

/* 新しいディレクトリノードを作成してメモリを確保
   返り値: 作成されたDirポインタ、メモリ確保失敗時はNULL */
static struct Dir *create_dir_node(const char *name, struct Dir *parent) {
    struct Dir *d = malloc(sizeof(struct Dir));
    if (!d) return NULL;  /* メモリ確保に失敗した場合はNULLを返す */
    strncpy(d->name, name, NAME_LEN - 1);
    d->name[NAME_LEN - 1] = '\0';
    d->parent = parent;
    d->file_count = 0;
    d->subdir_count = 0;
    return d;
}

/* pwd: 現在のパスを表示（ルートから現在ディレクトリまでの絶対パスを出力） */
static void pwd_cmd(struct Dir *cwd) {
    if (cwd->parent == NULL) {  /* ルートディレクトリの場合 */
        puts("/");
        return;
    }
    /* 親ポインタをたどってディレクトリ名の配列を後ろから構築 */
    const int MAX_DEPTH = 64;
    const char *parts[MAX_DEPTH];
    int depth = 0;
    struct Dir *p = cwd;
    /* 現在ディレクトリからルートへさかのぼってパス部分を収集 */
    while (p != NULL && depth < MAX_DEPTH) {
        parts[depth++] = p->name;
        p = p->parent;
    }
    /* 逆順で出力してパスを組み立てる（ルートから現在地へ） */
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

/* ls: ディレクトリ内の一覧表示（opt="-l"でロング形式） */
static void ls_cmd(struct Dir *cwd, const char *opt) {
    /* ディレクトリが空の場合は早期終了 */
    if (cwd->subdir_count == 0 && cwd->file_count == 0) {
        puts("ls: no entries");
        return;
    }
    /* -lオプションでロング形式表示か判定 */
    int longfmt = (opt && strcmp(opt, "-l") == 0);
    /* サブディレクトリを表示（末尾に / を付ける） */
    for (int i = 0; i < cwd->subdir_count; i++) {
        struct Dir *sd = cwd->subdirs[i];
        if (longfmt) {
            printf("drwx %4s %s/\n", "-", sd->name);
        } else {
            printf("%s/\n", sd->name);
        }
    }
    /* ファイルを表示 */
    for (int i = 0; i < cwd->file_count; i++) {
        struct File *f = &cwd->files[i];
        if (longfmt) {
            /* ファイルは先頭に "-" を付けて型を明示 */
            printf("-%s %4d %s\n", f->perm, f->size, f->name);
        } else {
            printf("%s\n", f->name);
        }
    }
}

/* touch: ファイル作成 */
static void touch_cmd(struct Dir *cwd, const char *name) {
    /* 引数チェック: ユーザーがファイル名を指定したか */
    if (!name) { puts("usage: touch <name>"); return; }
    
    /* 重複チェック: 同じ名を持つファイルが既にあるか
       永続確保を粗って的に拉否し、不幸な上書きを防ぐ */
    if (find_file_index(cwd, name) != -1) {
        printf("file '%s' already exists\n", name);
        return;
    }
    
    /* 上限チェック: ディレクトリが最大数のファイルを持っていにいか
       配列バッファオーバーフローを防の */
    if (cwd->file_count >= MAX_FILES) { puts("file limit reached"); return; }
    
    /* 型重複チェック: 同じ名を持つディレクトリがあるか
       ファイルとディレクトリは合作できない */
    if (find_subdir_index(cwd, name) != -1) { printf("name '%s' already used by directory\n", name); return; }

    /* 安全判定後アクセス: 新規ファイル篠を作成し適後に分配 */
    struct File *f = &cwd->files[cwd->file_count++];
    strncpy(f->name, name, NAME_LEN - 1);
    f->name[NAME_LEN - 1] = '\0';
    f->size = 0;
    strncpy(f->perm, "rw-", sizeof(f->perm) - 1);
    f->perm[sizeof(f->perm) - 1] = '\0';
    f->content[0] = '\0';
    printf("file '%s' created\n", name);
}

/* rm: ファイル削除 */
static void rm_cmd(struct Dir *cwd, const char *name) {
    /* 引数チェック: ユーザーが削除対象ファイルを指定したか */
    if (!name) { puts("usage: rm <name>"); return; }
    
    /* 存在確認: 削除対象ファイルを検索 */
    int idx = find_file_index(cwd, name);
    if (idx >= 0) {
        /* 配列詰め込み: 削除対象の後ろの要素を後ろにし、配列を詰めていく */
        for (int j = idx; j < cwd->file_count - 1; j++) cwd->files[j] = cwd->files[j + 1];
        cwd->file_count--;  /* ファイル数を減らす */
        printf("file '%s' removed\n", name);
    } else {
        /* ファイルが見つからない場合 */
        puts("no such file");
    }
}

/* mv: ファイルのリネーム（同じディレクトリ内で名前を変更） */
static void mv_cmd(struct Dir *cwd, const char *src, const char *dst) {
    /* 引数チェック */
    if (!src || !dst) {
        puts("usage: mv <old_name> <new_name>");
        return;
    }
    
    /* 移動元ファイルが存在するか確認 */
    int idx = find_file_index(cwd, src);
    if (idx < 0) {
        printf("mv: '%s' not found\n", src);
        return;
    }
    
    /* 新しい名前が既に存在しないか確認 */
    if (find_file_index(cwd, dst) != -1) {
        printf("mv: '%s' already exists\n", dst);
        return;
    }
    
    /* ディレクトリ名と重複していないか確認 */
    if (find_subdir_index(cwd, dst) != -1) {
        printf("mv: name '%s' already used by directory\n", dst);
        return;
    }
    
    /* ファイル名を変更 */
    strncpy(cwd->files[idx].name, dst, NAME_LEN - 1);
    cwd->files[idx].name[NAME_LEN - 1] = '\0';
    printf("file '%s' renamed to '%s'\n", src, dst);
}

/* mkdir: サブディレクトリ作成 */
static void mkdir_cmd(struct Dir *cwd, const char *name) {
    /* 引数チェック: ユーザーがディレクトリ名を指定したか */
    if (!name) { puts("usage: mkdir <name>"); return; }
    
    /* 上限チェック: ディレクトリ数の上限に達していないか */
    if (cwd->subdir_count >= MAX_SUBDIRS) { puts("subdir limit reached"); return; }
    
    /* 重複チェック: 同じ名のディレクトリやファイルが既にあるか */
    if (find_subdir_index(cwd, name) != -1 || find_file_index(cwd, name) != -1) {
        printf("name '%s' already exists\n", name);
        return;
    }
    
    /* 新見ディレクトリノードを初期化 */
    struct Dir *d = create_dir_node(name, cwd);
    /* メモリ確保失敗チェック: mallocが失敗した場合 */
    if (!d) { puts("mkdir: memory error"); return; }
    
    /* 新見ノードを現在ディレクトリの配列に追加 */
    cwd->subdirs[cwd->subdir_count++] = d;
    printf("directory '%s' created\n", name);
}

/* chmod: ファイルのパーミッション変更 */
static void chmod_cmd(struct Dir *cwd, const char *mode, const char *filename) {
    /* 引数チェック: ユーザーがモードとファイル名を両方指定したか */
    if (!mode || !filename) {
        puts("usage: chmod <mode> <filename>");
        return;
    }
    
    /* ファイル存在確認: 対象ファイルを検索 */
    int idx = find_file_index(cwd, filename);
    if (idx < 0) {
        printf("chmod: '%s' not found\n", filename);
        return;
    }
    
    /* パーミッション変更: perm フィールドを新しいモードで上書き */
    strncpy(cwd->files[idx].perm, mode, sizeof(cwd->files[idx].perm) - 1);
    cwd->files[idx].perm[sizeof(cwd->files[idx].perm) - 1] = '\0';
    printf("permissions of '%s' changed to '%s'\n", filename, mode);
}

/* cd: カレントディレクトリを移動
   特殊パス対応:
   - "/" でルートディレクトリに移動
   - ".." で親ディレクトリに移動
   - "." で現在ディレクトリのまま
   通常の場合はサブディレクトリ名を指定して移動
   戻り値: 移動後のディレクトリポインタ（移動失敗時は現在地のまま） */
static struct Dir *cd_cmd(struct Dir *cwd, const char *arg, struct Dir *root) {
    /* 引数チェック */
    if (!arg) { puts("usage: cd <dir>"); return cwd; }
    
    /* ルートディレクトリへの移動 */
    if (strcmp(arg, "/") == 0) return root;
    
    /* 親ディレクトリへの移動 */
    if (strcmp(arg, "..") == 0) {
        if (cwd->parent) return cwd->parent;
        return cwd;  /* ルートにいる場合は動かない */
    }
    
    /* 現在ディレクトリのまま（何もしない） */
    if (strcmp(arg, ".") == 0) return cwd;
    
    /* 指定されたサブディレクトリに移動 */
    int idx = find_subdir_index(cwd, arg);
    if (idx >= 0) {
        return cwd->subdirs[idx];
    } else {
        printf("cd: no such directory: %s\n", arg);
        return cwd;  /* 見つからない場合は現在地のまま */
    }
}

/* show_commands_jp: 日本語でサポートされているコマンド一覧を表示
   ユーザーが "コマンドリスト" と入力した場合に呼び出される */
static void show_commands_jp(void) {
    puts("使用できるコマンド:");
    puts(" touch <name>    - ファイル作成");
    puts(" ls [-l]         - 一覧表示");
    puts(" rm <name>       - ファイル削除");
    puts(" mv <old> <new>  - ファイルをリネーム");
    puts(" chmod <mode> <file> - パーミッション変更");
    puts(" mkdir <name>    - ディレクトリ作成");
    puts(" cd <dir>        - ディレクトリ移動 (/, .. 対応)");
    puts(" pwd / pwt       - 現在地表示");
    puts(" exit            - 終了");
}


/* cleanup: 再帰的にディレクトリツリーを解放（メモリリーク防止） */
static void free_dir_recursive(struct Dir *d) {
    if (!d) return;
    /* 子ディレクトリを再帰的に解放（後順走査） */
    for (int i = 0; i < d->subdir_count; i++) free_dir_recursive(d->subdirs[i]);
    free(d);  /* ノード自身を解放 */
}

int main(void) {
    /* ========== 初期化 ========== */
    struct Dir *root = create_dir_node("/", NULL);  /* ルートディレクトリ作成 */
    struct Dir *cwd = root;                         /* カレントディレクトリをルートに初期化 */

    char line[LINE_LEN];  /* 入力コマンド格納用配列 */

    /* ========== メインループ（コマンド処理） ========== */
    while (1) {
        if (cwd == root) printf("pseudo-linux:/> ");
        else printf("pseudo-linux:%s/> ", cwd->name);

        if (!fgets(line, sizeof(line), stdin)) { //fgetsで入力した文字をline[LINE_LEN]に格納
            puts("\nexit");
            break;
        }
        trim_newline(line);
        /* ユーザーが何も入力せずエンターを押した場合をスキップ */
        if (line[0] == '\0') continue;  /* 空行の場合は再度入力待ちに */

        /* 日本語入力: "コマンドリスト" または "コマンド リスト" で一覧表示 */
        if (strcmp(line, "コマンドリスト") == 0 || strcmp(line, "コマンド リスト") == 0) {
            show_commands_jp();
            continue;
        }

        /* コマンドと引数をスペース/タブで分割 */
        char *cmd = strtok(line, " \t");  /* コマンド部分 */
        char *arg = strtok(NULL, " \t");  /* 引数部分 */
        if (!cmd) continue;  /* コマンドが空の場合はスキップ */

        /* コマンド分岐処理 */
        if (strcmp(cmd, "exit") == 0) break;
        else if (strcmp(cmd, "touch") == 0) touch_cmd(cwd, arg);
        else if (strcmp(cmd, "ls") == 0) ls_cmd(cwd, arg);
        else if (strcmp(cmd, "rm") == 0) rm_cmd(cwd, arg);
        else if (strcmp(cmd, "mv") == 0) {
            /* mv コマンド: 2 つの引数を取得 */
            char *src = arg;                    /* 1 番目の引数（元の名前） */
            char *dst = strtok(NULL, " \t");    /* 2 番目の引数（新しい名前） */
            mv_cmd(cwd, src, dst);
        }
        else if (strcmp(cmd, "chmod") == 0) {
            /* chmod コマンド: 2 つの引数を取得 */
            char *mode = arg;                   /* 1 番目の引数（パーミッション） */
            char *filename = strtok(NULL, " \t"); /* 2 番目の引数（ファイル名） */
            chmod_cmd(cwd, mode, filename);
        }
        else if (strcmp(cmd, "mkdir") == 0) mkdir_cmd(cwd, arg);
        else if (strcmp(cmd, "cd") == 0) cwd = cd_cmd(cwd, arg, root);  /* cwdを更新 */
        else if (strcmp(cmd, "pwd") == 0 || strcmp(cmd, "pwt") == 0) pwd_cmd(cwd);
        else {
            puts("command not found");
        }
    }

    /* ========== クリーンアップ ========== */
    free_dir_recursive(root);  /* ツリー全体をメモリ解放 */
    return 0;
}
