#include <stdio.h>
#include <stdlib.h>
#include <SDL.h> //use SDL library keyboard/display

//#include "chip8core.h"
//to do: implement so functions use struct, implement the actual display, implement each opcode (perhaps in different file for modularity)

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
  //use a struct?

  //setup input

  initialize(); //Reset system
  load_game(argv[1]); //Load game in from file given

  while (true) {
    //emulate one cycle
    emulate_cycle();
    //usleep(400);

    if ((chip8->V[15]) == 1) { //if draw flag set
      chip8->V[15] = 0;
      //drawGraphics()
    }

    setkeys();

    //add a way for users to quit program
  }
}

/********* SCREEN *******/

void putpixel();
int draw_screen();
int clear_screen();
int update_graphics();

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


void initialize_main() {
  pc = 0x200; // Start Program counter at 0x200
  opcode = 0;
  index_reg = 0;
  stack_ptr = 0;

  //Clear display
  for (int x = 0; x < 32; x++) {
    for (int y = 0; y < 64; y++) {
      graphics[x][y] = 0;
    }
  }

  //Clear stack
  for (int i = 0; i < 16; i++) {
    stack[i] = 0;
  }

  //Clear registers V0-VF
  for (int i = 0; i < 16; i++) {
    V[i] = 0;
  }

  //Clear memory
  for (int m = 0; m < 4096; m++) {
    memory[m] = 0;
  }

  //Clear hex key pad
  for (int i = 0; i < 16; i++) {
    key[i] = 0;
  }


  //load in fontset
  for (int i = 0; i < 80; ++i) {
    memory[i] = chip8_fontset[i];
  }
}

void clear_display();

void load_game(char *src) { //load the game
  FILE *game_file = fopen(src, "rb");
  fread(memory[0x200], 0xfff, 1, game_file);
  //doublecheck the way reading is done

  fclose(game_file)
}

void emulate_cycle() {
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

void update_timers() {  //update timers, count down 60Hz until 0
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

void setkeys() { //store key press state, press and release
  //resource used: https://www.libsdl.org/release/SDL-1.2.15/docs/html/guideinputkeyboard.html
  SDL_PollEvent(&event); //pool for event
  switch(event.type) {
    case SDL_KEYDOWN: //key press detected
      break;

    case: SDL_KEYUP: //key release detected
      for (int i = 0; i < 16; i++) {
        key[i] = 0; //clear key array
      }
      break;

    default:
      break;
  }

  switch(event.key.keysym.sym) { //read in keyboard events
    //hex keypad maping
    case: SDLK_UP:
      key[0] = 1;
    break;

    case: SDLK_DOWN:
      key[1] = 1;
    break;

    case: SDLK_LEFT:
      key[2] = 1;
    break;

    case: SDLK_RIGHT:
      key[3] = 1;
    break;

    case SDLK_1:
      key[4] = 1;
    break;

    case SDLK_2:
      key[5] = 1;
    break;

    case SDLK_3:
      key[6] = 1;
    break;

    case SDLK_4:
      key[7] = 1;
    break;

    case SDLK_5:
      key[8] = 1;
    break;

    case SDLK_6:
      key[9] = 1;
    break;

    case SDLK_q:
      key[10] = 1;
    break;

    case SDLK_w:
      key[11] = 1;
    break;

    case SDLK_e:
      key[12] = 1;
    break;

    case SDLK_r:
      key[13] = 1;
    break;

    case SDLK_t:
      key[14] = 1;
    break;

    case SDLK_y:
      key[15] = 1;
    break;

    case SDL_QUIT: // window close
      //implement quitting procedure
    break;

    default:
      break;
  }
}

