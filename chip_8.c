// #include graphics/input
#include <stdio.h>
#include <stdlib.h>

//#include "chip8core.h"

int main(int argc, char **argv) {

  unsigned short opcode; //opcodes two bytes long
  unsigned char memory[4096]; //4K memory total, malloc?

  unsigned char V[16]; //general purpose registers
  unsigned short index_reg; //index register
  unsigned short pc; //program counter
  unsigned short stack[16]; //16 levels of stack
  unsigned short stack_ptr; //stack pointer

  unsigned char graphics[64 * 32]; //array of 1 or 0 pixel states for screen (2048)
  unsigned char delay_timer;
  unsigned char sound_timer;

  unsigned char key[16]; //hex keypad, array to store state

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

  //setup graphics
  //setup input

  initialize(); //Reset system
  load_game(argv[1]); //Load game in from file given

  while (true) {
    //emulate one cycle
    emulate_cycle();

    //if (mychip8.drawflag) //if draw flag set
      //drawGraphics()

    setkeys();
    return 0;
  }
}

void initialize() {
  pc = 0x200; // Start Program counter at 0x200
  opcode = 0;
  index_reg = 0;
  stack_ptr = 0;

  //Clear display
  //Clear stack
  //Clear registers V0-VF
  //Clear memory

  for (int i = 0; i < 80; ++i) { //load in fontset
    memory[i] = chip8_fontset[i];
  }
}

void load_game(char *src) {
  FILE *game_file = fopen(src, "rb");
  fread(memory[0x200], 0xfff, 1, game_file);
  fclose(game_file)
}

void emulate_cycle() {
  opcode = memory[pc] << 8 | memory[pc + 1]; //fetch
  pc += 2; //increment program counter

  //decode and execute
  switch(opcode & 0xF000) //check first four bits of opcode
  {
    case 0x0000:
      switch(opcode & 0x000F)
      {
        case 0x0000:
          //clear the screen
        break;

        case 0x00E:
          //return from subroutines
        break;

        default:
        printf("Unknown opcode [0x0000]: 0x%X\n", opcode);
      }
    break;

    case 0xa00: //ANNN: Sets I to the address NNN
      index_reg = opcode & 0x0FFF;
    break;

    //more opcodes

    default:
      printf("Unknown opcode: 0x%X\n", opcode);
  }

  //update timers, count down 60Hz until 0
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
  break;
}

