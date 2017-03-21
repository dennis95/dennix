/* Copyright (c) 2016, 2017 Dennis Wölfing
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* utils/snake.
 * The game Snake.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define HEIGHT 25
#define WIDTH 80

enum Direction {
    UP,
    LEFT,
    DOWN,
    RIGHT,
    NONE
};

struct SnakeSegment {
    int row;
    int col;
    enum Direction direction;
    struct SnakeSegment* next;
};

struct Food {
    int row;
    int col;
};

struct SnakeSegment* snakeHead;
struct SnakeSegment* snakeTail;
struct Food food;
struct termios oldTermios;
unsigned int score;

static bool checkCollision(void);
static void checkFood(void);
static void drawScreen(void);
static void handleInput(void);
static void initializeWorld(void);
static void move(struct SnakeSegment* snake);
static void restoreTermios(void);

int main(int argc, char* argv[]) {
    if (argc >= 2) {
        unsigned int seed = strtoul(argv[1], NULL, 10);
        srand(seed);
    } else {
#ifndef __dennix__
        srand(time(NULL));
#else
        srand(2);
#endif
    }

#ifndef __dennix__
    setbuf(stdout, NULL);
#endif

    // Set terminal attributes.
    tcgetattr(0, &oldTermios);
    atexit(restoreTermios);
    struct termios new_termios = oldTermios;
    new_termios.c_lflag &= ~(ECHO | ICANON);
    new_termios.c_cc[VMIN] = 0;
    tcsetattr(0, TCSAFLUSH ,&new_termios);

    initializeWorld();

    while (true) {
        // Game loop
        drawScreen();
        struct timespec sleepTime;
        sleepTime.tv_sec = 0;
        sleepTime.tv_nsec = 175000000;
        nanosleep(&sleepTime, NULL);
        handleInput();
        move(snakeHead);

        if (checkCollision()) {
            printf("\e[2JGame Over. Your score is: %u\n", score);
            return 0;
        }
    }
}

static bool checkCollision(void) {
    struct SnakeSegment* current = snakeHead;
    while (current) {
        if (current->row < 0 || current->row >= HEIGHT ||
                current->col < 0 || current->col >= WIDTH) {
            return true;
        }

        struct SnakeSegment* other = current->next;
        while (other) {
            if (current->row == other->row && current->col == other->col) {
                return true;
            }
            other = other->next;
        }
        current = current->next;
    }
    return false;
}

static void checkFood(void) {
    if (food.row == snakeHead->row && food.col == snakeHead->col) {
        struct SnakeSegment* newSegment = malloc(sizeof(struct SnakeSegment));
        newSegment->row = snakeTail->row;
        newSegment->col = snakeTail->col;
        // Set the direction to NONE so that the new segment will not move in
        // the current frame.
        newSegment->direction = NONE;
        newSegment->next = NULL;

        snakeTail->next = newSegment;
        snakeTail = newSegment;

        score++;

        // Create some new food at a random location.
        food.row = rand() % HEIGHT;
        food.col = rand() % WIDTH;
    }
}

static void drawScreen(void) {
    printf("\e[2J");

    struct SnakeSegment* current = snakeHead;
    while (current) {
        if (current->row >= 0 && current->row < HEIGHT &&
                current->col >= 0 && current->col < WIDTH) {
            printf("\e[%d;%dH0", current->row + 1, current->col + 1);
        }
        current = current->next;
    }

    printf("\e[%d;%dHX", food.row + 1, food.col + 1);
    printf("\e[H");
}

static void handleInput(void) {
    char key;
    if (read(0, &key, 1)) {
        if (key == 'q') {
            printf("\e[2J");
            exit(0);
        }
        enum Direction newDirection = snakeHead->direction;
        switch (key) {
        case 'w': case 'W': newDirection = UP; break;
        case 'a': case 'A': newDirection = LEFT; break;
        case 's': case 'S': newDirection = DOWN; break;
        case 'd': case 'D': newDirection = RIGHT; break;
        }

        // Don't allow the player to make a 180° turn.
        if ((snakeHead->direction + 2) % 4 != newDirection) {
            snakeHead->direction = newDirection;
        }
    }
}

static void initializeWorld(void) {
    // Create a snake with 6 segments.
    snakeHead = malloc(sizeof(struct SnakeSegment));
    snakeHead->row = 20;
    snakeHead->col = 10;
    snakeHead->direction = UP;

    snakeTail = snakeHead;
    for (int i = 0; i < 5; i++) {
        struct SnakeSegment* next = malloc(sizeof(struct SnakeSegment));
        snakeTail->next = next;

        next->row = 20;
        next->col = 11 + i;
        next->direction = LEFT;
        next->next = NULL;
        snakeTail = next;
    }

    food.row = rand() % HEIGHT;
    food.col = rand() % WIDTH;
}

static void move(struct SnakeSegment* snake) {
    switch (snake->direction) {
    case UP: snake->row--; break;
    case LEFT: snake->col--; break;
    case DOWN: snake->row++; break;
    case RIGHT: snake->col++; break;
    case NONE: break;
    }

    if (snake == snakeHead) {
        // Check for food before the other segments have moved so the old
        // position of the tail is still known.
        checkFood();
    }

    if (snake->next) {
        move(snake->next);
        snake->next->direction = snake->direction;
    }
}

static void restoreTermios(void) {
    tcsetattr(0, TCSAFLUSH, &oldTermios);
}
