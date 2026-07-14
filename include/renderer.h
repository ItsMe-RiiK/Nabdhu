#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace renderer
{

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

  enum class Gradient
  {
    None,
    GreenToRed,
    RedToGreen,
    DarkRedToRed,
    DarkGreenToGreen,
    DarkCyanToCyan
  };

  class Renderer
  {
  public:
    Renderer();
    void resize(int w, int h);
    void clear();
    void draw_text(int x, int y, const std::string &text, int fg = 39, int bg = 49, bool bold = false, bool inverted = false,
                   int max_w = -1);
    void draw_hline(int x, int y, int len, int fg = 39, int bg = 49);
    void draw_vline(int x, int y, int len, int fg = 39, int bg = 49);
    void draw_window(int x, int y, int w, int h, const std::string &title, int title_color = 91, int border_color = 37, int bg = 49);
    void draw_gauge(int x, int y, int len, double percent, int fg_on = 32, int fg_off = 90);
    void draw_block_gauge(int x, int y, int len, double percent, Gradient grad = Gradient::None, int fg_on = 31, int fg_off = 90);
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
