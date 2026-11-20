#include <SDL2/SDL.h>
#ifdef CFLAGS_EXTENSIONS
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_USE_SDL2
#include "cimgui.h"
#include "cimgui_impl.h"
#endif

#include "ns.h"
#include "math.h"
#include "pad.h"
#include "pc/gfx/gl.h"
#include "pc/time.h"

#define WINDOW_WIDTH  1024
#define WINDOW_HEIGHT 768

SDL_Window *window = 0;
SDL_GLContext ogl_context;
uint8_t keys[512] = { 0 };
int32_t mousex, mousey;
int mousel, mousem, mouser, mousewheel;

extern ns_struct ns;
extern int done;
extern eid_t insts[8];
extern page_struct texture_pages[16];

void SDLInit() {
#ifdef _WIN32
  SDL_SetMainReady();
#endif
  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
  window = SDL_CreateWindow("c1",
    SDL_WINDOWPOS_UNDEFINED,
    SDL_WINDOWPOS_UNDEFINED,
    WINDOW_WIDTH, WINDOW_HEIGHT,
    SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  ogl_context = SDL_GL_CreateContext(window);
#ifdef CFLAGS_EXTENSIONS
  igCreateContext(0);
  ImGui_ImplSDL2_InitForOpenGL(window, ogl_context);
#endif
}

void SDLKill() {
#ifdef CFLAGS_EXTENSIONS
  ImGui_ImplSDL2_Shutdown();
#endif
  SDL_GL_DeleteContext(ogl_context);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

void SDLUpdate() {
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
#ifdef CFLAGS_EXTENSIONS
    ImGui_ImplSDL2_ProcessEvent(&e);
#endif
    switch (e.type) {
    case SDL_QUIT:
      done = 1;
      break;
    case SDL_KEYDOWN:
    case SDL_KEYUP:
      if (e.key.keysym.sym & 0x40000000)
        keys[(e.key.keysym.sym & 0xFF) + 0x80] = e.type == SDL_KEYDOWN;
      else
        keys[(e.key.keysym.sym & 0x7F)] = e.type == SDL_KEYDOWN;
      break;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
      if (e.button.button == SDL_BUTTON_LEFT)
        mousel = e.type == SDL_MOUSEBUTTONDOWN;
      else if (e.button.button == SDL_BUTTON_MIDDLE)
        mousem = e.type == SDL_MOUSEBUTTONDOWN;
      else if (e.button.button == SDL_BUTTON_RIGHT)
        mouser = e.type == SDL_MOUSEBUTTONDOWN;
      break;
    case SDL_MOUSEWHEEL:
      mousewheel += e.wheel.y;
      break;
    case SDL_MOUSEMOTION:
      mousex = e.motion.x;
      mousey = e.motion.y;
      break;
    default:
      break;
    }
  }
#ifdef CFLAGS_EXTENSIONS
  ImGui_ImplSDL2_NewFrame();
#endif
}

void SDLSwap() {
  SDL_GL_SwapWindow(window);
}

void SDLInput(gl_input *input) {
  int i, width, height;

  SDL_GetWindowSize(window, &width, &height);
  input->window.w = width;
  input->window.h = height;
  input->mouse.x = mousex;
  input->mouse.y = mousey;
  input->buttons = (mousel ? 1 : 0) | (mousem ? 2 : 0) | (mouser ? 4 : 0);
  input->wheel = mousewheel;
  mousewheel = 0;
  for (i=0;i<512;i++) {
    input->keys[i] = keys[i];
  }
}

static int SDLFullscreen(int fullscreen, int *width, int *height) {
  if (fullscreen >= 0 && SDL_SetWindowFullscreen(window,
      fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) < 0) {
    fprintf(stderr, "Unable to change fullscreen mode: %s\n", SDL_GetError());
    return -1;
  }
  SDL_GL_GetDrawableSize(window, width, height);
  return !!(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN);
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
  callbacks.fullscreen = SDLFullscreen;
  GLInit(&callbacks);
  sranda2();
  for (i=1;i<4;i++)
    insts[i] = EID_NONE;
  for (i=0;i<16;i++)
    texture_pages[i].eid = EID_NONE;
  ns.draw_skip_counter = 0;
  return SUCCESS;
}

int _kill() {
  GLKill();
  SDLKill();
  return 0;
}
