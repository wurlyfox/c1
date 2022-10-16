#include <SDL2/SDL.h>

#include "ns.h"
#include "math.h"
#include "pad.h"
#include "pc/gfx/gl.h"
#include "pc/time.h"

#define WINDOW_WIDTH  640*2
#define WINDOW_HEIGHT 448*2

SDL_Window *window = 0;
SDL_GLContext ogl_context;
uint8_t keys[512] = { 0 };
int32_t mousex, mousey;
int mousel;

extern ns_struct ns;
extern int done;
extern eid_t insts[8];
extern page_struct texture_pages[16];

void SDLInit() {
  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
  window = SDL_CreateWindow("c1",
    SDL_WINDOWPOS_UNDEFINED,
    SDL_WINDOWPOS_UNDEFINED,
    WINDOW_WIDTH, WINDOW_HEIGHT,
    SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  ogl_context = SDL_GL_CreateContext(window);
}

void SDLKill() {
  SDL_GL_DeleteContext(ogl_context);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

void SDLUpdate() {
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    switch (e.type) {
    case SDL_QUIT:
      done = 1;
      break;
    case SDL_KEYDOWN:
      if (e.key.keysym.sym & 0x40000000)
        keys[(int)(e.key.keysym.sym & 0xFF) + 0x80] = 1;
      else
        keys[(int)(e.key.keysym.sym & 0x7F)] = 1;
      break;
    case SDL_KEYUP:
      if (e.key.keysym.sym & 0x40000000)
        keys[(e.key.keysym.sym & 0xFF) + 0x80] = 0;
      else
        keys[(e.key.keysym.sym & 0x7F)] = 0;
      break;
    case SDL_MOUSEBUTTONUP:
    case SDL_MOUSEBUTTONDOWN:
      mousel = e.type == SDL_MOUSEBUTTONDOWN;
    case SDL_MOUSEMOTION:
      mousex = e.motion.x;
      mousey = e.motion.y;
      break;
    default:
      break;
    }
  }
}

void SDLSwap() {
  SDL_GL_SwapWindow(window);
}

void SDLInput(gl_input *input) {
  int i;

  input->window.w = WINDOW_WIDTH;
  input->window.h = WINDOW_HEIGHT;
  input->mouse.x = mousex;
  input->mouse.y = mousey;
  input->click = 0 | (mousel ? 1 : 0);
  for (i=0;i<512;i++) {
    input->keys[i] = keys[i];
  }
}

int init() {
  gl_callbacks callbacks;
  int i;

  PadInit(2); /* initialize 2 joypad structs */
  SetTicksElapsed(0);
  SDLInit();
  callbacks.pre_update = SDLUpdate;
  callbacks.post_update = SDLSwap;
  callbacks.ext_supported = (int (*)(const char*))SDL_GL_ExtensionSupported;
  callbacks.proc_addr = SDL_GL_GetProcAddress;
  callbacks.input = SDLInput;
  GLInit(&callbacks);
  sranda2();
  for (i=1;i<4;i++)
    insts[i] = EID_NONE;
  for (i=0;i<16;i++)
    texture_pages[i].eid = EID_NONE;
  ns.draw_skip_counter = 0;
  return SUCCESS;
}

int kill() {
  GLKill();
  SDLKill();
}
