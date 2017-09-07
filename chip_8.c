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

/*void emulate_cycle(Chip8System *chip8, Main_Display *display) {
  opcode = memory[chip8->pc] << 8 | memory[chip8->pc + 1]; //fetch
  pc += 2; //increment program counter after retrieving

  //add boolean for if opcode is found for troubleshooting

  //given opcode 0x1234
  //opcode & 0xF000 ; // would give 0x1000
  //opcode & 0x0F00 ; // would give 0x0200
  //opcode & 0x0F0F ; // would give 0x0204
  //opcode & 0x00FF ; // would give 0x0034

  //decode and execute
  switch(opcode & 0xF000) //check first four bits of opcode
  {
    case 0x0000:
      switch(opcode & 0x00FF)
      {
        case 0x00E0:
          //clear the screen
          clear_screen(display);
          chip8->pc = chip8->pc + 2;
        break;

        case 0x00EE:
          //return from subroutines
        break;

        default:
        printf("Unknown opcode [0x0000]: 0x%X\n", opcode);
      }
    break;

    case 0x1000:
      chip8->pc = opcode & 0x0FFF; //only interested in NNN of opcode 1NNN
    break;

    case 0x2000:
      chip8->pc = opcode &
      //Calls subroutine at NNN, *(0xNNN)()
    break;

    case 0x3000: //3XNN
      //if (Vx==NN)
      //{
      //  chip8->pc = chip8->pc + 4; //skips the next instruction if VX equals NN.
      //} else {
      //  chip8->pc = chip8->pc + 2;
      //}
    break;

    case 0x4000: //4XNN
      //if Vx!=NN)
    break;

    case 0x5000:
      //if(Vx==Vy)
      //skip next instruction if vx = vy
    break;

    case 0x6000:
      //set VX to NN
    break;

    case 0x7000:
      //add NN to VX
    break;

    case 0x8000:
      switch(opcode & 0x00FF) {
        //implement subopcodes

        default:
        printf("Unknown opcode [0x0000]: 0x%X\n", opcode);
      }
    break;

    case 0x9000:
      //skip next instruction is vx != vy
    break;

    case 0xa000: //ANNN: Sets I to the address NNN
      index_reg = opcode & 0x0FFF;
    break;

    case 0xb000:
      //jump to address NNN + V0
    break;

    case 0xc000:
      //Vx=rand()&NN
    break;

    case 0xd000:
      //draw(Vx,Vy,N)
    break;

    case: 0xe000:
      //subopcodes
      //extraswitch statement
    break;

    case 0xf000:
      //subopcodes
      //extraswitchstatement
    break;

    default:
      printf("Unknown opcode: 0x%X\n", opcode);
    break;
  }
}
*/

//credit "u/rascal999, need to double check"
int emulate_cycle(Chip8System *chip8, Main_Display *display)
{
   int opfound = 0;
   int debug = 0;
   int i, x, tmp;

   unsigned short xcoord = 0;
   unsigned short ycoord = 0;
   unsigned short height = 0;
   unsigned short pixel;

   /* Fetch */
   chip8->opcode = chip8->memory[chip8->pc] << 8 | chip8->memory[chip8->pc+1];

   if (debug == 1) printf("%x\n",chip8->opcode);

   switch(chip8->opcode & 0xF000)
   {
      case 0x1000:
         chip8->pc=chip8->opcode & 0x0FFF;

         if (debug == 1) printf("pc = %x\n",chip8->opcode & 0x0FFF);
         opfound = 1;
      break;

      case 0xA000: /* Checked */
         chip8->I = chip8->opcode & 0x0FFF;

         if (debug == 1) printf("I = %x\n", chip8->opcode & 0x0FFF);
         chip8->pc = chip8->pc + 2;
         opfound = 1;
      break;

      case 0x4000:
         if (chip8->V[(chip8->opcode & 0x0F00) >> 8] != (chip8->opcode & 0x00FF))
         {
            chip8->pc = chip8->pc + 4;
         } else {
            chip8->pc = chip8->pc + 2;
         }

         if (debug == 1) printf("pc = %x\n",chip8->pc);
         opfound = 1;
      break;

      /* 4XNN - Skips the next instruction if VX doesn't equal NN. */

      case 0xC000:
         /* 5 should be a random number */
         chip8->V[(chip8->opcode & 0x0F00) >> 8] = 9 & (chip8->opcode & 0x00FF);

         if (debug == 1) printf("V[%x] = %x",(chip8->opcode & 0x0F00) >> 8,9 & (chip8->opcode & 0x00FF));
         chip8->pc = chip8->pc + 2;
         opfound = 1;
      break;

      /* Cxkk - RND Vx, byte
      Set Vx = random byte AND kk.

      The interpreter generates a random number from 0 to 255, which is then ANDed with the value kk. The results are stored in Vx. See instruction 8xy2 for more information on AND. */

      case 0x6000: /* Checked */
         chip8->V[(chip8->opcode & 0x0F00) >> 8] = chip8->opcode & 0x00FF;

         if (debug == 1) printf("V[%x] = %x\n", (chip8->opcode & 0x0F00) >> 8, chip8->opcode & 0x00FF);
         chip8->pc = chip8->pc + 2;
         opfound = 1;
      break;

      case 0xD000:
         chip8->V[0xF] = 0;
         height = chip8->opcode & 0x000F;
         xcoord = chip8->V[(chip8->opcode & 0x0F00) >> 8];
         ycoord = chip8->V[(chip8->opcode & 0x00F0) >> 4];

         for (i=0;i<height;i++)
         {
            pixel = chip8->memory[chip8->I + i];
            //printf("sprite %x\n",pixel);
            for (x=0;x<8;x++)
            {
               if ((pixel & (0x80 >> x)) != 0)
               {
                  //printf("x %d y %d px %x\n",xcoord,ycoord,pixel);
                  //printf("%x %d %d\n",chip8->opcode,xcoord,ycoord);
                  if (chip8->gfx[xcoord+x][ycoord+i] == 1) chip8->V[0xF] = 1;
                  chip8->gfx[xcoord+x][ycoord+i] ^= 1;
                }
            }
         }

         chip8->DrawFlag = 1;

         if (debug == 1) printf("Draw call %x\n",chip8->opcode);
         chip8->pc = chip8->pc + 2;
         opfound = 1;
      break;

      case 0x2000: /* Checked */
         chip8->stack[chip8->sp] = chip8->pc;
         chip8->sp++;
         chip8->pc = chip8->opcode & 0x0FFF;

         if (debug == 1) printf("pc = %x\n", chip8->opcode & 0x0FFF);
         opfound = 1;
      break;

      case 0x3000:
         if (chip8->V[(chip8->opcode & 0x0F00) >> 8] == (chip8->opcode & 0x00FF))
         {
            chip8->pc = chip8->pc + 4;
         } else {
            chip8->pc = chip8->pc + 2;
         }

         if (debug == 1) printf("V[%x] = %x\n",(chip8->opcode & 0x0F00) >> 8,chip8->V[(chip8->opcode & 0x0F00) >> 8]);
         opfound = 1;
      break;

      /* 3XNN - Skips the next instruction if VX equals NN. */

      case 0x7000: /* Checked */
         chip8->V[(chip8->opcode & 0x0F00) >> 8] = chip8->V[(chip8->opcode & 0x0F00) >> 8] + (chip8->opcode & 0x00FF);

         if (debug == 1) printf("V[%x] = %x\n", (chip8->opcode & 0x0F00) >> 8,chip8->V[(chip8->opcode & 0x00FF) >> 8] + (chip8->opcode & 0x00FF));
         chip8->pc = chip8->pc + 2;
         opfound = 1;
      break;
   }

   if (opfound == 1)
   {
      DecrementTimers(chip8);
      return 0;
   }

   switch(chip8->opcode & 0xF0FF)
   {
      case 0xF00A:
         for(i=0;i<16;i++)
         {
            if (chip8->key[i] != 0)
            {
               chip8->V[(chip8->opcode & 0x0F00) >> 8] = chip8->key[i];
               chip8->pc = chip8->pc + 2;
            }
         }
         opfound = 1;
      break;

      /* FX0A - A key press is awaited, and then stored in VX. */

      case 0xF01E:
         chip8->I = chip8->I + chip8->V[(chip8->opcode & 0x0F00) >> 8];

         if (debug == 1) printf("I = %x\n",chip8->I);
         chip8->pc = chip8->pc + 2;
         opfound = 1;
      break;

      /* Fx1E - ADD I, Vx
      Set I = I + Vx.

      The values of I and Vx are added, and the results are stored in I. */

      case 0xF018:
         chip8->sound_timer = chip8->V[(chip8->opcode & 0x0F00) >> 8];

         if (debug == 1) printf("sound_timer = %x\n",chip8->V[(chip8->opcode & 0x0F00) >> 8]);
         chip8->pc = chip8->pc + 2;
         opfound = 1;
      break;

      /* FX18 - Sets the sound timer to VX. */

      case 0xF033:
         chip8->memory[chip8->I] = chip8->V[(chip8->opcode & 0x0F00) >> 8] / 100;
         chip8->memory[chip8->I + 1] = (chip8->V[(chip8->opcode & 0x0F00) >> 8] / 10) % 10;
         chip8->memory[chip8->I + 2] = (chip8->V[(chip8->opcode & 0x0F00) >> 8] / 1) % 10;

         if (debug == 1)
         {
            printf("Mem[%x] = %x\n",chip8->I, chip8->V[(chip8->opcode & 0x0F00) >> 8] / 100);
            printf("Mem[%x] = %x\n",chip8->I + 1, (chip8->V[(chip8->opcode & 0x0F00) >> 8] / 10) % 10);
            printf("Mem[%x] = %x\n",chip8->I + 2, (chip8->V[(chip8->opcode & 0x0F00) >> 8] / 10) % 1);
            printf("I,I+1,I+2 = %d%d%d\n",chip8->memory[chip8->I],chip8->memory[chip8->I+1],chip8->memory[chip8->I+2]);
         }
         chip8->pc = chip8->pc + 2;
         opfound = 1;
      break;

      case 0xE09E:
         if (chip8->key[chip8->V[(chip8->opcode & 0x0F00) >> 8]] != 0)
         {
            chip8->pc = chip8->pc + 4;
         } else {
            chip8->pc = chip8->pc + 2;
         }

         if (debug == 1) printf("pc = %x\n",chip8->pc);
         opfound = 1;
      break;

      /* Ex9E - SKP Vx
      Skip next instruction if key with the value of Vx is pressed.

      Checks the keyboard, and if the key corresponding to the value of Vx is currently in the down position, PC is increased by 2. */

      case 0xE0A1:
         if (chip8->key[chip8->V[(chip8->opcode & 0x0F00) >> 8]] != 1)
         {
            chip8->pc = chip8->pc + 4;
         } else {
            chip8->pc = chip8->pc + 2;
         }

         if (debug == 1) printf("key[%x] = %x\n",chip8->V[(chip8->opcode & 0x0F00) >> 8],chip8->key[chip8->V[(chip8->opcode & 0x0F00) >> 8]]);
         opfound = 1;
      break;

      /* ExA1 - SKNP Vx
      Skip next instruction if key with the value of Vx is not pressed.

      Checks the keyboard, and if the key corresponding to the value of Vx is currently in the up position, PC is increased by 2. */

      case 0xF007:
         chip8->V[(chip8->opcode & 0x0F00) >> 8] = chip8->delay_timer;

         if (debug == 1) printf("V[%x] = %x\n",(chip8->opcode & 0x0F00) >> 8,chip8->delay_timer);
         chip8->pc = chip8->pc + 2;
         opfound = 1;
      break;

      /* FX07 - Sets VX to the value of the delay timer. */

      case 0xF015:
         chip8->delay_timer = chip8->V[(chip8->opcode & 0x0F00) >> 8];

         if (debug == 1) printf("delay_timer = %x\n",chip8->V[(chip8->opcode & 0x0F00) >> 8]);
         chip8->pc = chip8->pc + 2;
         opfound = 1;
      break;

      /* FX15 - Sets the delay timer to VX. */

      case 0xF065:
         for(i=0;i<=((chip8->opcode & 0x0F00) >> 8);i++)
         {
            chip8->V[i] = chip8->memory[chip8->I + i];

            if (debug == 1) printf("V[%x] is %x\n",i,chip8->memory[chip8->I + i]);
            opfound = 1;
         }
         chip8->pc = chip8->pc + 2;
      break;

      case 0xF029:
         /* chip8->I = chip8->memory[chip8->V[(chip8->opcode & 0x0F00) >> 8]*5]; -- The great bug */
         chip8->I = chip8->V[(chip8->opcode & 0x0F00) >> 8]*5;

         if (debug == 1) printf("I is %x\n",chip8->I);
         chip8->pc = chip8->pc + 2;
         opfound = 1;
      break;

      /* FX29 - Sets I to the location of the sprite for the character in VX. Characters 0-F (in hexadecimal) are represented by a 4x5 font. */

      case 0xF055:
         for(i=0;i<(chip8->opcode & 0x0F00) >> 8;i++)
         {
            /* unsigned short * ir = chip8->I+i;
            unsigned short * ir;
            ir = (unsigned short *) chip8->V[i]; */
            chip8->memory[chip8->I+i] = chip8->V[(chip8->opcode & 0x0F00) >> 8];

            if (debug == 1) printf("V[%x] = %x\n",i,chip8->I+i);
         }
         chip8->pc = chip8->pc + 2;
         opfound = 1;
      break;

      /* FX55 - Stores V0 to VX in memory starting at address I.[4] */
   }

   if (opfound == 1)
   {
      DecrementTimers(chip8);
      return 0;
   }

   switch(chip8->opcode & 0x00FF)
   {
      case 0x00E0:
         ClearDisplay(display);
         chip8->pc = chip8->pc + 2;
         opfound = 1;
      break;

      case 0x00EE:
         chip8->sp=chip8->sp - 1;
         chip8->pc = chip8->stack[chip8->sp];

         if (debug == 1) printf("pc = %x\n", chip8->opcode & 0x0FFF);
         chip8->pc = chip8->pc + 2;
         opfound = 1;
      break;
   }

   if (opfound == 1)
   {
      DecrementTimers(chip8);
      return 0;
   }

   switch(chip8->opcode & 0xF00F)
   {
      case 0x8000: /* 0x8XY0 */
         chip8->V[(chip8->opcode & 0x0F00) >> 8] = chip8->V[(chip8->opcode & 0x00F0) >> 4];

         if (debug == 1) printf("V[%x] = %x\n",(chip8->opcode & 0x0F00) >> 8,chip8->V[(chip8->opcode & 0x0F00) >> 8]);
         chip8->pc = chip8->pc + 2;
         opfound = 1;
      break;

      /* 8XY0 - Sets VX to the value of VY. */

      case 0x8002:
         chip8->V[(chip8->opcode & 0x0F00) >> 8] = chip8->V[(chip8->opcode & 0x0F00) >> 8] & chip8->V[(chip8->opcode & 0x00F0) >> 4];

         if (debug == 1) printf("V[%x] = %x\n",(chip8->opcode & 0x0F00) >> 8,chip8->V[(chip8->opcode & 0x0F00) >> 8]);
         chip8->pc = chip8->pc + 2;
         opfound = 1;
      break;

      /* 8XY2 - Sets VX to VX and VY. */

      case 0x8003:
         chip8->V[(chip8->opcode & 0x0F00) >> 8] = chip8->V[(chip8->opcode & 0x0F00) >> 8] ^ chip8->V[(chip8->opcode & 0x00F0) >> 4];

         if (debug == 1) printf("V[%x] = %x\n",(chip8->opcode & 0x0F00) >> 8,chip8->V[(chip8->opcode & 0x0F00) >> 8]);
         chip8->pc = chip8->pc + 2;
         opfound = 1;
      break;

      /* 8xy3 - XOR Vx, Vy
      Set Vx = Vx XOR Vy.

      Performs a bitwise exclusive OR on the values of Vx and Vy, then stores the result in Vx. An exclusive OR compares the corrseponding bits from two values, and if the bits are not both the same, then the corresponding bit in the result is set to 1. Otherwise, it is 0. */

      case 0x8004:
         tmp = chip8->V[(chip8->opcode & 0x0F00) >> 8];
         chip8->V[(chip8->opcode & 0x0F00) >> 8] = chip8->V[(chip8->opcode & 0x0F00) >> 8] + chip8->V[(chip8->opcode & 0x00F0) >> 4];

         if ((tmp + chip8->V[(chip8->opcode & 0x00F0) >> 4]) > 255)
         {
            chip8->V[0xF] = 1;
         } else {
            chip8->V[0xF] = 0;
         }

         if (debug == 1) printf("V[%x] = %x. V[F] = %x\n",(chip8->opcode & 0x0F00) >> 8,chip8->V[(chip8->opcode & 0x0F00) >> 8],chip8->V[0xF]);

         chip8->pc = chip8->pc + 2;
         opfound = 1;
      break;

      /* 8xy4 - ADD Vx, Vy
      Set Vx = Vx + Vy, set VF = carry.

      The values of Vx and Vy are added together. If the result is greater than 8 bits (i.e., > 255,) VF is set to 1, otherwise 0. Only the lowest 8 bits of the result are kept, and stored in Vx. */

      /* 8XY4    Adds VY to VX. VF is set to 1 when there's a carry, and to 0 when there isn't. */

      case 0x8005:
         if (chip8->V[(chip8->opcode & 0x0F00) >> 8] > chip8->V[(chip8->opcode & 0x00F0) >> 4])
         {
            chip8->V[0xF] = 1;
         } else {
            chip8->V[0xF] = 0;
         }
         chip8->V[(chip8->opcode & 0x0F00) >> 8] = chip8->V[(chip8->opcode & 0x0F00) >> 8] - chip8->V[(chip8->opcode & 0x00F0) >> 4];

         if (debug == 1) printf("V[%x] = %x. V[0xF] = %x\n",(chip8->opcode & 0x0F00) >> 8,chip8->V[(chip8->opcode & 0x0F00) >> 8],chip8->V[0xF]);
         chip8->pc = chip8->pc + 2;
         opfound = 1;
      break;

      /* 8xy5 - SUB Vx, Vy
      Set Vx = Vx - Vy, set VF = NOT borrow.

      If Vx > Vy, then VF is set to 1, otherwise 0. Then Vy is subtracted from Vx, and the results stored in Vx. */

      /* 8XY5    VY is subtracted from VX. VF is set to 0 when there's a borrow, and 1 when there isn't. */

      case 0x800E:
         if ((chip8->V[(chip8->opcode & 0x0F00) >> 8] >> 8) == 1)
         {
            chip8->V[0xF] = 1;
         } else {
            chip8->V[0xF] = 0;
         }
         chip8->V[(chip8->opcode & 0x0F00) >> 8] = chip8->V[(chip8->opcode & 0x0F00) >> 8] * 2;

         if (debug == 1) printf("V[%x] = %x. V[0xF] = %x\n",(chip8->opcode & 0x0F00) >> 8,chip8->V[(chip8->opcode & 0x0F00) >> 8],chip8->V[0xF]);
         chip8->pc = chip8->pc + 2;
         opfound = 1;
      break;

      /* 8xyE - SHL Vx {, Vy}
      Set Vx = Vx SHL 1.

      If the most-significant bit of Vx is 1, then VF is set to 1, otherwise to 0. Then Vx is multiplied by 2. */

      case 0x9000:
         if (chip8->V[(chip8->opcode & 0x0F00) >> 8] != chip8->V[(chip8->opcode & 0x00F0) >> 4])
         {
            chip8->pc = chip8->pc + 4;
         } else {
            chip8->pc = chip8->pc + 2;
         }

         if (debug == 1) printf("pc = %x\n",chip8->pc);
         opfound = 1;
      break;


      /* 9XY0 - Skips the next instruction if VX doesn't equal VY. */
   }

   if(chip8->delay_timer > 0)
   {
      chip8->delay_timer=chip8->delay_timer - 1;
   }

   if(chip8->sound_timer > 0)
   {
      if(chip8->sound_timer == 1)
      printf("BEEP!\n");
      chip8->sound_timer=chip8->sound_timer - 1;
   }

   if (opfound == 0)
   {
      printf("%x not found.\n",chip8->opcode);
      exiterror(20);
   }

   /*
      More accurate and complete instruction set (and general overview of CHIP8) available at http://devernay.free.fr/hacks/chip8/C8TECH10.HTM

      0NNN - Calls RCA 1802 program at address NNN.
      00E0 - Clears the screen.
      00EE - Returns from a subroutine.
      1NNN - Jumps to address NNN.
      2NNN - Calls subroutine at NNN.
      3XNN - Skips the next instruction if VX equals NN.
      4XNN - Skips the next instruction if VX doesn't equal NN.
      5XY0 - Skips the next instruction if VX equals VY.
      6XNN - Sets VX to NN.
      7XNN - Adds NN to VX.
      8XY0 - Sets VX to the value of VY.
      8XY1 - Sets VX to VX or VY.
      8XY2 - Sets VX to VX and VY.
      8XY3 - Sets VX to VX xor VY.
      8XY4 - Adds VY to VX. VF is set to 1 when there's a carry, and to 0 when there isn't.
      8XY5 - VY is subtracted from VX. VF is set to 0 when there's a borrow, and 1 when there isn't.
      8XY6 - Shifts VX right by one. VF is set to the value of the least significant bit of VX before the shift.[2]
      8XY7 - Sets VX to VY minus VX. VF is set to 0 when there's a borrow, and 1 when there isn't.
      8XYE - Shifts VX left by one. VF is set to the value of the most significant bit of VX before the shift.[2]
      9XY0 - Skips the next instruction if VX doesn't equal VY.
      ANNN - Sets I to the address NNN.
      BNNN - Jumps to the address NNN plus V0.
      CXNN - Sets VX to a random number and NN.
      DXYN - Draws a sprite at coordinate (VX, VY) that has a width of 8 pixels and a height of N pixels. Each row of 8 pixels is read as bit-coded (with the most significant bit of each byte displayed on the left) starting from memory location I; I value doesn't change after the execution of this instruction. As described above, VF is set to 1 if any screen pixels are flipped from set to unset when the sprite is drawn, and to 0 if that doesn't happen.
      EX9E - Skips the next instruction if the key stored in VX is pressed.
      EXA1 - Skips the next instruction if the key stored in VX isn't pressed.
      FX07 - Sets VX to the value of the delay timer.
      FX0A - A key press is awaited, and then stored in VX.
      FX15 - Sets the delay timer to VX.
      FX18 - Sets the sound timer to VX.
      FX1E - Adds VX to I.[3]
      FX29 - Sets I to the location of the sprite for the character in VX. Characters 0-F (in hexadecimal) are represented by a 4x5 font.
      FX33 - Stores the Binary-coded decimal representation of VX, with the most significant of three digits at the address in I, the middle digit at I plus 1, and the least significant digit at I plus 2. (In other words, take the decimal representation of VX, place the hundreds digit in memory at location in I, the tens digit at location I+1, and the ones digit at location I+2.)
      FX55 - Stores V0 to VX in memory starting at address I.[4]
      FX65 - Fills V0 to VX with values from memory starting at address I.[4]
   */

   /* Decode */

   /* Execute */
   return 0;
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

