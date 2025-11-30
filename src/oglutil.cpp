#include <oglutil.h>

namespace oglutil {

void load_glyph_to_texture(FT_GlyphSlot glyph, unsigned int &texture) {
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, glyph->bitmap.width,
               glyph->bitmap.rows, 0, GL_RED, GL_UNSIGNED_BYTE,
               glyph->bitmap.buffer);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void render_texture_over_rectangle(unsigned int &texture, unsigned int vbo,
                                   float xpos, float ypos, float w, float h) {

  float vertices[6][4] = {
      {xpos, ypos + h, 0.0f, 0.0f},    {xpos, ypos, 0.0f, 1.0f},
      {xpos + w, ypos, 1.0f, 1.0f},

      {xpos, ypos + h, 0.0f, 0.0f},    {xpos + w, ypos, 1.0f, 1.0f},
      {xpos + w, ypos + h, 1.0f, 0.0f}};

  glBindTexture(GL_TEXTURE_2D, texture);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glDrawArrays(GL_TRIANGLES, 0, 6);
}
void draw_rectangle(float x, float y, float scale) {
  // Render a blinking cursor (simple underscore)
  glColor3f(1.0f, 1.0f, 1.0f); // White color
  glDisable(GL_TEXTURE_2D);

  float cursor_height = 10.0f * scale;
  float cursor_width = 10.0f * scale / 2.0;

  glBegin(GL_QUADS);
  glVertex2f(x, y);
  glVertex2f(x + cursor_width, y);
  glVertex2f(x + cursor_width, y + cursor_height);
  glVertex2f(x, y + cursor_height);
  glEnd();

  glEnable(GL_TEXTURE_2D);
}

} // namespace oglutil
