/* Copyright (c) 2020, 2021, 2022 Dennis Wölfing
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

#include <dxui.h>
#include <math.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dennix/kbkeys.h>

#define min(x, y) ((x < y) ? (x) : (y))
#define max(x, y) ((x > y) ? (x) : (y))

typedef struct {
    double x;
    double y;
} Coord;

static const dxui_color
brick100[] = { RGB(163, 163, 163), RGB(190, 190, 190), RGB(127, 127, 127) },
brick500[] = { RGB(200, 100, 0), RGB(250, 150, 50), RGB(200, 30, 50) },
lifeBrick[] = { RGB(220, 220, 220), RGB(255, 255, 255), RGB(150, 150, 150) },
normalBrick[] = { RGB(127, 0, 50), RGB(150, 20, 90), RGB(100, 30, 80) },
threeBrick[] = { RGB(128, 0, 128), RGB(128, 50, 128), RGB(100, 0, 100) },
threeBrick2[] = { RGB(180, 60, 180), RGB(180, 100, 180), RGB(128, 0, 128) },
threeBrick3[] = { RGB(200, 100, 200), RGB(230, 150, 230), RGB(150, 30, 150) },
undestroyableBrick[] = { RGB(25, 25, 25), RGB(35, 35, 25), RGB(10, 10, 20) };
static const dxui_color paddleColor = RGB(127, 127, 0);
static const dxui_color bgColor = RGB(0, 0, 60);
static const dxui_color ballColor = RGB(255, 0, 0);

static const double ballSpeed = 0.00000002;
static const double brickHeight = 5.0;
static const double brickWidth = 10.0;
static const double paddleLength = 5.0;
static const double paddleSpeed = 0.9;
static const double paddleY = 105.0;
static const double pickupSpeed = 0.00000003;

static dxui_context* context;
static dxui_color* lfb;
static dxui_window* window;
static dxui_dim windowDim;

static const int levelWidth = 11;
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
static bool resized;

static unsigned brickMargin;
static unsigned lifes = 3;
static unsigned pixelsPerBrickX;
static unsigned pixelsPerBrickY;
static unsigned score;
static dxui_rect playArea;

static double pixelsPerUnit;
static Coord ballCoords = { 55.0, 80.0 };
static double ballAngle;
static double paddlePos = 55.0;

struct Pickup {
    Coord coords;
    char type;
    struct Pickup* prev;
    struct Pickup* next;
};

static void addPickup(double x, double y, char type);
static void drawBall(Coord oldCoords, Coord newCoords);
static void drawBrick(char type, unsigned brickX, unsigned brickY, bool redraw);
static void drawLevel(void);
static void handleResize(void);
static void onClose(dxui_window* window);
static void onKey(dxui_control* control, dxui_key_event* event);
static void onMouse(dxui_control* control, dxui_mouse_event* event);
static void onResize(dxui_window* window, dxui_resize_event* event);
static void redrawBricks(dxui_rect rect);
static void setup(void);
static void update(double nanoseconds);
static void updateBall(double nanoseconds);
static void updatePaddle(double diff);
static void updatePickup(struct Pickup* pickup, double nanoseconds);
static void updatePickups(double nanoseconds);

static dxui_pos coordsToPos(Coord coord) {
    dxui_pos pos;
    pos.x = playArea.x + coord.x * pixelsPerUnit;
    pos.y = playArea.y + coord.y * pixelsPerUnit;
    return pos;
}

static Coord posToCoords(dxui_pos pos) {
    Coord coords;
    coords.x = (pos.x - playArea.x) / pixelsPerUnit;
    coords.y = (pos.y - playArea.y) / pixelsPerUnit;
    return coords;
}

static void shutdown(void) {
    dxui_shutdown(context);
}


int main(void) {
    setup();

    struct timespec oldTs;
    clock_gettime(CLOCK_MONOTONIC, &oldTs);
    while (true) {
        dxui_pump_events(context, DXUI_PUMP_ONCE_CLEAR, 16);

        if (resized) {
            handleResize();
        }

        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
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
    if (!pickup) dxui_panic(context, "Out of memory");
    pickup->coords.x = x;
    pickup->coords.y = y;
    pickup->type = type;
    pickup->prev = NULL;
    pickup->next = pickups;
    if (pickup->next) {
        pickup->next->prev = pickup;
    }
    pickups = pickup;
}

static void drawBall(Coord oldCoords, Coord newCoords) {
    double unitsPerPixel = 1 / pixelsPerUnit;

    dxui_pos oldPos = coordsToPos(oldCoords);
    dxui_pos newPos = coordsToPos(newCoords);

    for (unsigned i = 0; i < pixelsPerUnit; i++) {
        double y = i * unitsPerPixel + unitsPerPixel / 2;
        if (y > 1.0) y = 1.0;
        double x = sqrt(1 - y * y);

        int width = round(x * pixelsPerUnit);
        for (int j = -width; j < width; j++) {
            lfb[(oldPos.y - i) * windowDim.width + oldPos.x + j] = bgColor;
            lfb[(oldPos.y + i) * windowDim.width + oldPos.x + j] = bgColor;
        }
    }

    for (unsigned i = 0; i < pixelsPerUnit; i++) {
        double y = i * unitsPerPixel + unitsPerPixel / 2;
        if (y > 1.0) y = 1.0;
        double x = sqrt(1 - y * y);

        int width = round(x * pixelsPerUnit);
        for (int j = -width; j < width; j++) {
            lfb[(newPos.y - i) * windowDim.width + newPos.x + j] = ballColor;
            lfb[(newPos.y + i) * windowDim.width + newPos.x + j] = ballColor;
        }
    }

    dxui_rect rect;
    rect.x = min(oldPos.x, newPos.x) - round(pixelsPerUnit);
    rect.y = min(oldPos.y, newPos.y) - round(pixelsPerUnit);
    rect.width = max(oldPos.x, newPos.x) + round(pixelsPerUnit) - rect.x;
    rect.height = max(oldPos.y, newPos.y) + round(pixelsPerUnit) - rect.y;
    dxui_update_framebuffer(window, rect);
}

static void drawBrick(char type, unsigned brickX, unsigned brickY,
        bool redraw) {
    const dxui_color background[] = { bgColor, bgColor, bgColor };
    const dxui_color* color = type == '=' ? normalBrick :
            type == '1' ? brick100 : type == '5' ? brick500 :
            type == '+' ? lifeBrick : type == '#' ? threeBrick :
            type == ':' ? threeBrick2 : type == '.' ? threeBrick3 :
            type == 'X' ? undestroyableBrick : background;

    unsigned xPixel = playArea.x + brickX * pixelsPerBrickX;
    unsigned yPixel = playArea.y + brickY * pixelsPerBrickY;
    for (unsigned x = 1; x < pixelsPerBrickX - 1; x++) {
        for (unsigned y = 1; y < pixelsPerBrickY - 1; y++) {
            dxui_color pixelColor = color[0];
            if ((x <= brickMargin && y < pixelsPerBrickY - x) ||
                    (y <= brickMargin && x < pixelsPerBrickX - y)) {
                pixelColor = color[1];
            } else if (x >= pixelsPerBrickX - 1 - brickMargin ||
                    y >= pixelsPerBrickY - 1 - brickMargin) {
                pixelColor = color[2];
            }
            lfb[(y + yPixel) * windowDim.width + x + xPixel] = pixelColor;
        }
    }

    if (redraw) {
        dxui_rect rect;
        rect.x = xPixel;
        rect.y = yPixel;
        rect.width = pixelsPerBrickX;
        rect.height = pixelsPerBrickY;
        dxui_update_framebuffer(window, rect);
    }
}

static void drawLevel(void) {
    for (int y = playArea.y; y < playArea.y + playArea.height; y++) {
        for (int x = playArea.x; x < playArea.x + playArea.width; x++) {
            lfb[y * windowDim.width + x] = bgColor;
        }
    }

    size_t height = sizeof(level) / levelWidth;
    for (size_t y = 0; y < height; y++) {
        for (size_t x = 0; x < levelWidth; x++) {
            char c = level[y * levelWidth + x];
            if (c == ' ') continue;

            drawBrick(c, x, y, false);
        }
    }

    drawBall(ballCoords, ballCoords);
    updatePaddle(0.0);
    dxui_rect rect = { .pos = {0, 0}, .dim = windowDim };
    dxui_update_framebuffer(window, rect);
}

static void handleResize(void) {
    windowDim = dxui_get_dim(window);
    lfb = dxui_get_framebuffer(window, windowDim);
    if (!lfb) dxui_panic(context, "Cannot create framebuffer");

    unsigned possibleWidth = windowDim.width - 5;
    unsigned possibleHeight = windowDim.height - 5;

    double possiblePixelsPerUnit = 110.0 / min(possibleWidth, possibleHeight);
    pixelsPerBrickY = brickHeight / possiblePixelsPerUnit;
    pixelsPerBrickX = 2 * pixelsPerBrickY;
    brickMargin = pixelsPerBrickX / 20;

    pixelsPerUnit = pixelsPerBrickX / brickWidth;

    playArea.width = pixelsPerUnit * 110.0;
    playArea.height = pixelsPerUnit * 110.0;
    playArea.x = (windowDim.width - playArea.width) / 2;
    playArea.y = (windowDim.height - playArea.height) / 2;

    for (int i = 0; i < windowDim.width * windowDim.height; i++) {
        lfb[i] = COLOR_BLACK;
    }

    drawLevel();
    resized = false;
}

static void onClose(dxui_window* window) {
    (void) window;
    exit(0);
}

static void onKey(dxui_control* control, dxui_key_event* event) {
    (void) control;

    if (event->key == KB_Q) {
        exit(0);
    } else if (event->key == KB_A || event->key == KB_LEFT) {
        updatePaddle(-paddleSpeed);
    } else if (event->key == KB_D || event->key == KB_RIGHT) {
        updatePaddle(paddleSpeed);
    } else if (preparing && event->key == KB_SPACE) {
        ballAngle = atan2(55.0 - paddlePos, paddleY - ballCoords.y);
        preparing = false;
    }
}

static void onMouse(dxui_control* control, dxui_mouse_event* event) {
    (void) control;

    if (preparing && event->flags & DXUI_MOUSE_LEFT) {
        ballAngle = atan2(55.0 - paddlePos, paddleY - ballCoords.y);
        preparing = false;
    }

    Coord coords = posToCoords(event->pos);
    updatePaddle(coords.x - paddlePos);
}

static void onResize(dxui_window* window, dxui_resize_event* event) {
    (void) window; (void) event;
    resized = true;
}

static void redrawBricks(dxui_rect rect) {
    rect = dxui_rect_crop(rect, windowDim);

    for (int y = rect.y; y < rect.y + rect.height; y++) {
        for (int x = rect.x; x < rect.x + rect.width; x++) {
            if (dxui_rect_contains_pos(playArea, (dxui_pos) {x, y})) {
                lfb[y * windowDim.width + x] = bgColor;
            } else {
                lfb[y * windowDim.width + x] = COLOR_BLACK;
            }
        }
    }

    Coord topLeft = posToCoords(rect.pos);
    dxui_pos pos2 = { rect.x + rect.width, rect.y + rect.height };
    Coord bottomRight = posToCoords(pos2);

    for (int y = topLeft.y / brickHeight; y <= bottomRight.y / brickHeight;
            y++) {
        if (y < 0 || y >= (int) sizeof(level) / levelWidth) continue;
        for (int x = topLeft.x / brickWidth; x <= bottomRight.x / brickWidth;
                x++) {
            if (x < 0 || x >= levelWidth) continue;

            size_t index = y * levelWidth + x;
            int c = (index >= sizeof(level) - 1) ? ' ' : level[index];
            drawBrick(c, x, y, false);
        }
    }
}

static void setup(void) {
    atexit(shutdown);

    context = dxui_initialize(0);
    if (!context) dxui_panic(NULL, "Cannot initialize dxui");

    dxui_rect rect;
    rect.x = -1;
    rect.y = -1;

    if (dxui_is_standalone(context)) {
        rect.dim = dxui_get_display_dim(context);
    } else {
        rect.width = 600;
        rect.height = 600;
    }

    window = dxui_create_window(context, rect, "Bricks", 0);
    if (!window) dxui_panic(context, "Cannot create window");
    windowDim = rect.dim;

    dxui_set_event_handler(window, DXUI_EVENT_KEY, onKey);
    dxui_set_event_handler(window, DXUI_EVENT_MOUSE, onMouse);
    dxui_set_event_handler(window, DXUI_EVENT_WINDOW_CLOSE, onClose);
    dxui_set_event_handler(window, DXUI_EVENT_WINDOW_RESIZED, onResize);

    handleResize();
    dxui_show(window);

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

        dxui_rect rect = { .pos = {0, 0}, .dim = windowDim };
        rect = dxui_get_text_rect(message, rect, DXUI_TEXT_CENTERED);

        dxui_rect crop = { .pos = {0, 0}, .dim = windowDim };
        dxui_draw_text_in_rect(context, lfb, message, COLOR_WHITE, rect.pos,
                crop, windowDim.width);
        dxui_update_framebuffer(window, rect);
        gameRunning = false;
    }

    char buffer[21];
    snprintf(buffer, sizeof(buffer), "%05u", score);

    dxui_rect rect = {{0, 10, 0, 0}};
    rect = dxui_get_text_rect(buffer, rect, 0);
    rect.x = windowDim.width - rect.width - 10;

    redrawBricks(rect);

    dxui_rect crop = { .pos = {0, 0}, .dim = windowDim };
    dxui_draw_text_in_rect(context, lfb, buffer, COLOR_WHITE, rect.pos, crop,
            windowDim.width);
    dxui_update_framebuffer(window, rect);


    snprintf(buffer, sizeof(buffer), "%u", lifes);
    rect = dxui_get_text_rect(buffer, rect, 0);
    rect.x = 10;
    redrawBricks(rect);
    dxui_draw_text_in_rect(context, lfb, buffer, COLOR_WHITE, rect.pos, crop,
            windowDim.width);
    dxui_update_framebuffer(window, rect);
}

static void updateBall(double nanoseconds) {
    if (preparing) return;
    double newX = ballCoords.x + sin(ballAngle) * ballSpeed * nanoseconds;
    double newY = ballCoords.y - cos(ballAngle) * ballSpeed * nanoseconds;

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
                drawBrick(':', brickX, brickY, true);
            } else if (c == ':') {
                score += 10;
                level[index] = ',';
                drawBrick('.', brickX, brickY, true);
            } else if (c != 'X' && c != ';' && c != ',') {
                level[index] = ' ';
                bricksLeft--;
                drawBrick(' ', brickX, brickY, true);
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
        Coord newCoords = { newX, newY };
        drawBall(ballCoords, newCoords);
        ballCoords = newCoords;
    } else if (side) {
        ballAngle = -ballAngle;
    } else {
        ballAngle = M_PI - ballAngle;
    }
}

static void updatePaddle(double diff) {
    unsigned oldLeft = playArea.x + (paddlePos - paddleLength) * pixelsPerUnit;
    unsigned oldRight = playArea.x + (paddlePos + paddleLength) * pixelsPerUnit;

    paddlePos += diff;
    if (paddlePos > 110.0 - paddleLength) {
        paddlePos = 110.0 - paddleLength;
    }
    if (paddlePos < paddleLength) paddlePos = paddleLength;

    unsigned left = playArea.x + (paddlePos - paddleLength) * pixelsPerUnit;
    unsigned right = playArea.x + (paddlePos + paddleLength) * pixelsPerUnit;

    unsigned paddleYPixel = playArea.y + (unsigned) (paddleY * pixelsPerUnit);
    unsigned paddleHeight = 2.0 * pixelsPerUnit;

    for (size_t y = paddleYPixel; y < paddleYPixel + paddleHeight; y++) {
        for (size_t x = min(oldLeft, left); x < max(oldRight, right); x++) {
            dxui_color color = (x >= left && x < right) ? paddleColor : bgColor;
            lfb[y * windowDim.width + x] = color;
        }
    }

    dxui_rect rect;
    rect.x = min(oldLeft, left);
    rect.y = paddleYPixel;
    rect.width = max(oldRight, right) - rect.x;
    rect.height = paddleHeight;
    dxui_update_framebuffer(window, rect);
}

static void updatePickup(struct Pickup* pickup, double nanoseconds) {
    bool removePickup = false;
    dxui_pos oldPos = coordsToPos(pickup->coords);
    pickup->coords.y += pickupSpeed * nanoseconds;
    if (pickup->coords.y >= 110.0) {
        removePickup = true;
    }

    if (pickup->coords.y >= paddleY && pickup->coords.y <= paddleY + 2.0 &&
            pickup->coords.x >= paddlePos - paddleLength &&
            pickup->coords.x <= paddlePos + paddleLength) {
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
    dxui_color color = 0;
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

    dxui_rect rect;
    rect.pos = coordsToPos(pickup->coords);
    rect.width = 0;
    rect.height = 0;

    int delta = rect.y - oldPos.y;
    rect = dxui_get_text_rect(text, rect, DXUI_TEXT_CENTERED);

    rect.y -= delta;
    redrawBricks(rect);
    rect.y += delta;

    if (!removePickup) {
        dxui_rect crop = { .pos = {0, 0}, .dim = windowDim };
        dxui_draw_text_in_rect(context, lfb, text, color, rect.pos, crop,
                windowDim.width);
    } else {
        if (pickup->next) {
            pickup->next->prev = pickup->prev;
        }
        if (pickup->prev) {
            pickup->prev->next = pickup->next;
        } else {
            pickups = pickup->next;
        }
        free(pickup);
    }

    rect.y -= delta;
    rect.height += delta;
    dxui_update_framebuffer(window, rect);
}

static void updatePickups(double nanoseconds) {
    struct Pickup* pickup = pickups;
    while (pickup) {
        struct Pickup* next = pickup->next;
        updatePickup(pickup, nanoseconds);
        pickup = next;
    }
}
