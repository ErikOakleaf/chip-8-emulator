#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "main.h"

// init variables
SDL_Window *window;

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

int init_sdl(void)
{
    // Initialize SDL
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
    {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    // Create a window
    SDL_Window *window = SDL_CreateWindow(
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

    switch (opcode & 0xF000)
    {
    case 0xA000:

        I = opcode & 0x0FFF;
        pc += 2;

        break;

    default:
        printf("Unknown opcode: 0x%X\n", opcode);
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

int main(int argc, char *argv[])
{

    // init sdl
    init_sdl();

    // init chip 8 and load game to memory
    init_chip8();
    load_rom("roms/1-chip8-logo.ch8");

    // Emulation loop
    bool quit = false;
    SDL_Event event;
    while (!quit)
    {

        // Emulate cycle
        emulate_cycle();

        // if the draw flag is set update the screen

        while (SDL_PollEvent(&event) != 0)
        {
            // User requests quit
            if (event.type == SDL_QUIT)
            {
                quit = true;
            }
        }

        // Store key press state (Press and Release)
    }

    // Destroy window
    SDL_DestroyWindow(window);

    // Quit SDL subsystems
    SDL_Quit();

    return 0;
}
