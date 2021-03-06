#ifndef __GJ_GAME_H__
#define __GJ_GAME_H__

// 256 Mb
#define MAX_INTERNAL_MEMORY_SIZE (256 * 1024 * 1024)

struct Program_Memory {
  void *start;
  void *free_memory;
  size_t allocated;

  // TODO: do something with allocations finally
  void *allocate(size_t);
};

enum Input_Button {
  IB_mouse_left = 0,
  IB_mouse_middle,
  IB_mouse_right,

  IB_up,
  IB_down,
  IB_left,
  IB_right,

  IB_shift,
  IB_escape,
  IB_key,

  IB__COUNT,
};

struct User_Input {
  bool buttons[IB__COUNT];
  bool key_pressed;
  int symbol;

  // Store the last position mouse was in when a button went down
  v2i mouse_positions[3];
  v2i mouse;

  int scroll = 0;

  User_Input *old;

  bool button_is_down(Input_Button);
  bool button_was_down(Input_Button);
  bool button_went_down(Input_Button);
  bool button_went_up(Input_Button);
  bool key_went_down(int);
};

struct Pixel_Buffer {
  int width;
  int height;
  int max_width;
  int max_height;

  bool was_resized;

  void *memory;

  void allocate();

  void draw_pixel(v2i, u32, bool);
};

struct Program_State {
  int kWindowWidth;
  int kWindowHeight;

  void init(Program_Memory *, Pixel_Buffer *);
};

// ============================== Globals =====================================

global bool g_running;
global Pixel_Buffer g_pixel_buffer;
global Program_Memory g_program_memory;

#endif  // __GJ_GAME_H__
