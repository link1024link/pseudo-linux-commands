# Linux コマンドシミュレータ

> メモリ上に仮想ファイルシステムを構築し、Linuxコマンドの内部処理を体験できる学習用プログラム

---

## プロジェクトの背景

**「Linuxコマンドを使える」だけでなく、「内部でどう処理されているか」を理解したい**

そんな思いから、実際のファイルシステムを使わず、メモリ上にツリー構造を実装しました。

### 学んだこと
- ファイルシステムの階層構造の仕組み
- ポインタを使ったツリー構造の実装
- メモリ管理の重要性（malloc/free）
- エラーハンドリングの設計思想

---

## 特徴

### データ構造の工夫
```
root (/)
├─ file1.txt
├─ subdir1/
│  ├─ file2.txt
│  └─ nested_dir/
└─ subdir2/
   └─ file3.txt
```

- **ツリー構造**: 親子関係を持つディレクトリ階層
- **動的メモリ管理**: `malloc`/`free`による効率的なメモリ使用
- **配列ベースの管理**: 固定長配列でファイル/ディレクトリを保持

### 安全性の追求

- すべてのポインタ操作前にNULLチェック
- 配列の境界チェックで範囲外アクセスを防止
- 重複チェックで意図しない上書きを回避
- プログラム終了時に再帰的メモリ解放

---

## 実装コマンド

| コマンド | 機能 | 実装の工夫 |
|---------|------|-----------|
| `touch <name>` | ファイル作成 | 重複・上限チェックで安全に作成 |
| `ls [-l]` | 一覧表示 | `-l`でパーミッション・サイズ表示 |
| `rm <name>` | ファイル削除 | 配列を詰めて効率的に削除 |
| `mv <old> <new>` | リネーム | 同一ディレクトリ内で名前変更 |
| `mkdir <name>` | ディレクトリ作成 | 新規ノードを動的生成 |
| `cd <dir>` | ディレクトリ移動 | `/`, `..`, `.` の特殊パス対応 |
| `pwd` / `pwt` | 現在地表示 | 親ポインタを逆順トラバース |
| `exit` | 終了 | メモリ解放してクリーンに終了 |

> **mvコマンドについて**: 現在はリネームのみ対応。ディレクトリ間移動は将来拡張として設計

---

## ビルドと実行

### macOS / Linux
```bash
gcc -Wall -Wextra linux-commands.c -o linux_sim
./linux_sim
```

### Windows (MinGW)
```bash
gcc -Wall -Wextra linux-commands.c -o linux_sim.exe
linux_sim.exe
```

### Windows (MSVC)
```bash
cl /W4 linux-commands.c /Fe:linux_sim.exe
linux_sim.exe
```

> 標準ライブラリのみ使用。外部依存なし。

---

## 使用例

```bash
pseudo-linux:/> mkdir documents
directory 'documents' created

pseudo-linux:/> cd documents
pseudo-linux:documents/> touch report.txt
file 'report.txt' created

pseudo-linux:documents/> ls -l
-rw- 0 report.txt

pseudo-linux:documents/> pwd
/documents

pseudo-linux:documents/> cd ..
pseudo-linux:/> ls
documents/

pseudo-linux:/> exit
```

---

## 設計の制約（意図的な簡略化）

| 項目 | 実装状況 | 理由 |
|-----|---------|------|
| ファイル内容の読み書き | 未実装 | 基本構造の理解を優先 |
| パーミッション変更 | 未実装 | 権限管理の複雑さを避けた |
| ディレクトリ削除 | 未実装 | 再帰削除の複雑さを避けた |
| ディレクトリ間のファイル移動 | 未実装 | パス解決の複雑さを避けた |
| ワイルドカード | 未実装 | パターンマッチは範囲外 |
| ファイル数上限 | 16個/dir | 配列ベースの設計による制限 |

> **設計判断**: 完全再現ではなく、**ファイルシステムの仕組みを学ぶこと**を最優先

---

## 技術的な実装ポイント

### 1. ツリー構造の実装

```c
struct Dir {
    char name[NAME_LEN];
    struct Dir *parent;              // 親へのポインタ
    struct File files[MAX_FILES];    // ファイル配列
    struct Dir *subdirs[MAX_SUBDIRS]; // サブディレクトリ配列
    int file_count, subdir_count;
};
```

**ポイント**: 親ポインタを持つことで、`cd ..` や `pwd` が実装できる

---

### 2. パス解決のロジック

```c
// 絶対パス (/)
if (strcmp(arg, "/") == 0) return root;

// 親ディレクトリ (..)
if (strcmp(arg, "..") == 0) return cwd->parent ? cwd->parent : cwd;

// 現在ディレクトリ (.)
if (strcmp(arg, ".") == 0) return cwd;
```

**ポイント**: ルートの親はNULLなので、NULLチェックが必須

---

### 3. メモリリークの防止

```c
static void free_dir(struct Dir *d) {
    if (!d) return;
    
    // 子ディレクトリを再帰的に解放
    for (int i = 0; i < d->subdir_count; i++) {
        free_dir(d->subdirs[i]);
    }
    
    // ノード自身を解放
    free(d);
}
```

**ポイント**: 後順走査（子→親の順）で解放

---

### 4. 防御的プログラミング

```c
// 引数チェック
if (!name) {
    puts("usage: touch <name>");
    return;
}

// 重複チェック
if (find_file_index(cwd, name) != -1) {
    puts("name already exists");
    return;
}

// 上限チェック
if (cwd->file_count >= MAX_FILES) {
    puts("file limit reached");
    return;
}
```

**ポイント**: 早期リターンで条件を階層化せず、読みやすく保つ

---

## 工夫した点

### コードの可読性
- 簡潔なコメント
- セクション区切りで構造を明確化
- 関数ごとに目的を明記

### エラーハンドリング
- NULLチェック
- 境界チェック
- 重複チェック

### ユーザビリティ
- 分かりやすいエラーメッセージ
- プロンプトで現在地を表示

---

## 今後の拡張案

- [ ] ファイル内容の読み書き (`cat`, `echo >`)
- [ ] パーミッション変更 (`chmod`) の実装
- [ ] ディレクトリ削除 (`rmdir`, `rm -r`)
- [ ] ディレクトリ間のファイル移動（完全な `mv`）
- [ ] 絶対パス指定の `cd` 対応
- [ ] 動的配列によるファイル数上限の撤廃
- [ ] `help` コマンドの追加

---

## このプロジェクトで得たスキル

### 技術面
- C言語によるデータ構造の実装
- メモリ管理の実践的な理解
- エラーハンドリングの設計力
- ポインタ操作の習熟

### 思考面
- 「何を実装し、何を省くか」の判断力
- 制約の中で本質を抽出する力
- コードの可読性と保守性のバランス
- 学習効果を最大化する設計思想

---

## 関連リンク

- [ソースコード (linux-commands.c)](linux-commands.c)
