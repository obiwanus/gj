
void *Program_Memory::allocate(size_t size) {
  // Deallocation is not intended (yet)
  void *result;
  if (this->allocated + size > MAX_INTERNAL_MEMORY_SIZE) {
    return NULL;
  }
  result = this->free_memory;
  this->free_memory = (void *)((u8 *)this->free_memory + size);
  return result;
}

void update_and_render(Program_State *state, User_Input *input) {

}

void Program_State::init(Program_Memory *memory, Pixel_Buffer *buffer,
                         Raytrace_Work_Queue *queue) {
  Program_State *state = this;

  state->kWindowWidth = 1500;
  state->kWindowHeight = 1000;

  // Allocate memory for the main buffer
  buffer->allocate();
  buffer->width = state->kWindowWidth;
  buffer->height = state->kWindowHeight;

}

void Pixel_Buffer::allocate() {
  *this = {};
  this->max_width = 3000;
  this->max_height = 3000;
  this->memory = malloc(this->max_width * this->max_height * sizeof(u32));
}

Rect Pixel_Buffer::get_rect() {
  Rect result = {0, 0, this->width, this->height};
  return result;
}

bool User_Input::button_was_down(Input_Button button) {
  if (this->old == NULL) return false;
  return this->old->button_is_down(button);
}

bool User_Input::button_is_down(Input_Button button) {
  assert(button < IB__COUNT);
  return this->buttons[button];
}

bool User_Input::button_went_down(Input_Button button) {
  return this->button_is_down(button) && !this->button_was_down(button);
}

bool User_Input::button_went_up(Input_Button button) {
  return !this->button_is_down(button) && this->button_was_down(button);
}

bool User_Input::key_went_down(int key) {
  return this->button_went_down(IB_key) && this->symbol == key;
}

