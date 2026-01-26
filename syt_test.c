
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#endif

#define SCREEN_WIDTH 60
#define SCREEN_HEIGHT 20
#define MAX_BULLETS 10
#define MAX_ENEMIES 3
#define MAX_ENEMY_BULLETS 10
#define FPS 10

typedef struct {
    int x, y;
    int is_active;
    char symbol;
} GameObject;

typedef struct {
    int x, y;
    int is_active;
    char symbol;
    int hp;
    int type; // 0=����, 1=����, 2=�ϋv
} Enemy;

char screen_buffer[SCREEN_HEIGHT][SCREEN_WIDTH + 1];
GameObject player;
GameObject bullets[MAX_BULLETS];
GameObject enemyBullets[MAX_ENEMY_BULLETS];
Enemy enemies[MAX_ENEMIES];
int score = 0;

void initialize_game();
void handle_input();
void update_game();
void update_enemies();
void enemy_fire();
void update_enemy_bullets();
void check_collision();
void check_player_hit();
void render();
void game_loop();

/* Platform abstraction */
void init_platform();
void restore_platform();
int kbhit_portable();
int getch_portable();
void msleep_portable(int ms);

void initialize_game() {
    srand((unsigned int)time(NULL));

    player.x = SCREEN_WIDTH / 2;
    player.y = SCREEN_HEIGHT - 2;
    player.is_active = 1;
    player.symbol = '^';

    for (int i = 0; i < MAX_BULLETS; i++) {
        bullets[i].is_active = 0;
        bullets[i].symbol = '|';
    }

    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
        enemyBullets[i].is_active = 0;
        enemyBullets[i].symbol = '!';
    }

    for (int i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].x = rand() % SCREEN_WIDTH;
        enemies[i].y = 2;
        enemies[i].is_active = 1;
        enemies[i].type = rand() % 3;
        switch (enemies[i].type) {
        case 0: enemies[i].symbol = 'V'; enemies[i].hp = 1; break;
        case 1: enemies[i].symbol = 'W'; enemies[i].hp = 1; break;
        case 2: enemies[i].symbol = 'X'; enemies[i].hp = 2; break;
        }
    }

    score = 0;
}

/* --- platform-specific implementations --- */

#ifdef _WIN32
static DWORD _old_console_mode = 0;
void init_platform() {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode;
    if (GetConsoleMode(h, &mode)) {
        _old_console_mode = mode;
        SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
}
void restore_platform() {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (_old_console_mode) SetConsoleMode(h, _old_console_mode);
}
int kbhit_portable() { return _kbhit(); }
int getch_portable() { return _getch(); }
void msleep_portable(int ms) { Sleep(ms); }
#else
static struct termios _orig_term;
static int _orig_flags;
void init_platform() {
    tcgetattr(STDIN_FILENO, &_orig_term);
    struct termios raw = _orig_term;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    _orig_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, _orig_flags | O_NONBLOCK);
}
void restore_platform() {
    tcsetattr(STDIN_FILENO, TCSANOW, &_orig_term);
    fcntl(STDIN_FILENO, F_SETFL, _orig_flags);
}
int kbhit_portable() {
    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}
int getch_portable() {
    unsigned char c = 0;
    if (read(STDIN_FILENO, &c, 1) <= 0) return 0;
    return c;
}
void msleep_portable(int ms) { usleep(ms * 1000); }
#endif


void handle_input() {
    if (kbhit_portable()) {
        char key = (char)getch_portable();
        switch (key) {
        case 'a':
        case 'A':
            if (player.x > 0) player.x--;
            break;
        case 'd':
        case 'D':
            if (player.x < SCREEN_WIDTH - 1) player.x++;
            break;
        case ' ':
            for (int i = 0; i < MAX_BULLETS; i++) {
                if (!bullets[i].is_active) {
                    bullets[i].x = player.x;
                    bullets[i].y = player.y - 1;
                    bullets[i].is_active = 1;
                    break;
                }
            }
            break;
        case 'q':
        case 'Q':
            exit(0);
        }
    }
}

void update_game() {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].is_active) continue;

        bullets[i].y--;
        if (bullets[i].y < 0) {
            bullets[i].is_active = 0;
            continue;
        }

        /* 移動後に敵との当たり判定を即時チェック（フレーム内での見逃しを減らす） */
        for (int k = 0; k < MAX_ENEMIES; k++) {
            if (!enemies[k].is_active) continue;
            if (bullets[i].x == enemies[k].x && bullets[i].y == enemies[k].y) {
                bullets[i].is_active = 0;
                enemies[k].hp--;
                if (enemies[k].hp <= 0) {
                    int points = (enemies[k].type == 0) ? 100 :
                                 (enemies[k].type == 1) ? 150 : 200;
                    score += points;
                    /* 敵を再生成（安全な範囲で座標を決定） */
                    enemies[k].x = rand() % SCREEN_WIDTH;
                    enemies[k].y = 2;
                    enemies[k].type = rand() % 3;
                    switch (enemies[k].type) {
                    case 0: enemies[k].symbol = 'V'; enemies[k].hp = 1; break;
                    case 1: enemies[k].symbol = 'W'; enemies[k].hp = 1; break;
                    case 2: enemies[k].symbol = 'X'; enemies[k].hp = 2; break;
                    }
                }
                break;
            }
        }
    }
}


void update_enemies() {
    static int direction[MAX_ENEMIES] = { 1, 1, 1 };
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].is_active) {
            int speed = (enemies[i].type == 1) ? 2 : 1;
            enemies[i].x += direction[i] * speed;
            if (enemies[i].x >= SCREEN_WIDTH - 1 || enemies[i].x <= 0) {
                direction[i] *= -1;
            }
        }
    }
}

void enemy_fire() {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].is_active && rand() % 20 == 0) {
            for (int j = 0; j < MAX_ENEMY_BULLETS; j++) {
                if (!enemyBullets[j].is_active) {
                    enemyBullets[j].x = enemies[i].x;
                    enemyBullets[j].y = enemies[i].y + 1;
                    enemyBullets[j].is_active = 1;
                    break;
                }
            }
        }
    }
}

void update_enemy_bullets() {
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
        if (enemyBullets[i].is_active) {
            enemyBullets[i].y++;
            if (enemyBullets[i].y >= SCREEN_HEIGHT) {
                enemyBullets[i].is_active = 0;
            }
        }
    }
}

void check_collision() {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].is_active) continue;
        for (int j = 0; j < MAX_BULLETS; j++) {
            if (bullets[j].is_active &&
                bullets[j].x == enemies[i].x &&
                bullets[j].y == enemies[i].y) {
                bullets[j].is_active = 0;
                enemies[i].hp--;
                if (enemies[i].hp <= 0) {
                    int points = (enemies[i].type == 0) ? 100 :
                        (enemies[i].type == 1) ? 150 : 200;
                    score += points;
                    // �ďo��
                    enemies[i].x = rand() % (SCREEN_WIDTH - 2);
                    enemies[i].y = 2;
                    enemies[i].type = rand() % 3;
                    switch (enemies[i].type) {
                    case 0: enemies[i].symbol = 'V'; enemies[i].hp = 1; break;
                    case 1: enemies[i].symbol = 'W'; enemies[i].hp = 1; break;
                    case 2: enemies[i].symbol = 'X'; enemies[i].hp = 2; break;
                    }
                }
            }
        }
    }
}

void check_player_hit() {
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
        if (enemyBullets[i].is_active &&
            enemyBullets[i].x == player.x &&
            enemyBullets[i].y == player.y) {
            printf("\n>>> GAME OVER <<<\n");
            exit(0);
        }
    }
}

void render() {
    for (int i = 0; i < SCREEN_HEIGHT; i++) {
        memset(screen_buffer[i], ' ', SCREEN_WIDTH);
        screen_buffer[i][SCREEN_WIDTH] = '\0';
    }

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].is_active) {
            if (enemies[i].y >= 0 && enemies[i].y < SCREEN_HEIGHT &&
                enemies[i].x >= 0 && enemies[i].x < SCREEN_WIDTH) {
                screen_buffer[enemies[i].y][enemies[i].x] = enemies[i].symbol;
            }
        }
    }

    if (player.is_active) {
        if (player.y >= 0 && player.y < SCREEN_HEIGHT &&
            player.x >= 0 && player.x < SCREEN_WIDTH) {
            screen_buffer[player.y][player.x] = player.symbol;
        }
    }

    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].is_active) {
            if (bullets[i].y >= 0 && bullets[i].y < SCREEN_HEIGHT &&
                bullets[i].x >= 0 && bullets[i].x < SCREEN_WIDTH) {
                screen_buffer[bullets[i].y][bullets[i].x] = bullets[i].symbol;
            } else {
                bullets[i].is_active = 0;
            }
        }
    }

    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
        if (enemyBullets[i].is_active) {
            if (enemyBullets[i].y >= 0 && enemyBullets[i].y < SCREEN_HEIGHT &&
                enemyBullets[i].x >= 0 && enemyBullets[i].x < SCREEN_WIDTH) {
                screen_buffer[enemyBullets[i].y][enemyBullets[i].x] = enemyBullets[i].symbol;
            } else {
                enemyBullets[i].is_active = 0;
            }
        }
    }

    printf("\033[H\033[J");
    for (int i = 0; i < SCREEN_WIDTH; i++) printf("=");
    printf("\nScore: %d\n", score);

    for (int i = 0; i < SCREEN_HEIGHT; i++) {
        printf("|%s|\n", screen_buffer[i]);
    }

    for (int i = 0; i < SCREEN_WIDTH; i++) printf("=");
    printf("\n");
}

void game_loop() {
    while (1) {
        handle_input();
        update_game();
        update_enemies();
        enemy_fire();
        update_enemy_bullets();
        check_collision();
        check_player_hit();
        render();
        msleep_portable(1000 / FPS);
    }
}

int main() {
    init_platform();
    atexit(restore_platform);
    initialize_game();
    game_loop();
    return 0;
}

