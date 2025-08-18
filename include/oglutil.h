#ifndef OGLUTIL_H
#define OGLUTIL_H
#include <GL/glew.h>
#include <ft2build.h>
#include <hb-ft.h>


namespace oglutil {
    void load_glyph_to_texture(FT_GlyphSlot glyph, unsigned int &texture);
    void render_texture_over_rectangle(unsigned int& texture, unsigned int vbo, float xpos, float ypos, float w, float h);
    void draw_rectangle(float x, float y, float scale);
}

#endif
