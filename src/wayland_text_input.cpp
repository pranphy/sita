#include "wayland_text_input.h"
#include <cstring>
#include <iostream>
#include <print>
#include <wayland-client-protocol.h>

// Include the generated text-input protocol header
#include "text-input-unstable-v3-client-protocol.h"

// Registry listener
static const wl_registry_listener registry_listener = {
    WaylandTextInput::registry_global,
    WaylandTextInput::registry_global_remove,
};

// Text input listener
static const zwp_text_input_v3_listener text_input_listener = {
    WaylandTextInput::text_input_enter,
    WaylandTextInput::text_input_leave,
    WaylandTextInput::text_input_preedit_string,
    WaylandTextInput::text_input_commit_string,
    WaylandTextInput::text_input_delete_surrounding_text,
    WaylandTextInput::text_input_done,
};

WaylandTextInput::WaylandTextInput(wl_display *display)
    : display(display), seat(nullptr), registry(nullptr),
      text_input_manager(nullptr), text_input(nullptr),
      pending_preedit_cursor(0) {

  if (!display) {
    std::println(std::cerr, "WaylandTextInput: Invalid display");
    return;
  }

  // Get the registry to find the text input manager
  registry = wl_display_get_registry(display);
  if (!registry) {
    std::println(std::cerr, "WaylandTextInput: Failed to get registry");
    return;
  }

  wl_registry_add_listener(registry, &registry_listener, this);
  wl_display_roundtrip(display);

  if (!text_input_manager) {
    std::println(std::cerr,
                 "WaylandTextInput: text_input_manager_v3 not available");
    return;
  }

  if (!seat) {
    std::println(std::cerr, "WaylandTextInput: No wl_seat found");
    return;
  }

  // Create text input for this seat
  text_input =
      zwp_text_input_manager_v3_get_text_input(text_input_manager, seat);
  if (!text_input) {
    std::println(std::cerr, "WaylandTextInput: Failed to create text input");
    return;
  }

  zwp_text_input_v3_add_listener(text_input, &text_input_listener, this);

  std::println("WaylandTextInput: Initialized successfully");
}

WaylandTextInput::~WaylandTextInput() {
  if (text_input) {
    zwp_text_input_v3_destroy(text_input);
  }
  if (text_input_manager) {
    zwp_text_input_manager_v3_destroy(text_input_manager);
  }
  if (registry) {
    wl_registry_destroy(registry);
  }
}

void WaylandTextInput::set_preedit_callback(PreeditCallback callback) {
  preedit_callback = callback;
}

void WaylandTextInput::set_commit_callback(CommitCallback callback) {
  commit_callback = callback;
}

void WaylandTextInput::enable() {
  if (text_input) {
    zwp_text_input_v3_enable(text_input);
    zwp_text_input_v3_commit(text_input);
    wl_display_flush(display);
  }
}

void WaylandTextInput::disable() {
  if (text_input) {
    zwp_text_input_v3_disable(text_input);
    zwp_text_input_v3_commit(text_input);
    wl_display_flush(display);
  }
}

void WaylandTextInput::focus_in() {
  enable();
  // Set content type hints
  if (text_input) {
    zwp_text_input_v3_set_content_type(
        text_input, ZWP_TEXT_INPUT_V3_CONTENT_HINT_NONE,
        ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_TERMINAL);
    zwp_text_input_v3_commit(text_input);
    wl_display_flush(display);
  }
}

void WaylandTextInput::focus_out() { disable(); }

void WaylandTextInput::set_cursor_rect(int x, int y, int width, int height) {
  if (text_input) {
    zwp_text_input_v3_set_cursor_rectangle(text_input, x, y, width, height);
    zwp_text_input_v3_commit(text_input);
    wl_display_flush(display);
  }
}

// Static callbacks

void WaylandTextInput::registry_global(void *data, wl_registry *registry,
                                       uint32_t name, const char *interface,
                                       uint32_t version) {
  auto *self = static_cast<WaylandTextInput *>(data);

  if (strcmp(interface, zwp_text_input_manager_v3_interface.name) == 0) {
    self->text_input_manager = static_cast<zwp_text_input_manager_v3 *>(
        wl_registry_bind(registry, name, &zwp_text_input_manager_v3_interface,
                         std::min(version, 1u)));
    std::println("WaylandTextInput: Bound to text_input_manager_v3");
  } else if (strcmp(interface, "wl_seat") == 0) {
    // Bind to the first seat we find
    if (!self->seat) {
      self->seat = static_cast<wl_seat *>(wl_registry_bind(
          registry, name, &wl_seat_interface, std::min(version, 1u)));
      std::println("WaylandTextInput: Bound to wl_seat");
    }
  }
}

void WaylandTextInput::registry_global_remove(void *data, wl_registry *registry,
                                              uint32_t name) {
  // Handle global removal if needed
}

void WaylandTextInput::text_input_enter(void *data,
                                        zwp_text_input_v3 *text_input,
                                        wl_surface *surface) {
  auto *self = static_cast<WaylandTextInput *>(data);
  std::println("WaylandTextInput: Enter event");
  self->enable();
}

void WaylandTextInput::text_input_leave(void *data,
                                        zwp_text_input_v3 *text_input,
                                        wl_surface *surface) {
  auto *self = static_cast<WaylandTextInput *>(data);
  std::println("WaylandTextInput: Leave event");
  self->disable();
}

void WaylandTextInput::text_input_preedit_string(void *data,
                                                 zwp_text_input_v3 *text_input,
                                                 const char *text,
                                                 int32_t cursor_begin,
                                                 int32_t cursor_end) {
  auto *self = static_cast<WaylandTextInput *>(data);
  self->pending_preedit = text ? text : "";
  self->pending_preedit_cursor = cursor_begin;
  std::println("WaylandTextInput: Preedit: '{}' cursor: {}",
               self->pending_preedit, cursor_begin);
}

void WaylandTextInput::text_input_commit_string(void *data,
                                                zwp_text_input_v3 *text_input,
                                                const char *text) {
  auto *self = static_cast<WaylandTextInput *>(data);
  self->pending_commit = text ? text : "";
  std::println("WaylandTextInput: Commit: '{}'", self->pending_commit);
}

void WaylandTextInput::text_input_delete_surrounding_text(
    void *data, zwp_text_input_v3 *text_input, uint32_t before_length,
    uint32_t after_length) {
  // Handle text deletion if needed
  std::println("WaylandTextInput: Delete surrounding text: before={}, after={}",
               before_length, after_length);
}

void WaylandTextInput::text_input_done(void *data,
                                       zwp_text_input_v3 *text_input,
                                       uint32_t serial) {
  auto *self = static_cast<WaylandTextInput *>(data);

  // Process pending events
  if (!self->pending_commit.empty()) {
    if (self->commit_callback) {
      self->commit_callback(self->pending_commit);
    }
    self->pending_commit.clear();
    // Clear preedit when text is committed
    self->pending_preedit.clear();
    if (self->preedit_callback) {
      self->preedit_callback("", 0);
    }
  } else if (!self->pending_preedit.empty() || self->preedit_callback) {
    // Update preedit even if empty (to clear it)
    if (self->preedit_callback) {
      self->preedit_callback(self->pending_preedit,
                             self->pending_preedit_cursor);
    }
  }
}
