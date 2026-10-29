#ifndef EXT_H
#define EXT_H

typedef struct {
  int fps_mode;
  int aspect_mode; 
  int enable_textures;
  int enable_gouraud;
  int enable_wireframe;
  int show_octrees;
  int show_objbounds;
  int show_wallmap;
} ext_config_t;

#endif /* EXT_H */