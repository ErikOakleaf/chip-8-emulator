#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "main.h"

int main(int argc, char *argv[])
{
    // Initialize SDL
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
    {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    // Create a window
    SDL_Window *window = SDL_CreateWindow(
        "SDL Window",            // Window title
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

    // Event loop
    bool quit = false;
    SDL_Event event;
    while (!quit)
    {
        // Handle events on queue
        while (SDL_PollEvent(&event) != 0)
        {
            // User requests quit
            if (event.type == SDL_QUIT)
            {
                quit = true;
            }
        }
    }

    // Destroy window
    SDL_DestroyWindow(window);

    // Quit SDL subsystems
    SDL_Quit();

    return 0;
}
