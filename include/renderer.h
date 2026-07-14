#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace renderer
{
  namespace Symbols
  {
    extern const std::array<std::string, 10> superscript;
    extern const std::unordered_map<std::string, std::vector<std::string>> graph_symbols;
    extern const std::string hline;
    extern const std::string vline;
    extern const std::string top_left;
    extern const std::string top_right;
    extern const std::string bottom_left;
    extern const std::string bottom_right;
    extern const std::string block;
    extern const std::string degree;
    extern const std::array<std::string, 9> blocks;
  } // namespace Symbols

  enum class GradientDirection
  {
    None,
    LightToDark,
    DarkToLight,
    ColorToColor
  };

  constexpr int RGB_FLAG = 1 << 24;

  inline int rgb(uint8_t r, uint8_t g, uint8_t b)
  {
    return RGB_FLAG | (r << 16) | (g << 8) | b;
  }

  struct Cell
  {
    std::string ch = " ";
    int fg = 39; // Default foreground
    int bg = 49; // Default background
    bool bold = false;
    bool inverted = false;

    bool operator!=(const Cell &other) const
    {
      return ch != other.ch || fg != other.fg || bg != other.bg || bold != other.bold || inverted != other.inverted;
    }
  };

  inline int color_to_rgb(int c)
  {
    if (c & RGB_FLAG)
      return c;

    switch (c)
    {
    case 31:
    case 41:
    case 91:
    case 101:
      return rgb(255, 0, 0);
    case 32:
    case 42:
    case 92:
    case 102:
      return rgb(0, 255, 0);
    case 33:
    case 43:
    case 93:
    case 103:
      return rgb(255, 255, 0);
    case 34:
    case 44:
    case 94:
    case 104:
      return rgb(0, 0, 255);
    case 35:
    case 45:
    case 95:
    case 105:
      return rgb(255, 0, 255);
    case 36:
    case 46:
    case 96:
    case 106:
      return rgb(0, 255, 255);
    case 37:
    case 47:
    case 97:
    case 107:
      return rgb(255, 255, 255);
    case 30:
    case 40:
    case 90:
    case 100:
      return rgb(128, 128, 128);
    default:
      return rgb(255, 255, 255);
    }
  }

  class Renderer
  {
  public:
    Renderer();
    void resize(int w, int h);
    void clear();
    void
    draw_text(int x, int y, const std::string &text, int fg = 39, int bg = 49, bool bold = false, bool inverted = false, int max_w = -1);
    void draw_hline(int x, int y, int len, int fg = 39, int bg = 49);
    void draw_vline(int x, int y, int len, int fg = 39, int bg = 49);
    void fill_rect(int x, int y, int w, int h, const std::string &ch = " ", int fg = 39, int bg = 49);
    void draw_window(int x, int y, int w, int h, const std::string &title, int title_color = 91, int border_color = 37, int bg = 49);
    void draw_gauge(int x, int y, int len, double percent, int fg_on = 32, int fg_off = 90);
    void draw_block_gauge(
        int x, int y, int len, double percent, GradientDirection dir = GradientDirection::None, int start_col = -1, int end_col = -1,
        int fg_on = 31, int fg_off = 90
    );
    void draw_graph(int x, int y, int w, int h, const std::vector<double> &history, int fg = 92);
    void draw_block_graph(int x, int y, int w, int h, const std::vector<double> &history);
    void render(); // flushes to stdout using ANSI escape sequences

  private:
    int width = 0;
    int height = 0;
    std::vector<Cell> back_buffer;
    std::vector<Cell> front_buffer;
  };

} // namespace renderer
