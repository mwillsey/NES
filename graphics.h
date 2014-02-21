/* 
 * graphics.h
 * by Max Willsey
 * graphics for a lame NES emulator
 */

#include <SDL2/SDL.h>

typedef struct {
  Uint8 *frame_buffer;
  SDL_Window *window;
  SDL_Surface *screen;
} tv;


void tv_init (tv* tv, Uint8 *frame_buffer);
void tv_update (tv* tv);
