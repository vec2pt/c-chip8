#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chip8.h"

void chip8_init(chip8_t *chip8) {
  static const uint8_t chip8_fontset[80] = {
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

  memset(chip8, 0, sizeof(chip8_t));
  chip8->running = true;
  chip8->PC = CHIP8_ENTRY_POINT;
  chip8->sp = 0;
  memcpy(chip8->memory, chip8_fontset, sizeof(chip8_fontset));

  // Clear screen
  memset(chip8->framebuffer, false, sizeof(chip8->framebuffer));

  // Clear keypad
  memset(chip8->keypad, false, sizeof(chip8->keypad));

  // Set timers
  chip8->delay_timer = 4;
  chip8->sound_timer = 4;
}

bool chip8_load_rom(chip8_t *chip8, const char *rom_path) {
  FILE *f = fopen(rom_path, "rb");
  if (f == NULL) {
    perror("Error opening file");
    fclose(f);
    return false;
  }

  // Get/check rom size
  fseek(f, 0, SEEK_END);
  const size_t rom_size = ftell(f);
  const size_t max_size = CHIP8_MEMORY_SIZE - CHIP8_ENTRY_POINT;
  rewind(f);
  if (rom_size > max_size) {
    perror("Error opening file");
    fclose(f);
    return false;
  }

  fread(&chip8->memory[CHIP8_ENTRY_POINT], 1, rom_size, f);
  fclose(f);
  return true;
}

void chip8_draw(chip8_t *chip8, uint8_t X, uint8_t Y, uint8_t N) {
  uint8_t x = chip8->V[X] % CHIP8_SCREEN_W;
  uint8_t y = chip8->V[Y] % CHIP8_SCREEN_H;
  const uint8_t width = 8;

  chip8->V[0xF] = 0;
  for (uint8_t row = 0; row < N; row++) {
    const uint8_t sprite_byte = chip8->memory[chip8->I + row];
    uint16_t dst_y = y + row;
    for (uint8_t col = 0; col < width; col++) {
      uint8_t pixel = (sprite_byte >> (7 - col)) & 1;
      uint16_t dst_x = x + col;
      bool *screen_pixel = &chip8->framebuffer[dst_y * CHIP8_SCREEN_W + dst_x];

      if (*screen_pixel != pixel)
        chip8->V[0xF] = 1;
      *screen_pixel ^= pixel;
      if (++dst_x >= CHIP8_SCREEN_W)
        break;
    }
    if (++dst_y >= CHIP8_SCREEN_H)
      break;
  }
}

void chip8_tick_timers(chip8_t *chip8) {
  if (chip8->delay_timer > 0)
    chip8->delay_timer--;
  if (chip8->sound_timer > 0)
    chip8->sound_timer--;
}

void chip8_step(chip8_t *chip8) {
  chip8->draw_flag = false;

  uint16_t opcode =
      (chip8->memory[chip8->PC] << 8) | chip8->memory[chip8->PC + 1];
  chip8->PC += 2;

  uint16_t NNN = opcode & 0x0FFF;
  uint8_t NN = opcode & 0x00FF;
  uint8_t N = opcode & 0x000F;
  uint8_t X = (opcode >> 8) & 0x0F;
  uint8_t Y = (opcode >> 4) & 0x0F;

  switch ((uint8_t)(opcode >> 12) & 0x0F) {
  case 0x0:
    if (opcode == 0x00E0)
      memset(chip8->framebuffer, false, sizeof(chip8->framebuffer));
    else if (opcode == 0x00EE)
      chip8->PC = chip8->stack[--chip8->sp];
    else
      fprintf(stderr, "Error opcode: %x\n", opcode);
    break;
  case 0x1:
    chip8->PC = NNN;
    break;
  case 0x2:
    chip8->stack[chip8->sp++] = chip8->PC;
    chip8->PC = NNN;
    break;
  case 0x3:
    if (chip8->V[X] == NN)
      chip8->PC += 2;
    break;
  case 0x4:
    if (chip8->V[X] != NN)
      chip8->PC += 2;
    break;
  case 0x5:
    if (N != 0)
      break;
    if (chip8->V[X] == chip8->V[Y])
      chip8->PC += 2;
    break;
  case 0x6:
    chip8->V[X] = NN;
    break;
  case 0x7:
    chip8->V[X] += NN;
    break;
  case 0x8:
    switch (N) {
    case 0x0:
      chip8->V[X] = chip8->V[Y];
      break;
    case 0x1:
      chip8->V[X] |= chip8->V[Y];
      break;
    case 0x2:
      chip8->V[X] &= chip8->V[Y];
      break;
    case 0x3:
      chip8->V[X] ^= chip8->V[Y];
      break;
    case 0x4:
      chip8->V[0xF] = ((uint16_t)(chip8->V[X] + chip8->V[Y]) > 255);
      chip8->V[X] += chip8->V[Y];
      break;
    case 0x5:
      chip8->V[0xF] = (chip8->V[X] >= chip8->V[Y]);
      chip8->V[X] -= chip8->V[Y];
      break;
    case 0x6:
      chip8->V[0xF] = chip8->V[X] & 1;
      chip8->V[X] >>= 1;
      break;
    case 0x7:
      chip8->V[0xF] = (chip8->V[Y] >= chip8->V[X]);
      chip8->V[X] = chip8->V[Y] - chip8->V[X];
      break;
    case 0xE:
      chip8->V[0xF] = (chip8->V[X] & 0x80) >> 7;
      chip8->V[X] <<= 1;
      break;
    default:
      fprintf(stderr, "(0x8) Error opcode: %x\n", opcode);
      break;
    }
    break;
  case 0x9:
    if (chip8->V[X] != chip8->V[Y])
      chip8->PC += 2;
    break;
  case 0xA:
    chip8->I = NNN;
    break;
  case 0xB:
    chip8->PC = chip8->V[0x0] + NNN;
    break;
  case 0xC:
    chip8->V[X] = (rand() % 256) & NN;
    break;
  case 0xD:
    chip8_draw(chip8, X, Y, N);
    chip8->draw_flag = true;
    break;
  case 0xE:
    if (NN == 0x9E) {
      if (chip8->keypad[chip8->V[X]])
        chip8->PC += 2;
    } else if (NN == 0xA1) {
      if (!chip8->keypad[chip8->V[X]])
        chip8->PC += 2;
    } else
      fprintf(stderr, "Error opcode: %x\n", opcode);
    break;
  case 0xF:
    switch (NN) {
    case 0x07:
      chip8->V[X] = chip8->delay_timer;
      break;
    case 0x0A:
      bool key_pressed = false;
      for (uint8_t i = 0; i < 16; i++) {
        if (chip8->keypad[i]) {
          key_pressed = true;
          // TODO: Debug it
          // chip8->V[X] = i;
          break;
        }
      }
      if (!key_pressed)
        chip8->PC -= 2;
      break;
    case 0x15:
      chip8->delay_timer = chip8->V[X];
      break;
    case 0x18:
      chip8->sound_timer = chip8->V[X];
      break;
    case 0x1E:
      chip8->I += chip8->V[X];
      break;
    case 0x29:
      chip8->I = chip8->V[X] * 5;
      break;
    case 0x33:
      chip8->memory[chip8->I + 2] = (uint8_t)(chip8->V[X] % 10);
      chip8->memory[chip8->I + 1] = (uint8_t)(chip8->V[X] / 10) % 10;
      chip8->memory[chip8->I + 0] = (uint8_t)(chip8->V[X] / 100);
      break;
    case 0x55:
      for (uint8_t i = 0; i <= X; i++)
        chip8->memory[chip8->I + i] = chip8->V[i];
      break;
    case 0x65:
      for (uint8_t i = 0; i <= X; i++)
        chip8->V[i] = chip8->memory[chip8->I + i];
      break;
    default:
      fprintf(stderr, "(0xF) Error opcode: %x\n", opcode);
      break;
    }
    break;
  default:
    fprintf(stderr, "(Gen) Error opcode: %x\n", opcode);
    break;
  };
}
