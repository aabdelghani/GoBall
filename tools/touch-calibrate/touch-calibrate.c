/*
 * touch-calibrate — SDL2 touchscreen calibration tool
 *
 * Shows 5 crosshair targets one at a time. Tap each target.
 * After all 5, prints the libinput calibration matrix and udev rule.
 *
 * Usage: WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/labwc touch-calibrate
 */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <math.h>

#define SCREEN_W 2560
#define SCREEN_H 720
#define MARGIN   80
#define CROSS_SZ 30
#define NUM_PTS  5

typedef struct { float x, y; } Point;

/* Target positions (screen coords) */
static Point targets[NUM_PTS] = {
    { MARGIN,            MARGIN },              /* top-left */
    { SCREEN_W - MARGIN, MARGIN },              /* top-right */
    { SCREEN_W / 2.0f,   SCREEN_H / 2.0f },    /* center */
    { MARGIN,            SCREEN_H - MARGIN },   /* bottom-left */
    { SCREEN_W - MARGIN, SCREEN_H - MARGIN },   /* bottom-right */
};

/* Recorded touch positions */
static Point touches[NUM_PTS];

static void draw_cross(SDL_Renderer *r, int cx, int cy, int active)
{
    if (active)
        SDL_SetRenderDrawColor(r, 0, 244, 106, 255); /* green */
    else
        SDL_SetRenderDrawColor(r, 100, 100, 100, 255); /* grey */

    SDL_RenderDrawLine(r, cx - CROSS_SZ, cy, cx + CROSS_SZ, cy);
    SDL_RenderDrawLine(r, cx, cy - CROSS_SZ, cx, cy + CROSS_SZ);

    /* Draw circle outline */
    for (int i = 0; i < 360; i++)
    {
        float rad = i * M_PI / 180.0f;
        int px = cx + (int)(CROSS_SZ * cosf(rad));
        int py = cy + (int)(CROSS_SZ * sinf(rad));
        SDL_RenderDrawPoint(r, px, py);
    }
}

static void draw_text_hint(SDL_Renderer *r, int point_idx)
{
    /* Simple rectangle as "banner" since we don't have SDL_ttf */
    SDL_Rect banner = { SCREEN_W / 2 - 200, SCREEN_H / 2 + 60, 400, 40 };
    SDL_SetRenderDrawColor(r, 0, 0, 0, 200);
    SDL_RenderFillRect(r, &banner);

    /* Can't render text without SDL_ttf, but the crosshair is self-explanatory */
    (void)point_idx;
}

static void render(SDL_Renderer *r, int current)
{
    SDL_SetRenderDrawColor(r, 30, 30, 30, 255);
    SDL_RenderClear(r);

    /* Draw all previous points as grey, current as green */
    for (int i = 0; i <= current && i < NUM_PTS; i++)
    {
        draw_cross(r, (int)targets[i].x, (int)targets[i].y, i == current);
    }

    /* Draw already-tapped points in red to show recorded position */
    SDL_SetRenderDrawColor(r, 255, 60, 60, 255);
    for (int i = 0; i < current; i++)
    {
        int tx = (int)touches[i].x;
        int ty = (int)touches[i].y;
        SDL_RenderDrawLine(r, tx - 10, ty - 10, tx + 10, ty + 10);
        SDL_RenderDrawLine(r, tx - 10, ty + 10, tx + 10, ty - 10);
    }

    draw_text_hint(r, current);
    SDL_RenderPresent(r);
}

/*
 * Compute 2D affine calibration matrix from touch→screen mapping.
 * We solve for: screen = M * touch
 *   sx = a*tx + b*ty + c
 *   sy = d*tx + e*ty + f
 *
 * Using least squares with NUM_PTS points.
 * Output: libinput calibration matrix [a b c d e f]
 */
static void compute_matrix(float mat[6])
{
    /* Normalize coordinates to [0,1] */
    float sum_tx = 0, sum_ty = 0, sum_tx2 = 0, sum_ty2 = 0, sum_txty = 0;
    float sum_sx = 0, sum_sy = 0, sum_txsx = 0, sum_tysx = 0;
    float sum_txsy = 0, sum_tysy = 0;

    for (int i = 0; i < NUM_PTS; i++)
    {
        float tx = touches[i].x / SCREEN_W;
        float ty = touches[i].y / SCREEN_H;
        float sx = targets[i].x / SCREEN_W;
        float sy = targets[i].y / SCREEN_H;

        sum_tx += tx;     sum_ty += ty;
        sum_tx2 += tx*tx; sum_ty2 += ty*ty;
        sum_txty += tx*ty;
        sum_sx += sx;     sum_sy += sy;
        sum_txsx += tx*sx; sum_tysx += ty*sx;
        sum_txsy += tx*sy; sum_tysy += ty*sy;
    }

    float n = NUM_PTS;

    /* Solve 3x3 system for [a, b, c] and [d, e, f] using Cramer's rule */
    /* | sum_tx2  sum_txty  sum_tx | | a |   | sum_txsx |
       | sum_txty sum_ty2   sum_ty | | b | = | sum_tysx |
       | sum_tx   sum_ty    n      | | c |   | sum_sx   | */

    float A[3][3] = {
        { sum_tx2,  sum_txty, sum_tx },
        { sum_txty, sum_ty2,  sum_ty },
        { sum_tx,   sum_ty,   n      },
    };

    /* Determinant */
    float det = A[0][0]*(A[1][1]*A[2][2] - A[1][2]*A[2][1])
              - A[0][1]*(A[1][0]*A[2][2] - A[1][2]*A[2][0])
              + A[0][2]*(A[1][0]*A[2][1] - A[1][1]*A[2][0]);

    if (fabsf(det) < 1e-10f)
    {
        fprintf(stderr, "Singular matrix — calibration failed\n");
        mat[0] = 1; mat[1] = 0; mat[2] = 0;
        mat[3] = 0; mat[4] = 1; mat[5] = 0;
        return;
    }

    /* Solve for sx coefficients (a, b, c) */
    float rhs_x[3] = { sum_txsx, sum_tysx, sum_sx };
    float inv_det = 1.0f / det;

    mat[0] = inv_det * (rhs_x[0]*(A[1][1]*A[2][2]-A[1][2]*A[2][1])
                       -A[0][1]*(rhs_x[1]*A[2][2]-A[1][2]*rhs_x[2])
                       +A[0][2]*(rhs_x[1]*A[2][1]-A[1][1]*rhs_x[2]));

    mat[1] = inv_det * (A[0][0]*(rhs_x[1]*A[2][2]-A[1][2]*rhs_x[2])
                       -rhs_x[0]*(A[1][0]*A[2][2]-A[1][2]*A[2][0])
                       +A[0][2]*(A[1][0]*rhs_x[2]-rhs_x[1]*A[2][0]));

    mat[2] = inv_det * (A[0][0]*(A[1][1]*rhs_x[2]-rhs_x[1]*A[2][1])
                       -A[0][1]*(A[1][0]*rhs_x[2]-rhs_x[1]*A[2][0])
                       +rhs_x[0]*(A[1][0]*A[2][1]-A[1][1]*A[2][0]));

    /* Solve for sy coefficients (d, e, f) */
    float rhs_y[3] = { sum_txsy, sum_tysy, sum_sy };

    mat[3] = inv_det * (rhs_y[0]*(A[1][1]*A[2][2]-A[1][2]*A[2][1])
                       -A[0][1]*(rhs_y[1]*A[2][2]-A[1][2]*rhs_y[2])
                       +A[0][2]*(rhs_y[1]*A[2][1]-A[1][1]*rhs_y[2]));

    mat[4] = inv_det * (A[0][0]*(rhs_y[1]*A[2][2]-A[1][2]*rhs_y[2])
                       -rhs_y[0]*(A[1][0]*A[2][2]-A[1][2]*A[2][0])
                       +A[0][2]*(A[1][0]*rhs_y[2]-rhs_y[1]*A[2][0]));

    mat[5] = inv_det * (A[0][0]*(A[1][1]*rhs_y[2]-rhs_y[1]*A[2][1])
                       -A[0][1]*(A[1][0]*rhs_y[2]-rhs_y[1]*A[2][0])
                       +rhs_y[0]*(A[1][0]*A[2][1]-A[1][1]*A[2][0]));
}

int main(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow("Touch Calibration",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_W, SCREEN_H, SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!win)
    {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!ren)
    {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    SDL_ShowCursor(SDL_DISABLE);

    int current = 0;
    int running = 1;

    printf("\n=== Touch Calibration ===\n");
    printf("Tap the GREEN crosshair target. %d points total.\n\n", NUM_PTS);

    render(ren, current);

    while (running && current < NUM_PTS)
    {
        SDL_Event ev;
        while (SDL_PollEvent(&ev))
        {
            switch (ev.type)
            {
            case SDL_QUIT:
                running = 0;
                break;

            case SDL_FINGERDOWN:
                /* Ignore — touchscreen also sends MOUSEBUTTONDOWN */
                break;

            case SDL_MOUSEBUTTONDOWN:
            {
                float tx = (float)ev.button.x;
                float ty = (float)ev.button.y;
                touches[current].x = tx;
                touches[current].y = ty;

                printf("Point %d: target(%.0f, %.0f) → click(%.0f, %.0f)  "
                       "offset(%.0f, %.0f)\n",
                       current + 1,
                       targets[current].x, targets[current].y,
                       tx, ty,
                       tx - targets[current].x, ty - targets[current].y);

                current++;
                render(ren, current);
                SDL_Delay(500); /* debounce — ignore taps for 500ms */
                /* Flush any queued events during debounce */
                { SDL_Event flush; while (SDL_PollEvent(&flush)) {} }
                break;
            }

            case SDL_KEYDOWN:
                if (ev.key.keysym.sym == SDLK_ESCAPE || ev.key.keysym.sym == SDLK_q)
                    running = 0;
                break;
            }
        }
        SDL_Delay(16);
    }

    if (current == NUM_PTS)
    {
        float mat[6];
        compute_matrix(mat);

        printf("\n=== Calibration Complete ===\n\n");
        printf("Libinput calibration matrix:\n");
        printf("  %.4f %.4f %.4f %.4f %.4f %.4f\n\n",
               mat[0], mat[1], mat[2], mat[3], mat[4], mat[5]);

        printf("Add this udev rule to /etc/udev/rules.d/99-touchscreen-cal.rules:\n\n");
        printf("  ENV{ID_INPUT_TOUCHSCREEN}==\"1\", "
               "ENV{LIBINPUT_CALIBRATION_MATRIX}=\"%.4f %.4f %.4f %.4f %.4f %.4f\"\n\n",
               mat[0], mat[1], mat[2], mat[3], mat[4], mat[5]);

        printf("Then run: udevadm control --reload-rules && udevadm trigger\n\n");

        /* Also save to file */
        FILE *f = fopen("/tmp/touch-calibration.txt", "w");
        if (f)
        {
            fprintf(f, "LIBINPUT_CALIBRATION_MATRIX=%.4f %.4f %.4f %.4f %.4f %.4f\n",
                    mat[0], mat[1], mat[2], mat[3], mat[4], mat[5]);
            fprintf(f, "ENV{ID_INPUT_TOUCHSCREEN}==\"1\", "
                    "ENV{LIBINPUT_CALIBRATION_MATRIX}=\"%.4f %.4f %.4f %.4f %.4f %.4f\"\n",
                    mat[0], mat[1], mat[2], mat[3], mat[4], mat[5]);
            fclose(f);
            printf("Saved to /tmp/touch-calibration.txt\n");
        }
    }
    else
    {
        printf("\nCalibration cancelled.\n");
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
