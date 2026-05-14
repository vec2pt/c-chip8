#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "chip8.h"

typedef struct {
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *texture;
  SDL_Event event;
  SDL_AudioStream *stream;
} sdl_t;

typedef struct {
  unsigned int sdl_key;
  uint8_t chip8_key;
} keymap_t;

bool sdl_init(sdl_t *sdl);
void SDLCALL _audio_callback(void *userdata, SDL_AudioStream *stream,
                             int additional_amount, int total_amount);
void sdl_handle_event(sdl_t *sdl, chip8_t *chip8);
void _set_key_state(chip8_t *chip8, unsigned int sdl_key, bool state);
void sdl_draw(sdl_t *sdl, const chip8_t chip8);
void sdl_quit(sdl_t *sdl);

const keymap_t keymap[] = {
    {SDLK_1, 0x1}, {SDLK_2, 0x2}, {SDLK_3, 0x3}, {SDLK_4, 0xC},
    {SDLK_Q, 0x4}, {SDLK_W, 0x5}, {SDLK_E, 0x6}, {SDLK_R, 0xD},
    {SDLK_A, 0x7}, {SDLK_S, 0x8}, {SDLK_D, 0x9}, {SDLK_F, 0xE},
    {SDLK_Z, 0xA}, {SDLK_X, 0x0}, {SDLK_C, 0xB}, {SDLK_V, 0xF},
};

bool sdl_init(sdl_t *sdl) {
  const uint8_t scale = 20;

  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error initializing SDL3: %s\n",
                 SDL_GetError());
    return false;
  }

  sdl->window = SDL_CreateWindow("Chip8", CHIP8_SCREEN_W * scale,
                                 CHIP8_SCREEN_H * scale, 0);
  if (!sdl->window) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error create Window: %s\n",
                 SDL_GetError());
    return false;
  }

  sdl->renderer = SDL_CreateRenderer(sdl->window, NULL);
  if (!sdl->renderer) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error create Renderer: %s\n",
                 SDL_GetError());
    return false;
  }

  sdl->texture = SDL_CreateTexture(sdl->renderer, SDL_PIXELFORMAT_RGB24,
                                   SDL_TEXTUREACCESS_TARGET, CHIP8_SCREEN_W,
                                   CHIP8_SCREEN_H);
  if (!sdl->texture) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error create texture: %s\n",
                 SDL_GetError());
    return false;
  }

  if (!SDL_SetTextureScaleMode(sdl->texture, SDL_SCALEMODE_NEAREST)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Error set scale mode for Texture: %s\n", SDL_GetError());
    return false;
  }

  SDL_AudioSpec spec;
  spec.channels = 1;
  spec.format = SDL_AUDIO_S16;
  spec.freq = 44100;
  sdl->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                          &spec, _audio_callback, NULL);
  if (!sdl->stream) {
    SDL_Log("Error create audio stream: %s", SDL_GetError());
    return false;
  }
  // SDL_ResumeAudioStreamDevice(sdl->stream);
  return true;
}

void SDLCALL _audio_callback(void *userdata, SDL_AudioStream *stream,
                             int additional_amount, int total_amount) {
  additional_amount /= sizeof(int16_t);

  static uint32_t running_sample_index = 0;
  const uint32_t freq = 440;
  const uint32_t audio_sample_rate = 44100;
  const int16_t volume = 3000;

  const uint32_t square_wave_period = audio_sample_rate / freq;
  const uint32_t half_square_wave_period = square_wave_period / 2;
  while (additional_amount > 0) {
    int16_t samples[128];
    const int total = SDL_min(additional_amount, SDL_arraysize(samples));
    for (int i = 0; i < total; i++)
      samples[i] = ((running_sample_index++ / half_square_wave_period) % 2)
                       ? volume
                       : -volume;

    SDL_PutAudioStreamData(stream, samples, total * sizeof(int16_t));
    additional_amount -= total;
  }
}

void sdl_handle_event(sdl_t *sdl, chip8_t *chip8) {
  while (SDL_PollEvent(&sdl->event)) {
    switch (sdl->event.type) {
    case SDL_EVENT_QUIT:
      chip8->running = false;
      break;
    case SDL_EVENT_KEY_DOWN:
      // Quit Esc
      if (sdl->event.key.key == SDLK_ESCAPE)
        chip8->running = false;
      else
        _set_key_state(chip8, sdl->event.key.key, true);
      break;
    case SDL_EVENT_KEY_UP:
      _set_key_state(chip8, sdl->event.key.key, false);
      break;
    }
  }
}

void _set_key_state(chip8_t *chip8, unsigned int sdl_key, bool state) {
  for (uint8_t i = 0; i < 16; i++)
    if (sdl_key == keymap[i].sdl_key)
      chip8->keypad[keymap[i].chip8_key] = state;
}

void sdl_draw(sdl_t *sdl, const chip8_t chip8) {
  SDL_SetRenderTarget(sdl->renderer, sdl->texture);
  SDL_SetRenderDrawColor(sdl->renderer, 0x05, 0x1B, 0x2C, 0xFF);
  SDL_RenderClear(sdl->renderer);

  for (uint16_t i = 0; i < CHIP8_SCREEN_W * CHIP8_SCREEN_H; i++) {
    if (chip8.framebuffer[i]) {
      SDL_SetRenderDrawColor(sdl->renderer, 0x8B, 0xC8, 0xFE, 0xFF);
      SDL_RenderPoint(sdl->renderer, i % CHIP8_SCREEN_W,
                      (uint8_t)(i / CHIP8_SCREEN_W));
    }
  };

  SDL_SetRenderTarget(sdl->renderer, NULL);
  SDL_RenderTexture(sdl->renderer, sdl->texture, NULL, NULL);
  SDL_RenderPresent(sdl->renderer);
}

void sdl_quit(sdl_t *sdl) {
  if (sdl->texture) {
    SDL_DestroyTexture(sdl->texture);
    sdl->texture = NULL;
  }
  if (sdl->renderer) {
    SDL_DestroyRenderer(sdl->renderer);
    sdl->renderer = NULL;
  }
  if (sdl->window) {
    SDL_DestroyWindow(sdl->window);
    sdl->window = NULL;
  }
  if (sdl->stream) {
    SDL_DestroyAudioStream(sdl->stream);
    sdl->stream = NULL;
  }
  SDL_Quit();
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <rom_name>\n", argv[0]);
    return EXIT_FAILURE;
  }

  chip8_t chip8 = {0};
  sdl_t sdl = {0};

  // Init
  sdl_init(&sdl);
  chip8_init(&chip8);

  // Load rom
  const char *rom_name = argv[1];
  if (!chip8_load_rom(&chip8, rom_name))
    return EXIT_FAILURE;

  // Initial drawing (blank screen)
  sdl_draw(&sdl, chip8);

  // Renderer loop
  while (chip8.running) {
    sdl_handle_event(&sdl, &chip8);

    // TODO: Probably not the best solution.
    bool redraw = false;

    const uint64_t start_frame_time = SDL_GetPerformanceCounter();

    // 600 Instructions per second
    for (uint32_t i = 0; i < 10; i++) {
      chip8_step(&chip8);
      if (chip8.draw_flag)
        redraw = true;
    }

    const uint64_t end_frame_time = SDL_GetPerformanceCounter();
    const double time_elapsed =
        (double)((end_frame_time - start_frame_time) * 1000) /
        SDL_GetPerformanceFrequency();

    SDL_Delay(16.67f > time_elapsed ? 16.67f - time_elapsed : 0);

    if (redraw)
      sdl_draw(&sdl, chip8);
    if (chip8.sound_timer > 0 && SDL_AudioStreamDevicePaused(sdl.stream))
      SDL_ResumeAudioStreamDevice(sdl.stream);
    else if (chip8.sound_timer == 0 && !SDL_AudioStreamDevicePaused(sdl.stream))
      SDL_PauseAudioStreamDevice(sdl.stream);

    chip8_tick_timers(&chip8);
  }

  // Clean up
  sdl_quit(&sdl);
  return EXIT_SUCCESS;
}
