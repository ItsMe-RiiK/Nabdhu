#pragma once

namespace input {
  enum class KeyCode {
    Unknown,
    Up,
    Down,
    Left,
    Right,
    Enter,
    Escape,
    Tab,
    Backspace,
    Char,
    MouseClickLeft,
    MouseClickRight,
    MouseScrollUp,
    MouseScrollDown,
    Resize
  };

  struct Event
  {
    KeyCode key;
    char    ch;
    int     mouse_x;
    int     mouse_y;
  };

  void init_event_system();
  void shutdown_event_system();
  void notify_main_thread();

  // Reads a single event from stdin, blocking until one is available
  Event read_event();

  // Check if input is available (non-blocking)
  bool has_event();

  // Wait for event with timeout
  bool wait_for_event(int timeout_ms);

}  // namespace input
