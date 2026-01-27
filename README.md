# Linux コマンドシミュレータ

## 概要
メモリ上に仮想ファイルシステムを構築し、基本的なLinuxコマンドを体験できる簡易シミュレータです。

## 特徴
- ツリー構造によるディレクトリ階層の実装
- 基本的なファイル操作コマンド（touch, ls, rm, mv, chmod）
- ディレクトリ操作（mkdir, cd, pwd）
- 日本語コマンドリスト表示

## 実装コマンド
- `touch <name>` - ファイル作成
- `ls [-l]` - 一覧表示（-lでロング形式）
- `rm <name>` - ファイル削除
- `mv <old> <new>` - ファイルリネーム
- `chmod <mode> <file>` - パーミッション変更
- `mkdir <name>` - ディレクトリ作成
- `cd <dir>` - ディレクトリ移動（/, .., . 対応）
- `pwd` / `pwt` - 現在地表示
- `exit` - 終了

## ビルドと実行

### macOS/Linux
```bash
gcc -Wall -Wextra L2.c -o linux_sim
./linux_sim
```

### Windows (MinGW)
```bash
gcc -Wall -Wextra L2.c -o linux_sim.exe
linux_sim.exe
```

### Windows (MSVC)
```bash
cl /W4 L2.c /Fe:linux_sim.exe
linux_sim.exe
```

## 使用例
```bash
pseudo-linux:/> mkdir documents
directory 'documents' created
pseudo-linux:/> cd documents
pseudo-linux:documents/> touch file.txt
file 'file.txt' created
pseudo-linux:documents/> ls -l
-rw- 0 file.txt
pseudo-linux:documents/> pwd
/documents
```
