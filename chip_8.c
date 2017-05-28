#include <stdio.h>
#include <stdlib.h>
#include <SDL.h> //use SDL library keyboard/display
//#include "chip8core.h"
//to do: implement each opcode

typedef struct {
  SDL_Surface *screen;
  SDL_Event event;
  int key;
  int x;
  int y;
  //int z;
} Main_Display;

typedef struct {
  unsigned short opcode; //opcodes two bytes long
  unsigned char memory[4096]; //4K memory total, malloc?

  unsigned char V[16]; //general purpose registers
  unsigned short index_reg; //index register
  unsigned short pc; //program counter
  unsigned short stack[16]; //16 levels of stack
  unsigned short stack_ptr; //stack pointer

  unsigned char graphics[64][32]; //multidim array of 1 or 0 pixel states for screen (2048)
  unsigned char delay_timer;
  unsigned char sound_timer;

  unsigned char key[16]; //hex keypad, array to store state
} Chip8System;

unsigned char chip8_fontset[80] = //each 4px wide & 5px high
{
  0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
  0x20, 0x60, 0x20, 0x20, 0x70, // 1
  0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
  0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
  0x90, 0x90, 0xF0, 0x10, 0x10, // 4
  0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
  0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
  0xF0, 0x10, 0x20, 0x40, 0x40, // 7
  0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
  0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
  0xF0, 0x90, 0xF0, 0x90, 0x90, // A
  0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
  0xF0, 0x80, 0x80, 0x80, 0xF0, // C
  0xE0, 0x90, 0x90, 0x90, 0xE0, // D
  0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
  0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

int main(int argc, char **argv) {
  Chip8System chip8; //declare Chip8 struct

  //setup graphics
  SDL_Surface *screen;
  SDL_Event event;

  initialize_screen(screen);
  initialize_main(chip8); //Reset system

  if ((argc != 2)) { // If the argument count is invalid
        printf("Usage: chip8 GAME_FILE\n");
        return 1;
  }

  load_game(argv[1]); //Load game in from file given

  while (true) {  //add ability to quit, variable, SDL_KEY input "esc"
    //emulate one cycle
    emulate_cycle(chip8, screen);
    //usleep(400);

    if ((chip8->V[15]) == 1) { //if draw flag set
      chip8->V[15] = 0;
      update_graphics(chip8, screen);
    }

    setkeys(chip8);
  }

  return 0;
}

/********* SCREEN ******
credit for SDL implementation code and screen functions to github user rascal999
and resource: https://www.libsdl.org/release/SDL-1.2.15/docs/html/guidevideo.html
*/

void putpixel(Main_Display *display, int x, int y, Uint8 r, Uint8 g, Uint8 b) {
  /* Here p is the address to the pixel we want to set */
  Uint32 *p;
  Uint32 pixel;

  pixel = SDL_MapRGB(display->screen->format, r, g, b);

  p = (Uint32*) display->screen->pixels + y + x;
  *p = pixel;
}

int draw_screen(Main_Display *display, int x, int y, int c) {
  int ytimesw;
  x = x * 10;
  y = y * 10;

  (c == 1) ? (c = 128): c = 0;

  if (SDL_MUSTLOCK(display->screen)) {
    if (SDL_LockSurface(display->screen) == -1) {
      return 1;
    }
  }

  for (int blocky = 0; blocky < 10; blocky++) {
    ytimesw = y * display->screen->pitch/4;
    for (int blockx = 0; blockx < 10; blockx++) {
      putpixel(display, blockx + x, (blocky*(display->screen->pitch/4)) \
        + ytimesw, c,c,c)
    }
  }
}

int clear_screen(Main_Display *display) {
  for (int x = 0; x < 32; x++) {
    for (int y = 0; y < 64; y++) {
      draw_screen(display, x, y, 0);
    }
  }

  if (SDL_MUSTLOCK(display->screen)) {
    SDL_UnlockSurface(display->screen);
  }

  SDL_Flip(display->screen);
}

int update_graphics(Chip8System *chip8, Main_Display *display) {
  clear_display(display);

  for (int x = 0; x < 32; x++) {
    for (int y = 0; y < 64; y++) {
      if(chip8->graphics[x][y] != 0) {
        draw_screen(display, x, y, 1);
      }
    }
  }

  if (SDL_MUSTLOCK(display->screen)) {
    SDL_UnlockSurface(display->screen);
  }

  SDL_Flip(display->screen);
}

int initialize_screen(Main_Display *display) {
  if ((SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO) == -1)) {
    printf("Could not initialize SDL: %s.\n", SDL_GetError());
    exit(-1);
  }
  /*
   * Initialize the display in a 640x320 32-bit palettized mode,
   * requesting a software surface
   */
  display->screen = SDL_SetVideoMode(640, 320, 32, SDL_SWSURFACE);
  //use a struct
  if (display->screen == NULL) {
      fprintf(stderr, "Couldn't set 640x320x32 video mode: %s\n",
                      SDL_GetError());
      exit(1);
  }
}

/****************/

void initialize_main(Chip8System *chip8) {
  chip8->pc = 0x200; // Start Program counter at 0x200
  chip8->opcode = 0;
  chip8->index_reg = 0;
  chip8->stack_ptr = 0;

  //Reset timers
  chip8->delay_timer = 0;
  chip8->sound_timer = 0;

  //Clear display
  for (int x = 0; x < 32; x++) {
    for (int y = 0; y < 64; y++) {
      chip8->graphics[x][y] = 0;
    }
  }

  //Clear stack
  for (int i = 0; i < 16; i++) {
    chip8->stack[i] = 0;
  }

  //Clear registers V0-VF
  for (int i = 0; i < 16; i++) {
    chip8->V[i] = 0;
  }

  //Clear memory
  for (int m = 0; m < 4096; m++) {
    chip8->memory[m] = 0;
  }

  //Clear hex key pad
  for (int i = 0; i < 16; i++) {
    chip8->key[i] = 0;
  }

  //load in fontset
  for (int i = 0; i < 80; ++i) {
    chip8->memory[i] = chip8_fontset[i];
  }
}

void load_game(char *src) { //load the game
  FILE *game_file = fopen(src, "rb");
  fread(memory[0x200], 0xfff, 1, game_file);
  //double check the way reading is done, will need to loop

  fclose(game_file)
}

void emulate_cycle(Chip8System *chip8, Main_Display *display) {
  opcode = memory[pc] << 8 | memory[pc + 1]; //fetch
  pc += 2; //increment program counter after retrieving

  //decode and execute
  switch(opcode & 0xF000) //check first four bits of opcode
  {
    case 0x0000:
      switch(opcode & 0x000F)
      {
        case 0x00E0:
          //clear the screen
        break;

        case 0x00E:
          //return from subroutines
        break;

        default:
        printf("Unknown opcode [0x0000]: 0x%X\n", opcode);
      }
    break;

    case 0x1000:
      pc = opcode & 0x0FFF; //only interested in NNN of opcode 1NNN
    break;

    case 0xa00: //ANNN: Sets I to the address NNN
      index_reg = opcode & 0x0FFF;
    break;

    //more opcodes

    default:
      printf("Unknown opcode: 0x%X\n", opcode);
    break;
  }
}

void update_timers(Chip8System *chip8) {  //update timers, count down 60Hz until 0
  if(delay_timer > 0) {
    delay_timer--;
  }

  if(sound_timer > 0) {
    if(sound_timer == 1){
      //make beeping sound
      sound_timer--;
    }
  }
}

void setkeys(Chip8System *chip8) { //store key press state, press and release
  //resource used: https://www.libsdl.org/release/SDL-1.2.15/docs/html/guideinputkeyboard.html
  SDL_PollEvent(&event); //pool for event
  switch(event.type) {
    case SDL_KEYDOWN: //key press detected
      break;

    case: SDL_KEYUP: //key release detected
      for (int i = 0; i < 16; i++) {
        chip8->key[i] = 0; //clear key array
      }
      break;

    default:
      break;
  }

  switch(event.key.keysym.sym) { //read in keyboard events
    //hex keypad maping
    case: SDLK_UP:
      chip8->key[0] = 1;
    break;

    case: SDLK_DOWN:
      chip8->key[1] = 1;
    break;

    case: SDLK_LEFT:
      chip8->key[2] = 1;
    break;

    case: SDLK_RIGHT:
      chip8->key[3] = 1;
    break;

    case SDLK_1:
      chip8->key[4] = 1;
    break;

    case SDLK_2:
      chip8->key[5] = 1;
    break;

    case SDLK_3:
      chip8->key[6] = 1;
    break;

    case SDLK_4:
      chip8->key[7] = 1;
    break;

    case SDLK_5:
      chip8->key[8] = 1;
    break;

    case SDLK_6:
      chip8->key[9] = 1;
    break;

    case SDLK_q:
      chip8->key[10] = 1;
    break;

    case SDLK_w:
      chip8->key[11] = 1;
    break;

    case SDLK_e:
      chip8->key[12] = 1;
    break;

    case SDLK_r:
      chip8->key[13] = 1;
    break;

    case SDLK_t:
      chip8->key[14] = 1;
    break;

    case SDLK_y:
      chip8->key[15] = 1;
    break;

    case SDL_QUIT: // window close
      //implement quitting procedure
    break;

    default:
      break;
  }
}

