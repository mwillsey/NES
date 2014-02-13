/* 
 * graphics.c
 * graphics for a lame NES emulator
 */

#include <SDL2/SDL.h>

/* static const SDL_Color color_palette[] = { */
/*   (SDL_Color){7C,7C,7C}, (SDL_Color){00,00,FC}, (SDL_Color){00,00,BC}, (SDL_Color){44,28,BC}, */
/*   (SDL_Color){94,00,84}, (SDL_Color){A8,00,20}, (SDL_Color){A8,10,00}, (SDL_Color){88,14,00}, */
/*   (SDL_Color){50,30,00}, (SDL_Color){00,78,00}, (SDL_Color){00,68,00}, (SDL_Color){00,58,00}, */
/*   (SDL_Color){00,40,58}, (SDL_Color){00,00,00}, (SDL_Color){00,00,00}, (SDL_Color){00,00,00}, */
/*   (SDL_Color){BC,BC,BC}, (SDL_Color){00,78,F8}, (SDL_Color){00,58,F8}, (SDL_Color){68,44,FC}, */
/*   (SDL_Color){D8,00,CC}, (SDL_Color){E4,00,58}, (SDL_Color){F8,38,00}, (SDL_Color){E4,5C,10}, */
/*   (SDL_Color){AC,7C,00}, (SDL_Color){00,B8,00}, (SDL_Color){00,A8,00}, (SDL_Color){00,A8,44}, */
/*   (SDL_Color){00,88,88}, (SDL_Color){00,00,00}, (SDL_Color){00,00,00}, (SDL_Color){00,00,00}, */
/*   (SDL_Color){F8,F8,F8}, (SDL_Color){3C,BC,FC}, (SDL_Color){68,88,FC}, (SDL_Color){98,78,F8}, */
/*   (SDL_Color){F8,78,F8}, (SDL_Color){F8,58,98}, (SDL_Color){F8,78,58}, (SDL_Color){FC,A0,44}, */
/*   (SDL_Color){F8,B8,00}, (SDL_Color){B8,F8,18}, (SDL_Color){58,D8,54}, (SDL_Color){58,F8,98}, */
/*   (SDL_Color){00,E8,D8}, (SDL_Color){78,78,78}, (SDL_Color){00,00,00}, (SDL_Color){00,00,00}, */
/*   (SDL_Color){FC,FC,FC}, (SDL_Color){A4,E4,FC}, (SDL_Color){B8,B8,F8}, (SDL_Color){D8,B8,F8}, */
/*   (SDL_Color){F8,B8,F8}, (SDL_Color){F8,A4,C0}, (SDL_Color){F0,D0,B0}, (SDL_Color){FC,E0,A8}, */
/*   (SDL_Color){F8,D8,78}, (SDL_Color){D8,F8,78}, (SDL_Color){B8,F8,B8}, (SDL_Color){B8,F8,D8}, */
/*   (SDL_Color){00,FC,FC}, (SDL_Color){F8,D8,F8}, (SDL_Color){00,00,00}, (SDL_Color){00,00,00} */
/* } */

typedef struct {
  Uint8 *frame_buffer;
  SDL_Window *window;
  SDL_Surface *screen;
} tv;

void tv_init (tv* tv, Uint8 *frame_buffer) {
  tv->frame_buffer = frame_buffer;

  SDL_Init (SDL_INIT_VIDEO);
  tv->window = SDL_CreateWindow("my terrible NES", 
                                SDL_WINDOWPOS_UNDEFINED, 
                                SDL_WINDOWPOS_UNDEFINED, 
                                256, 240, SDL_WINDOW_SHOWN);
  tv->screen = SDL_GetWindowSurface(tv->window);
}
  

void tv_update (tv* tv) {
  int color;
  int i, j;

  int *pixels = (int*)tv->screen->pixels;

  for (i = 0; i < 240; i++) {
    for (j = 0; j < 256; j++) {
      color = 0xff0000; //tv->frame_buffer[i][j]
      pixels[i*240 + j] = 0xff000000 | color;
    }
  }
}









