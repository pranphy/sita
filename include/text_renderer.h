#ifndef TEXT_RENDERER_H
#define TEXT_RENDERER_H

#include <GL/glew.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>
#include <map>
#include <string>
#include <vector>
#include "shader.h"

// Structure to hold character information
struct Character {
    unsigned int texture_id; // ID handle of the glyph texture
    int width;               // Width of the glyph
    int height;              // Height of the glyph
    int bearing_x;           // Horizontal offset from baseline to leftmost
    int bearing_y;           // Vertical offset from baseline to topmost
    unsigned int advance;     // Horizontal offset to advance to next glyph
};

// Structure to hold shaped glyph information
struct ShapedGlyph {
    unsigned int glyph_id;   // HarfBuzz glyph ID
    float x_offset;          // X offset from base position
    float y_offset;          // Y offset from base position
    float x_advance;         // X advance for next glyph
    float y_advance;         // Y advance for next glyph
    Character* character;    // Pointer to character data
    int font_index;          // Index of the font used for this glyph
};

std::string show_char(Character);

class TextRenderer {
public:
    TextRenderer(const char* primary_font_path, const char* fallback_font_path = nullptr);
    ~TextRenderer();
    void load_font(const char* font_path, int font_index);
    void render_text(const std::string& text, float x, float y, float scale, const float* color, int window_width, int window_height);
    void render_text_harfbuzz(const std::string& text, float x, float y, float scale, const float* color, int window_width, int window_height);

private:
    std::map<char, Character> characters;
    std::map<unsigned int, Character> glyphs; // Map HarfBuzz glyph IDs to characters
    std::vector<std::map<unsigned int, Character>> font_glyphs; // Glyphs per font using glyph_id as key
    unsigned int vao, vbo;
    Shader* shader;
    std::vector<FT_Face> ft_faces;
    std::vector<hb_font_t*> hb_fonts;
    hb_buffer_t* hb_buffer;
    
    void setup_buffers();
    std::vector<ShapedGlyph> shape_text(const std::string& text);
    void load_glyph(unsigned int glyph_id, int font_index);
    unsigned int get_glyph_id_for_char(char c, int font_index);
};

#endif // TEXT_RENDERER_H

