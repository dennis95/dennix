/* Copyright (c) 2020 Dennis Wölfing
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

/* utils/bricks.c
 * Bricks game.
 */

#include <devctl.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <poll.h>
#include <sched.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <dennix/display.h>
#include <dennix/mouse.h>

#define min(x, y) ((x < y) ? (x) : (y))
#define max(x, y) ((x > y) ? (x) : (y))

static const uint32_t
brick100[] = { RGB(163, 163, 163), RGB(190, 190, 190), RGB(127, 127, 127) },
brick500[] = { RGB(200, 100, 0), RGB(250, 150, 50), RGB(200, 30, 50) },
lifeBrick[] = { RGB(220, 220, 220), RGB(255, 255, 255), RGB(150, 150, 150) },
normalBrick[] = { RGB(127, 0, 50), RGB(150, 20, 90), RGB(100, 30, 80) },
threeBrick[] = { RGB(128, 0, 128), RGB(128, 50, 128), RGB(100, 0, 100) },
threeBrick2[] = { RGB(180, 60, 180), RGB(180, 100, 180), RGB(128, 0, 128) },
threeBrick3[] = { RGB(200, 100, 200), RGB(230, 150, 230), RGB(150, 30, 150) },
undestroyableBrick[] = { RGB(25, 25, 25), RGB(35, 35, 25), RGB(10, 10, 20) };
static const uint32_t paddleColor = RGB(127, 127, 0);
static const uint32_t bgColor = RGB(0, 0, 60);
static const uint32_t ballColor = RGB(255, 0, 0);

static const double ballSpeed = 0.00000002;
static const double brickHeight = 5.0;
static const double brickWidth = 10.0;
static const double mousePaddleSpeed = 0.08;
static const double paddleLength = 5.0;
static const double paddleSpeed = 0.9;
static const double paddleY = 105.0;
static const double pickupSpeed = 0.00000003;

static int displayFd;
static struct display_draw draw;
static int mouseFd;
static int oldMode;
static struct termios oldTermios;
static struct display_resolution res;

static const size_t levelWidth = 11;
static char level[] =
"=5===1===5="
"+=#11111#=+"
"====#5#===="
" X1=   =1X "
"  X     X  ";

static size_t bricksLeft;
static bool gameRunning = true;
static struct Pickup* pickups;
static bool preparing = true;

static unsigned brickMargin;
static unsigned lifes = 3;
static unsigned pixelsPerBrickX;
static unsigned pixelsPerBrickY;
static unsigned pixelPlayArea;
static unsigned score;
static unsigned xoff, yoff;

static double pixelsPerUnit;
static double ballX = 55.0;
static double ballY = 80.0;
static double ballAngle;
static double paddlePos = 55.0;

static uint32_t textLfb[20 * 9 * 16];
static uint32_t* lfb;
static char vgafont[4096];

struct Pickup {
    double x;
    double y;
    char type;
    struct Pickup* prev;
    struct Pickup* next;
};

static void addPickup(double x, double y, char type);
static void drawBall(double oldPosX, double oldPosY, double newPosX,
        double newPosY);
static void drawBrick(char type, unsigned brickX, unsigned brickY);
static void drawLevel(void);
static void handleInput(void);
static void printText(const char* text, unsigned x, unsigned y,
        uint32_t textColor, uint32_t backColor);
static void redraw(unsigned x, unsigned y, unsigned width, unsigned height);
static void setup(void);
static void setupLevel(void);
static void update(double nanoseconds);
static void updateBall(double nanoseconds);
static void updatePaddle(double diff);
static void updatePickup(struct Pickup* pickup, double nanoseconds);
static void updatePickups(double nanoseconds);

static void restoreDisplayAndTerminal(void) {
    tcsetattr(0, TCSAFLUSH, &oldTermios);
    posix_devctl(displayFd, DISPLAY_SET_MODE, &oldMode, sizeof(oldMode), NULL);
}

static void onSignal(int signo) {
    restoreDisplayAndTerminal();
    signal(signo, SIG_DFL);
    raise(signo);
}

int main(void) {
    setup();
    setupLevel();
    drawLevel();

    struct timespec oldTs;
    clock_gettime(CLOCK_MONOTONIC, &oldTs);
    while (true) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        handleInput();
        if (ts.tv_sec == oldTs.tv_sec && ts.tv_nsec == oldTs.tv_nsec) {
            sched_yield();
            continue;
        }

        double nanoseconds = (ts.tv_sec - oldTs.tv_sec) * 1000000000.0
                + (ts.tv_nsec - oldTs.tv_nsec);
        if (gameRunning) update(nanoseconds);
        oldTs = ts;
    }
}

static void addPickup(double x, double y, char type) {
    struct Pickup* pickup = malloc(sizeof(struct Pickup));
    if (!pickup) err(1, "malloc");
    pickup->x = x;
    pickup->y = y;
    pickup->type = type;
    pickup->prev = NULL;
    pickup->next = pickups;
    if (pickup->next) {
        pickup->next->prev = pickup;
    }
    pickups = pickup;
}

static void drawBall(double oldPosX, double oldPosY, double newPosX,
        double newPosY) {
    double unitsPerPixel = 1 / pixelsPerUnit;

    unsigned oldX = round(oldPosX * pixelsPerUnit);
    unsigned oldY = round(oldPosY * pixelsPerUnit);
    unsigned newX = round(newPosX * pixelsPerUnit);
    unsigned newY = round(newPosY * pixelsPerUnit);

    for (unsigned i = 0; i < pixelsPerUnit; i++) {
        double y = i * unitsPerPixel + unitsPerPixel / 2;
        if (y > 1.0) y = 1.0;
        double x = sqrt(1 - y * y);

        int width = round(x * pixelsPerUnit);
        for (int j = -width; j < width; j++) {
            lfb[(yoff + oldY - i) * res.width + xoff + oldX + j] = bgColor;
            lfb[(yoff + oldY + i) * res.width + xoff + oldX + j] = bgColor;
        }
    }

    for (unsigned i = 0; i < pixelsPerUnit; i++) {
        double y = i * unitsPerPixel + unitsPerPixel / 2;
        if (y > 1.0) y = 1.0;
        double x = sqrt(1 - y * y);

        int width = round(x * pixelsPerUnit);
        for (int j = -width; j < width; j++) {
            lfb[(yoff + newY - i) * res.width + xoff + newX + j] = ballColor;
            lfb[(yoff + newY + i) * res.width + xoff + newX + j] = ballColor;
        }
    }

    unsigned minX = min(oldX, newX) - round(pixelsPerUnit);
    unsigned minY = min(oldY, newY) - round(pixelsPerUnit);
    unsigned maxX = max(oldX, newX) + round(pixelsPerUnit);
    unsigned maxY = max(oldY, newY) + round(pixelsPerUnit);
    redraw(xoff + minX, yoff + minY, maxX - minX, maxY - minY);
}

static void drawBrick(char type, unsigned brickX, unsigned brickY) {
    const uint32_t background[] = { bgColor, bgColor, bgColor };
    const uint32_t* color = type == '=' ? normalBrick : type == '1' ? brick100 :
            type == '5' ? brick500 : type == '+' ? lifeBrick :
            type == '#' ? threeBrick : type == ':' ? threeBrick2 :
            type == '.' ? threeBrick3 : type == 'X' ? undestroyableBrick :
            background;

    unsigned xPixel = xoff + brickX * pixelsPerBrickX;
    unsigned yPixel = yoff + brickY * pixelsPerBrickY;
    for (unsigned x = 1; x < pixelsPerBrickX - 1; x++) {
        for (unsigned y = 1; y < pixelsPerBrickY - 1; y++) {
            uint32_t pixelColor = color[0];
            if ((x <= brickMargin && y < pixelsPerBrickY - x) ||
                    (y <= brickMargin && x < pixelsPerBrickX - y)) {
                pixelColor = color[1];
            } else if (x >= pixelsPerBrickX - 1 - brickMargin ||
                    y >= pixelsPerBrickY - 1 - brickMargin) {
                pixelColor = color[2];
            }
            lfb[(y + yPixel) * res.width + x + xPixel] = pixelColor;
        }
    }
    redraw(xPixel, yPixel, pixelsPerBrickX, pixelsPerBrickY);
}

static void drawLevel(void) {
    for (unsigned x = xoff; x < xoff + pixelPlayArea; x++) {
        for (unsigned y = yoff; y < yoff + pixelPlayArea; y++) {
            lfb[y * res.width + x] = bgColor;
        }
    }

    size_t height = sizeof(level) / levelWidth;
    for (size_t y = 0; y < height; y++) {
        for (size_t x = 0; x < levelWidth; x++) {
            char c = level[y * levelWidth + x];
            if (c == ' ') continue;

            drawBrick(c, x, y);
        }
    }

    drawBall(ballX, ballY, ballX, ballY);
    updatePaddle(0.0);
    redraw(0, 0, res.width, res.height);
}

static void handleInput(void) {
    struct pollfd pfd[2];
    pfd[0].fd = 0;
    pfd[0].events = POLLIN;
    pfd[1].fd = mouseFd;
    pfd[1].events = POLLIN;

    while (poll(pfd, 2, 0) >= 1) {
        if (pfd[0].revents & POLLIN) {
            char key;
            read(0, &key, 1);
            if (key == 'q' || key == 'Q') {
                exit(0);
            } else if (key == 'a' || key == 'A') {
                updatePaddle(-paddleSpeed);
            } else if (key == 'd' || key == 'D') {
                updatePaddle(paddleSpeed);
            } else if (preparing && key == ' ') {
                ballAngle = atan2(55.0 - paddlePos, paddleY - ballY);
                preparing = false;
            }
        }

        if (pfd[1].revents & POLLIN) {
            struct mouse_data data;
            read(mouseFd, &data, sizeof(data));

            if (data.mouse_x) {
                updatePaddle(data.mouse_x * mousePaddleSpeed);
            }

            if (preparing && data.mouse_flags & MOUSE_LEFT) {
                ballAngle = atan2(55.0 - paddlePos, paddleY - ballY);
                preparing = false;
            }
        }
    }
}

static void printText(const char* text, unsigned x, unsigned y,
        uint32_t textColor, uint32_t backColor) {
    size_t length = strnlen(text, 20);
    for (size_t i = 0; i < length; i++) {
        const char* font = &vgafont[text[i] * 16];
        for (size_t j = 0; j < 16; j++) {
            for (size_t k = 0; k < 8; k++) {
                bool pixelFg = font[j] & (1 << (7 - k));
                uint32_t rgbColor = pixelFg ? textColor : backColor;
                textLfb[j * 20 * 9 + i * 9 + k] = rgbColor;
            }
            if (text[i + 1]) {
                textLfb[j * 20 * 9 + i * 9 + 8] = backColor;
            }
        }
    }

    struct display_draw textDraw;
    textDraw.lfb = textLfb;
    textDraw.lfb_x = x;
    textDraw.lfb_y = y;
    textDraw.lfb_pitch = 20 * 9 * 4;
    textDraw.draw_x = 0;
    textDraw.draw_y = 0;
    textDraw.draw_height = 16;
    textDraw.draw_width = length * 9 - 1;
    posix_devctl(displayFd, DISPLAY_DRAW, &textDraw, sizeof(textDraw), NULL);
}

static void redraw(unsigned x, unsigned y, unsigned width, unsigned height) {
    draw.draw_x = x;
    draw.draw_y = y;
    draw.draw_width = width;
    draw.draw_height = height;
    posix_devctl(displayFd, DISPLAY_DRAW, &draw, sizeof(draw), NULL);
}

static void setup(void) {
    FILE* fontFile = fopen("/share/fonts/vgafont", "r");
    size_t glyphsRead = fread(vgafont, 16, 255, fontFile);
    if (glyphsRead < 255) err(1, "Cannot read font");
    fclose(fontFile);

    displayFd = open("/dev/display", O_RDONLY);
    if (displayFd < 0) err(1, "Cannot open '/dev/display'");
    int mode = DISPLAY_MODE_QUERY;
    errno = posix_devctl(displayFd, DISPLAY_SET_MODE, &mode, sizeof(mode),
            &oldMode);
    if (errno) err(1, "Cannot get display mode");
    if (tcgetattr(0, &oldTermios) < 0) err(1, "tcgetattr");

    atexit(restoreDisplayAndTerminal);
    signal(SIGABRT, onSignal);
    signal(SIGBUS, onSignal);
    signal(SIGFPE, onSignal);
    signal(SIGILL, onSignal);
    signal(SIGINT, onSignal);
    signal(SIGQUIT, onSignal);
    signal(SIGSEGV, onSignal);
    signal(SIGTERM, onSignal);

    mode = DISPLAY_MODE_LFB;
    errno = posix_devctl(displayFd, DISPLAY_SET_MODE, &mode, sizeof(mode),
            NULL);
    if (errno) err(1, "Cannot set display mode");

    struct termios newTermios = oldTermios;
    newTermios.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(0, TCSAFLUSH, &newTermios);

    mouseFd = open("/dev/mouse", O_RDONLY);
    if (mouseFd >= 0) {
        // Discard any mouse data that has been buffered.
        struct pollfd pfd[1];
        pfd[0].fd = mouseFd;
        pfd[0].events = POLLIN;
        while (poll(pfd, 1, 0) == 1) {
            struct mouse_data data[256];
            read(mouseFd, data, sizeof(data));
        }
    }

    posix_devctl(displayFd, DISPLAY_GET_RESOLUTION, &res, sizeof(res), NULL);

    lfb = malloc(res.width * 4 * res.height);
    if (!lfb) err(1, "malloc");
    draw.lfb = lfb;
    draw.lfb_pitch = res.width * 4;

    for (size_t i = 0; i < res.width * res.height; i++) {
        lfb[i] = RGB(0, 0, 0);
    }
}

static void setupLevel(void) {
    unsigned possibleWidth = res.width - 120;
    unsigned possibleHeight = res.height - 30;

    double possiblePixelsPerUnit = 110.0 / min(possibleWidth, possibleHeight);
    pixelsPerBrickY = brickHeight / possiblePixelsPerUnit;
    pixelsPerBrickX = 2 * pixelsPerBrickY;
    brickMargin = pixelsPerBrickX / 20;

    pixelsPerUnit = pixelsPerBrickX / brickWidth;
    pixelPlayArea = pixelsPerUnit * 110.0;

    unsigned margin = res.width - pixelPlayArea;
    if (margin < 240) {
        margin = (margin - 120) * 2;
    }
    xoff = margin / 2;
    yoff = (res.height - pixelPlayArea) / 2;

    for (const char* l = level; *l; l++) {
        if (*l != ' '  && *l != 'X') bricksLeft++;
    }
}

static void update(double nanoseconds) {
    updateBall(nanoseconds);
    updatePickups(nanoseconds);
    updatePaddle(0.0);

    for (char* l = level; *l; l++) {
        if (*l == ';') *l = ':';
        if (*l == ',') *l = '.';
    }

    if (bricksLeft == 0 || lifes == 0) {
        const char* message = lifes == 0 ? "Game Over" : "You won!";
        size_t pixelLength = strlen(message) * 9 - 1;
        printText(message, (res.width - pixelLength) / 2, (res.height - 16) / 2,
                RGB(255, 0, 0), RGB(0, 255, 0));
        gameRunning = false;
    }

    char buffer[21];
    snprintf(buffer, sizeof(buffer), "Score: %05u", score);
    printText(buffer, xoff + pixelPlayArea, yoff + 10, RGB(255, 255, 255),
            RGB(0, 0, 0));
    snprintf(buffer, sizeof(buffer), "Lifes: %u", lifes);
    printText(buffer, xoff + pixelPlayArea, yoff + 26, RGB(255, 255, 255),
            RGB(0, 0, 0));
}

static void updateBall(double nanoseconds) {
    if (preparing) return;
    double newX = ballX + sin(ballAngle) * ballSpeed * nanoseconds;
    double newY = ballY - cos(ballAngle) * ballSpeed * nanoseconds;

    bool collision = false;
    bool side = false;

    for (double w = 0.0; w < 2 * M_PI; w += M_PI / 20) {
        double x = newX + sin(w);
        double y = newY + cos(w);

        unsigned brickX = x / brickWidth;
        unsigned brickY = y / brickHeight;

        size_t index = brickY * levelWidth + brickX;
        int c = (index >= sizeof(level) - 1) ? ' ' : level[index];
        if (c != ' ') {
            collision = true;
            if (c == '#') {
                score += 10;
                level[index] = ';';
                drawBrick(':', brickX, brickY);
            } else if (c == ':') {
                score += 10;
                level[index] = ',';
                drawBrick('.', brickX, brickY);
            } else if (c != 'X' && c != ';' && c != ',') {
                level[index] = ' ';
                bricksLeft--;
                drawBrick(' ', brickX, brickY);
            }
            if (c == '=' || c == '.') {
                score += 10;
            } else if (c == '5') {
                addPickup(brickX * brickWidth + brickWidth / 2,
                        brickY * brickHeight + brickHeight / 2, '5');
            } else if (c == '1') {
                addPickup(brickX * brickWidth + brickWidth / 2,
                        brickY * brickHeight + brickHeight / 2, '1');
            } else if (c == '+') {
                addPickup(brickX * brickWidth + brickWidth / 2,
                        brickY * brickHeight + brickHeight / 2, '+');
            }

            double brickLeft = brickX * brickWidth;
            double brickTop = brickY * brickHeight;

            bool left = newX < brickLeft;
            bool right = newX > brickLeft + brickWidth;
            bool top = newY < brickTop;
            bool bottom = newY > brickTop + brickHeight;
            side = left || right;

            if ((left || right) && (top || bottom)) {
                double relX = left ? newX - brickLeft :
                        newX - (brickLeft + brickWidth);
                double relY = top ? newY - brickTop :
                        newY - (brickTop + brickHeight);

                if (fabs(relX) <= fabs(relY)) {
                    side = false;
                }
            }
        }

        if (x <= 0.0 || y <= 0.0 || x >= 110.0) {
            collision = true;
            side = x <= 0.0 || x >= 110.0;
        }

        if (y >= 110.0) {
            lifes--;
            if (lifes == 0) {
                gameRunning = false;
            } else {
                newX = 55.0;
                newY = 80.0;
                preparing = true;
            }
            break;
        }

        if (y >= paddleY && y <= paddleY + 2.0 && x >= paddlePos - paddleLength
                && x <= paddlePos + paddleLength) {
            double relativePos = (newX - paddlePos) / (paddleLength + 1.5);
            ballAngle = relativePos * fabs(relativePos) * M_PI_2;
            newX += sin(ballAngle) * ballSpeed * nanoseconds;
            newY += sin(ballAngle) * ballSpeed * nanoseconds;
            collision = newX <= 1.0 || newX >= 109.0;
            break;
        }
    }

    if (!collision) {
        drawBall(ballX, ballY, newX, newY);
        ballX = newX;
        ballY = newY;
    } else if (side) {
        ballAngle = -ballAngle;
    } else {
        ballAngle = M_PI - ballAngle;
    }
}

static void updatePaddle(double diff) {
    unsigned oldLeft = xoff + (paddlePos - paddleLength) * pixelsPerUnit;
    unsigned oldRight = xoff + (paddlePos + paddleLength) * pixelsPerUnit;

    paddlePos += diff;
    if (paddlePos > 110.0 - paddleLength) {
        paddlePos = 110.0 - paddleLength;
    }
    if (paddlePos < paddleLength) paddlePos = paddleLength;

    unsigned left = xoff + (paddlePos - paddleLength) * pixelsPerUnit;
    unsigned right = xoff + (paddlePos + paddleLength) * pixelsPerUnit;

    unsigned paddleYPixel = yoff + (unsigned) (paddleY * pixelsPerUnit);
    unsigned paddleHeight = 2.0 * pixelsPerUnit;

    for (size_t y = paddleYPixel; y < paddleYPixel + paddleHeight; y++) {
        for (size_t x = min(oldLeft, left); x < max(oldRight, right); x++) {
            uint32_t color = (x >= left && x < right) ? paddleColor : bgColor;
            lfb[y * res.width + x] = color;
        }
    }

    redraw(min(oldLeft, left), paddleYPixel,
            max(oldRight, right) - min(oldLeft, left), paddleHeight);
}

static void updatePickup(struct Pickup* pickup, double nanoseconds) {
    bool removePickup = false;
    unsigned oldY = round(pickup->y * pixelsPerUnit);
    pickup->y += pickupSpeed * nanoseconds;
    if (pickup->y >= 110.0) {
        removePickup = true;
    }

    if (pickup->y >= paddleY && pickup->y <= paddleY + 2.0 &&
            pickup->x >= paddlePos - paddleLength &&
            pickup->x <= paddlePos + paddleLength) {
        if (pickup->type == '1') {
            score += 100;
        } else if (pickup->type == '5') {
            score += 500;
        } else if (pickup->type == '+') {
            lifes++;
        }
        removePickup = true;
    }

    const char* text = "";
    uint32_t color = 0;
    if (pickup->type == '1') {
        text = "100";
        color = brick100[0];
    } else if (pickup->type == '5') {
        text = "500";
        color = brick500[0];
    } else if (pickup->type == '+') {
        text = "Life";
        color = lifeBrick[0];
    }
    size_t pixelLength = strlen(text) * 9 - 1;
    unsigned x = round(pickup->x * pixelsPerUnit);
    redraw(xoff + x - pixelLength / 2, yoff + oldY - 8, pixelLength, 16);

    if (removePickup) {
        if (pickup->next) {
            pickup->next->prev = pickup->prev;
        }
        if (pickup->prev) {
            pickup->prev->next = pickup->next;
        } else {
            pickups = pickup->next;
        }
        free(pickup);
        return;
    }

    unsigned y = round(pickup->y * pixelsPerUnit);
    printText(text, xoff + x - pixelLength / 2, yoff + y - 8, color,
            RGBA(0, 0, 0, 0));
}

static void updatePickups(double nanoseconds) {
    struct Pickup* pickup = pickups;
    while (pickup) {
        struct Pickup* next = pickup->next;
        updatePickup(pickup, nanoseconds);
        pickup = next;
    }
}
