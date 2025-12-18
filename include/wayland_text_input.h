#ifndef WAYLAND_TEXT_INPUT_H
#define WAYLAND_TEXT_INPUT_H

#include <cstdint>
#include <functional>
#include <string>

// Forward declarations for Wayland types
struct wl_display;
struct wl_surface;
struct wl_registry;
struct wl_seat; // Seat will be discovered via registry
struct zwp_text_input_manager_v3;
struct zwp_text_input_v3;

class WaylandTextInput {
public:
  using PreeditCallback =
      std::function<void(const std::string &text, int cursor)>;
  using CommitCallback = std::function<void(const std::string &text)>;

  // Constructor now only needs display, will discover seat from registry
  WaylandTextInput(wl_display *display);
  ~WaylandTextInput();

  // Disable copy and move
  WaylandTextInput(const WaylandTextInput &) = delete;
  WaylandTextInput &operator=(const WaylandTextInput &) = delete;

  // Set callbacks
  void set_preedit_callback(PreeditCallback callback);
  void set_commit_callback(CommitCallback callback);

  // Input context control
  void enable();
  void disable();
  void focus_in();
  void focus_out();

  // Update cursor position for candidate window placement
  void set_cursor_rect(int x, int y, int width, int height);

  // Check if initialized successfully
  bool is_valid() const { return text_input != nullptr; }

  // Static callbacks for Wayland events (must be public for C linkage)
  static void registry_global(void *data, wl_registry *registry, uint32_t name,
                              const char *interface, uint32_t version);
  static void registry_global_remove(void *data, wl_registry *registry,
                                     uint32_t name);

  static void text_input_enter(void *data, zwp_text_input_v3 *text_input,
                               wl_surface *surface);
  static void text_input_leave(void *data, zwp_text_input_v3 *text_input,
                               wl_surface *surface);
  static void text_input_preedit_string(void *data,
                                        zwp_text_input_v3 *text_input,
                                        const char *text, int32_t cursor_begin,
                                        int32_t cursor_end);
  static void text_input_commit_string(void *data,
                                       zwp_text_input_v3 *text_input,
                                       const char *text);
  static void text_input_delete_surrounding_text(void *data,
                                                 zwp_text_input_v3 *text_input,
                                                 uint32_t before_length,
                                                 uint32_t after_length);
  static void text_input_done(void *data, zwp_text_input_v3 *text_input,
                              uint32_t serial);

private:
  wl_display *display;
  wl_seat *seat;
  wl_registry *registry;
  zwp_text_input_manager_v3 *text_input_manager;
  zwp_text_input_v3 *text_input;

  PreeditCallback preedit_callback;
  CommitCallback commit_callback;

  // Pending state (accumulated between done events)
  std::string pending_preedit;
  int pending_preedit_cursor;
  std::string pending_commit;
};

#endif // WAYLAND_TEXT_INPUT_H
