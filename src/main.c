#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "main.h"

// init variables
SDL_Window *window;
SDL_Renderer *renderer;
TTF_Font *textFont;

typedef struct
{
    SDL_Texture *texture;
    SDL_Rect rect;
    unsigned char value;
} TextureInfo;

TextureInfo v_textures[16];
SDL_Surface *textSurface = NULL;

unsigned short opcode;

unsigned char memory[4096];
unsigned char V[16];
unsigned short I;
unsigned short pc;

unsigned short stack[16];
unsigned short sp;

unsigned char screen[64 * 32];
char key[16];

unsigned char delay_timer;
unsigned char sound_timer;
bool drawflag = false;

int init_sdl(void)
{
    // Initialize SDL
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
    {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    // Create a window
    window = SDL_CreateWindow(
        "chip-8 emulator",       // Window title
        SDL_WINDOWPOS_UNDEFINED, // Initial x position
        SDL_WINDOWPOS_UNDEFINED, // Initial y position
        WINDOW_WIDTH,            // Width, in pixels
        WINDOW_HEIGHT,           // Height, in pixels
        SDL_WINDOW_SHOWN         // Flags - see below
    );
    if (window == NULL)
    {
        fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    if (renderer == NULL)
    {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    if (TTF_Init() < 0)
    {
        printf("SDL_ttf could not initialize! TTF_Error: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    textFont = TTF_OpenFont("src/pixelated.ttf", 16);

    if (textFont == NULL)
    {
        printf("Failed to load font: %s\n", TTF_GetError());
        // Handle error (e.g., exit the program)
        return -1;
    }

    return 0;
}

void init_chip8()
{
    delay_timer = 0;
    sound_timer = 0;
    opcode = 0;
    pc = 0x200;
    I = 0;
    sp = 0;
    memset(key, 0, sizeof(key));

    // load font of first 80 indexes in memory
    for (int i = 0; i < 80; i++)
        memory[i] = font[i];

    // init the values in the v_textures array
    for (int i = 0; i < 16; i++)
    {
        v_textures[i].texture = NULL;
        v_textures[i].value = 0xFF;
    }
}

void load_rom(char *filename)
{
    FILE *file = fopen(filename, "rb");
    if (file == NULL)
    {
        fprintf(stderr, "Failed to open file: %s\n", filename);
        exit(EXIT_FAILURE);
    }

    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    rewind(file);

    // load rom into memory
    fread(memory + 512, sizeof(char), size, file);

    fclose(file);

    // TODO - add error checking for fread
}

void emulate_cycle()
{
    // Fetch Opcode
    opcode = memory[pc] << 8 | memory[pc + 1];

    unsigned nnn = opcode & 0x0FFF;
    unsigned nn = opcode & 0x00FF;
    unsigned n = (opcode & 0x000F);
    unsigned x = (opcode & 0x0F00) >> 8;
    unsigned y = (opcode & 0x00F0) >> 4;

    pc += 2;

    switch (opcode & 0xF000)
    {
    case 0x0000:
        switch (nn)
        {
        case 0x00E0:
            memset(screen, 0, 2048);
            drawflag = true;
            break;

        case 0x00EE:
            --sp;
            pc = stack[sp];
            break;

        default:
            printf("Unknown opcode [0x0000]: 0x%X\n", opcode);
            break;
        }
        break;

    case 0x1000:
        pc = nnn;
        break;

    case 0x2000:
        stack[sp] = pc;
        ++sp;
        pc = nnn;
        break;

    case 0x3000:
        if (V[x] == nn)
            pc += 2;
        break;

    case 0x4000:
        if (V[x] != nn)
            pc += 2;
        break;

    case 0x5000:
        if (V[x] == V[y])
            pc += 2;
        break;

    case 0x6000:
        V[x] = nn;
        break;

    case 0x7000:
        V[x] += nn;
        break;

    case 0x8000:
        switch (opcode & 0x000F)
        {
        case 0x0000:
            V[x] = V[y];
            break;

        case 0x0001:
            V[x] |= V[y];
            break;

        case 0x0002:
            V[x] &= V[y];
            break;

        case 0x0003:
            V[x] ^= V[y];
            break;

        case 0x0004:
        {
            if (V[x] + V[y] > 255)
                V[0xF] = 1;
            else
                V[0xF] = 0;
            V[x] += V[y];
            break;
        }

        case 0x0005:
            if (V[x] > V[y])
                V[0xF] = 0;
            else
                V[0xF] = 1;
            V[x] -= V[y];
            break;

        case 0x0006:
            V[0xF] = V[x] & 1;
            V[x] >>= 1;
            break;

        case 0x0007:
            if (V[y] > V[x])
                V[0xF] = 1;
            else
                V[0xF] = 0;
            V[x] = V[y] - V[x];
            break;

        case 0x000E:
            V[0xF] = (V[x] >> 7);
            V[x] <<= 1;
            break;

        default:
            printf("Unknown opcode: 0x%X\n", opcode);
            break;
        }
        break;

    case 0x9000:
        if (V[x] != V[y])
            pc += 2;
        break;

    case 0xA000:
        I = nnn;
        break;

    case 0xB000:
        pc = V[0] + nnn;
        break;

    case 0xC000:
        V[x] = (rand() % 0x100) & nn;
        break;

    case 0xD000:
    {
        uint16_t px = V[x];
        uint16_t py = V[y];
        uint16_t height = n;

        unsigned short pixel;
        V[0xF] = 0;
        for (int yline = 0; yline < n; yline++)
        {
            pixel = memory[I + yline];
            for (int xline = 0; xline < 8; xline++)
            {
                if ((pixel & (0x80 >> xline)) != 0)
                {
                    if (screen[px + xline + ((py + yline) * 64)] == 1)
                        V[0xF] = 1;
                    screen[px + xline + ((py + yline) * 64)] ^= 1;
                }
            }
        }
        drawflag = true;
    }
    break;

    case 0xE000:
        switch (nn)
        {
        case 0x009E:
            if (key[V[x]] != 0)
                pc += 2;
            break;

        case 0x00A1:
            if (key[V[x]] == 0)
                pc += 2;
            break;

        default:
            printf("Unknown opcode: 0x%X\n", opcode);
            break;
        }
        break;

    case 0xF000:
        switch (nn)
        {
        case 0x0007:
            V[x] = delay_timer;
            break;

        case 0x000A:
        {
            bool key_pressed = false;
            for (int i = 0; i < 16; i++)
            {
                if (key[i])
                {
                    key_pressed = true;
                    V[x] = i;
                }
            }
            if (key_pressed == false)
                pc -= 2;
        }
        break;

        case 0x0015:
            delay_timer = V[x];
            break;

        case 0x0018:
            sound_timer = V[x];
            break;

        case 0x001E:
            I += V[x];
            break;

        case 0x0029:
            I = V[x] * 5;
            break;

        case 0x0033:
            memory[I] = (V[x] % 1000) / 100;
            memory[I + 1] = (V[x] % 100) / 10;
            memory[I + 2] = (V[x] % 10);
            break;

        case 0x0055:
            for (int i = 0; i <= x; i++)
                memory[I + i] = V[i];
            break;

        case 0x0065:
            for (int i = 0; i <= x; i++)
                V[i] = memory[I + i];
            break;
        }
        break;

    default:
        printf("Unknown opcode: 0x%X\n", opcode);
        break;
    }
}

void update_timers()
{
    if (delay_timer > 0)
        --delay_timer;

    if (sound_timer > 0)
    {
        if (sound_timer == 1)
            printf("BEEP!\n");
        --sound_timer;
    }
}

SDL_Surface *render_hex_value(char value)
{
    char buffer[5];
    snprintf(buffer, sizeof(buffer), "%02X", value);

    SDL_Color textColor = {255, 255, 255, 255};

    return TTF_RenderText_Solid(textFont, buffer, textColor);
}

void draw()
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    // draw chip 8 screen

    if (drawflag)
    {
        for (int y = 0; y < 32; y++)
        {
            for (int x = 0; x < 64; x++)
            {
                if (screen[x + (y * 64)])
                {
                    SDL_Rect rect;

                    rect.x = x * SIZE;
                    rect.y = y * SIZE;
                    rect.w = SIZE;
                    rect.h = SIZE;

                    SDL_RenderFillRect(renderer, &rect);
                }
            }
        }
    }

    // draw seperation line

    SDL_RenderDrawLine(renderer, (SIZE * 64), 0, (SIZE * 64), WINDOW_HEIGHT);

    // draw debug info
    // TODO add texture caching - check claude. Also only update texture if the V value is changed.

    int cell_width = 40;
    int cell_height = 30;
    int grid_width = 4;
    int grid_height = 4;

    int start_x = SIZE * 64 + ((SIZE * 16 - (grid_width * cell_width)) / 2);
    int start_y = (WINDOW_HEIGHT - (grid_height * cell_height)) / 2;

    for (int i = 0; i < 16; i++)
    {
        int row = i / 4;
        int col = i % 4;

        if (v_textures[i].value != V[i] || v_textures[i].texture == NULL)
        {

            v_textures[i].value = V[i];

            if (v_textures[i].texture != NULL)
            {
                SDL_DestroyTexture(v_textures[i].texture);
            }

            textSurface = render_hex_value(V[i]);
            v_textures[i].texture = SDL_CreateTextureFromSurface(renderer, textSurface);

            v_textures[i].rect = (SDL_Rect){start_x + (col * cell_width) + (cell_width - textSurface->w) / 2,
                                            start_y + (row * cell_height) + (cell_height - textSurface->h) / 2,
                                            textSurface->w,
                                            textSurface->h};
        }
    }

    for (int i = 0; i < 16; i++)
    {
        SDL_RenderCopy(renderer, v_textures[i].texture, NULL, &v_textures[i].rect);
    }

    SDL_RenderPresent(renderer);
}

void get_input(SDL_Event event)
{
    switch (event.type)
    {
    case SDL_KEYDOWN:
        switch (event.key.keysym.scancode)
        {
        case SDL_SCANCODE_1:
            // printf("Key pressed: 1\n");
            key[1] = 1;
            break;

        case SDL_SCANCODE_2:
            // printf("Key pressed: 2\n");
            key[2] = 1;
            break;

        case SDL_SCANCODE_3:
            // printf("Key pressed: 3\n");
            key[3] = 1;
            break;

        case SDL_SCANCODE_4:
            // printf("Key pressed: 4\n");
            key[12] = 1;
            break;

        case SDL_SCANCODE_Q:
            // printf("Key pressed: Q\n");
            key[4] = 1;
            break;

        case SDL_SCANCODE_W:
            // printf("Key pressed: W\n");
            key[5] = 1;
            break;

        case SDL_SCANCODE_E:
            // printf("Key pressed: E\n");
            key[6] = 1;
            break;

        case SDL_SCANCODE_R:
            // printf("Key pressed: R\n");
            key[13] = 1;
            break;

        case SDL_SCANCODE_A:
            // printf("Key pressed: A\n");
            key[7] = 1;
            break;

        case SDL_SCANCODE_S:
            // printf("Key pressed: S\n");
            key[8] = 1;
            break;

        case SDL_SCANCODE_D:
            // printf("Key pressed: D\n");
            key[9] = 1;
            break;

        case SDL_SCANCODE_F:
            // printf("Key pressed: F\n");
            key[14] = 1;
            break;

        case SDL_SCANCODE_Z:
            // printf("Key pressed: Z\n");
            key[10] = 1;
            break;

        case SDL_SCANCODE_X:
            // printf("Key pressed: X\n");
            key[0] = 1;
            break;

        case SDL_SCANCODE_C:
            // printf("Key pressed: C\n");
            key[11] = 1;
            break;

        case SDL_SCANCODE_V:
            // printf("Key pressed: V\n");
            key[15] = 1;
            break;
        }
        break;

    case SDL_KEYUP:
    {
        switch (event.key.keysym.scancode)
        {
        case SDL_SCANCODE_1:
            // printf("Key unpressed: 1\n");
            key[1] = 0;
            break;

        case SDL_SCANCODE_2:
            // printf("Key unpressed: 2\n");
            key[2] = 0;
            break;

        case SDL_SCANCODE_3:
            // printf("Key unpressed: 3\n");
            key[3] = 0;
            break;

        case SDL_SCANCODE_4:
            // printf("Key unpressed: 4\n");
            key[12] = 0;
            break;

        case SDL_SCANCODE_Q:
            // printf("Key unpressed: Q\n");
            key[4] = 0;
            break;

        case SDL_SCANCODE_W:
            // printf("Key unpressed: W\n");
            key[5] = 0;
            break;

        case SDL_SCANCODE_E:
            // printf("Key unpressed: E\n");
            key[6] = 0;
            break;

        case SDL_SCANCODE_R:
            // printf("Key unpressed: R\n");
            key[13] = 0;
            break;

        case SDL_SCANCODE_A:
            // printf("Key unpressed: A\n");
            key[7] = 0;
            break;

        case SDL_SCANCODE_S:
            // printf("Key unpressed: S\n");
            key[8] = 0;
            break;

        case SDL_SCANCODE_D:
            // printf("Key unpressed: D\n");
            key[9] = 0;
            break;

        case SDL_SCANCODE_F:
            // printf("Key unpressed: F\n");
            key[14] = 0;
            break;

        case SDL_SCANCODE_Z:
            // printf("Key unpressed: Z\n");
            key[10] = 0;
            break;

        case SDL_SCANCODE_X:
            // printf("Key unpressed: X\n");
            key[0] = 0;
            break;

        case SDL_SCANCODE_C:
            // printf("Key unpressed: C\n");
            key[11] = 0;
            break;

        case SDL_SCANCODE_V:
            // printf("Key unpressed: V\n");
            key[15] = 0;
            break;
        }
    }
    break;
    }
}

int main(int argc, char *argv[])
{

    // init sdl
    init_sdl();

    // init chip 8 and load game to memory
    init_chip8();
    load_rom("roms/Space Invaders [David Winter].ch8");

    bool quit = false;
    SDL_Event event;

    Uint32 frameStart;
    Uint32 lastTimerUpdate = 0;
    Uint32 timerUpdateInterval = 1000 / 60;
    Uint32 frameCountStart = SDL_GetTicks();
    int frameCount = 0;

    float fps = 0;
    int frameTime;

    while (!quit)
    {
        frameStart = SDL_GetTicks();

        while (SDL_PollEvent(&event) != 0)
        {
            switch (event.type)
            {
            case SDL_QUIT:
                quit = true;
                break;
            case SDL_KEYDOWN:
            case SDL_KEYUP:
                get_input(event);
                break;
            }
        }

        emulate_cycle();

        Uint32 currentTime = SDL_GetTicks();
        if (currentTime - lastTimerUpdate >= timerUpdateInterval)
        {
            update_timers();
            draw();
            lastTimerUpdate = currentTime;
        }

        frameTime = SDL_GetTicks() - frameStart;
        if (frameTime < FRAME_DELAY)
        {
            SDL_Delay(FRAME_DELAY - frameTime);
        }
    }

    // cleanup
    TTF_CloseFont(textFont);
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    for (int i = 0; i < 16; i++)
    {
        if (v_textures[i].texture != NULL)
        {
            SDL_DestroyTexture(v_textures[i].texture);
        }
    }

    return 0;
}
