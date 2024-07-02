#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "main.h"

// init variables
SDL_Window *window;
SDL_Renderer *renderer;

unsigned short opcode;

unsigned char memory[4096];
unsigned char V[16];
unsigned short I;
unsigned short pc;

unsigned short stack[16];
unsigned short sp;

unsigned char screen[64 * 32];
unsigned char key[16];

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
}

void init_chip8()
{
    delay_timer = 0;
    sound_timer = 0;
    opcode = 0;
    pc = 0x200;
    I = 0;
    sp = 0;

    // load font of first 80 indexes in memory
    for (int i = 0; i < 80; i++)
        memory[i] = font[i];
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

    printf("Executing opcode 0x%04X at PC 0x%04X\n", opcode, pc);

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
            for (int i = 0; i < 16; i++)
            {
                if (key[i])
                {
                    V[x] = i;
                }
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

    // Update timers
    if (delay_timer > 0)
        --delay_timer;

    if (sound_timer > 0)
    {
        if (sound_timer == 1)
            printf("BEEP!\n");
        --sound_timer;
    }
}

void draw()
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    // Draw a white rectangle
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    for (int y = 0; y < 32; y++)
    {
        for (int x = 0; x < 64; x++)
        {
            if (screen[x + (y * 64)])
            {
                SDL_Rect rect;

                rect.x = x * 16;
                rect.y = y * 16;
                rect.w = 16;
                rect.h = 16;

                SDL_RenderFillRect(renderer, &rect);
            }
        }
    }

    // SDL_Rect rect1 = {50, 50, 50, 50};
    // SDL_RenderFillRect(renderer, &rect1);

    SDL_RenderPresent(renderer);
}

int main(int argc, char *argv[])
{

    // init sdl
    init_sdl();

    // init chip 8 and load game to memory
    init_chip8();
    load_rom("roms/test_opcode.ch8");

    // Emulation loop
    bool quit = false;
    SDL_Event event;

    Uint32 frameStart;
    int frameTime;

    while (!quit)
    {
        frameStart = SDL_GetTicks();

        emulate_cycle();

        if (drawflag)
            draw();

        while (SDL_PollEvent(&event) != 0)
        {
            // User requests quit
            if (event.type == SDL_QUIT)
            {
                quit = true;
            }
        }

        // Store key press state (Press and Release)

        frameTime = SDL_GetTicks() - frameStart;
        if (frameTime < FRAME_DELAY)
        {
            SDL_Delay(FRAME_DELAY - frameTime);
        }
    }

    // Destroy window
    SDL_DestroyWindow(window);

    // Quit SDL subsystems
    SDL_Quit();

    return 0;
}
