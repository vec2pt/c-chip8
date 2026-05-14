#pragma once
#include <stdbool.h>
#include <stdint.h>

#define CHIP8_SCREEN_W 64
#define CHIP8_SCREEN_H 32

#define CHIP8_MEMORY_SIZE 4096
#define CHIP8_ENTRY_POINT 0x200

typedef struct {
  /* Memory */
  uint8_t memory[CHIP8_MEMORY_SIZE];

  /* CPU */
  uint16_t PC;
  uint16_t I;
  uint16_t stack[16];
  uint8_t sp;
  uint8_t V[16];

  /* Timers */
  uint8_t delay_timer;
  uint8_t sound_timer;

  /* Display */
  bool framebuffer[CHIP8_SCREEN_W * CHIP8_SCREEN_H];
  bool draw_flag;

  /* Input */
  bool keypad[16];

  /* State */
  bool running;
} chip8_t;

void chip8_init(chip8_t *chip8);
bool chip8_load_rom(chip8_t *chip8, const char *path);
void chip8_draw(chip8_t *chip8, uint8_t X, uint8_t Y, uint8_t N);
void chip8_tick_timers(chip8_t *chip8);
void chip8_step(chip8_t *chip8);
// void chip8_set_key(chip8_t *chip8, int key, bool pressed);
