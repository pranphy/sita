#include <iostream>
#include <format>
#include <print>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "oglutil.h"
#include "text_renderer.h"

template <>
struct std::formatter<Character> : std::formatter<std::string> {
    // Format the Character object
    auto format(const Character& ch, format_context& ctx) {
        // Create a string representation
        std::string representation = "Character Info: "
                                      "Texture ID: " + std::to_string(ch.texture_id) + ", "
                                      "Size: (" + std::to_string(ch.width) + ", " + std::to_string(ch.height) + "), "
                                      "Bearing: (" + std::to_string(ch.bearing_x) + ", " + std::to_string(ch.bearing_y) + "), "
                                      "Advance: " + std::to_string(ch.advance);
        return std::formatter<std::string>::format(representation, ctx);
    }
};

std::string show_char(Character c){
    return std::format("Width: {}, height: {}, bearing_x: {}, bearing_y: {} advance: {}",c.width,c.height,c.bearing_x,c.bearing_y,c.advance);
}
TextRenderer::TextRenderer():
   main_font ("/home/pranphy/.local/share/fonts/iosevka/IosevkaTermSlabNerdFont-Regular.ttf"),
   fallback_font ("/home/pranphy/.local/share/fonts/devanagari/NotoSerif/NotoSerifDevanagari-Regular.ttf")
{


    // Initialize shader - look for files in current directory (build directory)
    shader = new Shader("text.vert", "text.frag");
    
    // Setup buffers
    setup_buffers();
    
    // Initialize HarfBuzz
    hb_buffer = hb_buffer_create();
    
    // Load primary font (index 0)
    load_font(main_font, 0);
    
   load_font(fallback_font, 1);
}

TextRenderer::~TextRenderer() {
    delete shader;
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    
    // Cleanup HarfBuzz
    if (hb_buffer) hb_buffer_destroy(hb_buffer);
    for (auto hb_font : hb_fonts) {
        if (hb_font) hb_font_destroy(hb_font);
    }
}

void TextRenderer::setup_buffers() {
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void TextRenderer::load_font(const char* font_path, unsigned int font_index) {
    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        std::println(std::cerr,"ERROR::FREETYPE: Could not init FreeType Library");
        return;
    }

    FT_Face face;
    if (FT_New_Face(ft, font_path, 0, &face)) {
        std::println(std::cerr,"ERROR::FREETYPE: Failed to load font");
        return;
    }

    FT_Set_Pixel_Sizes(face, 0, 48); // Set font size

    // Initialize HarfBuzz font
    hb_font_t* hb_font = hb_ft_font_create(face, nullptr);
    if (!hb_font) {
        std::println(std::cerr,"ERROR::HARFBUZZ: Failed to create HarfBuzz font");
        return;
    }

    // Store the face and font
    if (font_index >= ft_faces.size()) {
        ft_faces.resize(font_index + 1);
        hb_fonts.resize(font_index + 1);
        font_glyphs.resize(font_index + 1);
    }
    ft_faces[font_index] = face;
    hb_fonts[font_index] = hb_font;

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // Disable byte-alignment restriction this is !important


    // Note: Don't destroy ft_face here as HarfBuzz needs it
    // FT_Done_Face(face);
    // FT_Done_FreeType(ft);
}

unsigned int TextRenderer::get_glyph_id_for_char(char c, unsigned int font_index) {
    if (font_index < ft_faces.size() && ft_faces[font_index]) {
        return FT_Get_Char_Index(ft_faces[font_index], c);
    }
    return 0;
}

std::vector<ShapedGlyph> TextRenderer::shape_text(const std::string& text) {
    std::vector<ShapedGlyph> shaped_glyphs;
    
    // Reset buffer
    hb_buffer_reset(hb_buffer);
    
    // Add text to buffer
    hb_buffer_add_utf8(hb_buffer, text.c_str(), -1, 0, -1);
    
    // Detect if text contains Devanagari characters
    bool has_devanagari = false;
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(text.c_str());
    size_t len = text.length();
    
    for (size_t i = 0; i < len; i++) {
        if (bytes[i] == 0xE0 && i + 2 < len) {
            // Check for Devanagari range: 0xE0 0xA4 0x80 to 0xE0 0xA5 0xBF
            if (bytes[i + 1] == 0xA4 || bytes[i + 1] == 0xA5) {
                if ((bytes[i + 1] == 0xA4 && bytes[i + 2] >= 0x80) or (bytes[i + 1] == 0xA5 && bytes[i + 2] <= 0xBF)) {
                    has_devanagari = true;
                    break;
                }
            }
        }
    }
    
    // Set appropriate script and language
    hb_script_t script; const char* script_name;
    if (has_devanagari) {
        script = HB_SCRIPT_DEVANAGARI;
        script_name = "ne";
    } else {
        script = HB_SCRIPT_LATIN;
        script_name = "en";
    }
    hb_buffer_set_direction(hb_buffer, HB_DIRECTION_LTR);
    hb_buffer_set_script(hb_buffer, script);
    hb_buffer_set_language(hb_buffer, hb_language_from_string(script_name, -1));
    
    // Try primary font first (index 0)
    int font_index = 0;
    hb_font_t* current_hb_font = hb_fonts[font_index];
    
    // Shape the text with primary font
    hb_shape(current_hb_font, hb_buffer, nullptr, 0);
    
    // Get shaped glyph information
    unsigned int glyph_count;
    hb_glyph_info_t* glyph_info = hb_buffer_get_glyph_infos(hb_buffer, &glyph_count);
    hb_glyph_position_t* glyph_pos = hb_buffer_get_glyph_positions(hb_buffer, &glyph_count);
    
    // Check if primary font has all glyphs, if not, try fallback font
    bool need_fallback = false;
    for (unsigned int i = 0; i < glyph_count; i++) {
        if (glyph_info[i].codepoint == 0) {
            need_fallback = true;
            break;
        }
    }
    
    // If primary font doesn't have all glyphs and we have a fallback font, try it
    if (need_fallback && hb_fonts.size() > 1) {
        // Reset buffer and try fallback font
        hb_buffer_reset(hb_buffer);
        hb_buffer_add_utf8(hb_buffer, text.c_str(), -1, 0, -1);
        hb_buffer_set_direction(hb_buffer, HB_DIRECTION_LTR);
        hb_buffer_set_script(hb_buffer, script);
        if (has_devanagari) {
            hb_buffer_set_language(hb_buffer, hb_language_from_string("ne", -1));
        } else {
            hb_buffer_set_language(hb_buffer, hb_language_from_string("en", -1));
        }
        
        font_index = 1; // Use fallback font
        current_hb_font = hb_fonts[font_index];
        hb_shape(current_hb_font, hb_buffer, nullptr, 0);
        
        // Get updated glyph information
        glyph_info = hb_buffer_get_glyph_infos(hb_buffer, &glyph_count);
        glyph_pos = hb_buffer_get_glyph_positions(hb_buffer, &glyph_count);
    }
    
    for (unsigned int i = 0; i < glyph_count; i++) {
        ShapedGlyph shaped_glyph;
        shaped_glyph.glyph_id = glyph_info[i].codepoint;
        shaped_glyph.x_offset = glyph_pos[i].x_offset / 64.0f;
        shaped_glyph.y_offset = glyph_pos[i].y_offset / 64.0f;
        shaped_glyph.x_advance = glyph_pos[i].x_advance / 64.0f;
        shaped_glyph.y_advance = glyph_pos[i].y_advance / 64.0f;
        shaped_glyph.font_index = font_index;
        
        // Load glyph if not already loaded for this font
        if (font_glyphs[font_index].find(shaped_glyph.glyph_id) == font_glyphs[font_index].end()) {
            load_glyph(shaped_glyph.glyph_id, font_index);
        }
        
        shaped_glyph.character = &font_glyphs[font_index][shaped_glyph.glyph_id];
        shaped_glyphs.push_back(shaped_glyph);
    }
    
    return shaped_glyphs;
}

void TextRenderer::load_glyph(unsigned int glyph_id, unsigned int font_index) {
    // Load the glyph using FreeType
    if (FT_Load_Glyph(ft_faces[font_index], glyph_id, FT_LOAD_RENDER)) {
        std::println(std::cerr,"ERROR::FREETYPE: Failed to load Glyph {}", glyph_id);
        return;
    }
    
    // Generate texture
    unsigned int texture;
    oglutil::load_glyph_to_texture(ft_faces[font_index]->glyph, texture);
    
    // Store character information
    Character character = {
        texture,
        static_cast<int>(ft_faces[font_index]->glyph->bitmap.width),
        static_cast<int>(ft_faces[font_index]->glyph->bitmap.rows),
        ft_faces[font_index]->glyph->bitmap_left,
        ft_faces[font_index]->glyph->bitmap_top,
        static_cast<unsigned int>(ft_faces[font_index]->glyph->advance.x >> 6)
    };
    
    font_glyphs[font_index][glyph_id] = character;
}

Coord TextRenderer::render_text_harfbuzz(const std::string& text, Coord cur_pos, float scale, const float* color, int window_width, int window_height) {
    // Enable blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Activate shader
    shader->use();
    shader->set_vec3("textColor", color[0], color[1], color[2]);
    
    // Create orthographic projection matrix using window dimensions
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(window_width), 0.0f, static_cast<float>(window_height));
    shader->set_mat4("projection", glm::value_ptr(projection));
    
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(vao);
    
    // Shape the text using HarfBuzz
    std::vector<ShapedGlyph> shaped_glyphs = shape_text(text);
    
    for (const ShapedGlyph& shaped_glyph : shaped_glyphs) {
        Character* ch = shaped_glyph.character;
        
        // Calculate position with HarfBuzz offsets
        float xpos = cur_pos.x + (ch->bearing_x + shaped_glyph.x_offset) * scale;
        float ypos = cur_pos.y - (ch->height - ch->bearing_y - shaped_glyph.y_offset) * scale;
        
        float w = ch->width * scale;
        float h = ch->height * scale;
        
        oglutil::render_texture_over_rectangle(ch->texture_id, vbo, xpos,ypos,w,h);
        
        // Advance cursor using HarfBuzz advances
        cur_pos.x += shaped_glyph.x_advance * scale;
        if(cur_pos.x > window_width){
            cur_pos.x = 0;
            cur_pos.y -= 2*h;
        }
        cur_pos.y += shaped_glyph.y_advance * scale;
    }
    
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    return cur_pos;
}

