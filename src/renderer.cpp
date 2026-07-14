#include "renderer.h"

#include <fmt/core.h>
#include <fmt/format.h>
#include <iostream>
#include <string>

namespace renderer
{

  Renderer::Renderer() {}

  void Renderer::resize(int w, int h)
  {
    if (w == width && h == height)
      return;
    width = w;
    height = h;
    back_buffer.assign(width * height, Cell());
    front_buffer.assign(width * height, Cell{ "", -1, -1, false, false });
    std::cout << "\x1b[2J" << std::flush;
  }

  void Renderer::clear()
  {
    for (auto &cell : back_buffer)
    {
      cell.ch = " ";
      cell.fg = 39;
      cell.bg = 49;
      cell.bold = false;
      cell.inverted = false;
    }
  }

  void Renderer::draw_text(int x, int y, const std::string &text, int fg, int bg, bool bold, bool inverted, int max_w)
  {
    if (y < 0 || y >= height)
      return;
    int limit = (max_w >= 0) ? std::min(width, x + max_w) : width;

    size_t i = 0;
    int pos = x;
    while (i < text.length() && pos < limit)
    {
      size_t len = 1;
      unsigned char c = text[i];
      if (c >= 0xF0)
        len = 4;
      else if (c >= 0xE0)
        len = 3;
      else if (c >= 0xC0)
        len = 2;
      if (i + len > text.length())
        len = text.length() - i;

      if (pos >= 0 && pos < limit)
      {
        int idx = y * width + pos;
        back_buffer[idx].ch.assign(text, i, len);
        back_buffer[idx].fg = fg;
        back_buffer[idx].bg = bg;
        back_buffer[idx].bold = bold;
        back_buffer[idx].inverted = inverted;
      }
      i += len;
      pos++;
    }
  }

  void Renderer::draw_hline(int x, int y, int len, int fg, int bg)
  {
    if (y < 0 || y >= height)
      return;
    for (int i = 0; i < len; ++i)
    {
      int cur_x = x + i;
      if (cur_x >= 0 && cur_x < width)
      {
        int idx = y * width + cur_x;
        back_buffer[idx].ch = "─";
        back_buffer[idx].fg = fg;
        back_buffer[idx].bg = bg;
        back_buffer[idx].bold = false;
        back_buffer[idx].inverted = false;
      }
    }
  }

  void Renderer::draw_vline(int x, int y, int len, int fg, int bg)
  {
    if (x < 0 || x >= width)
      return;
    for (int i = 0; i < len; ++i)
    {
      int cur_y = y + i;
      if (cur_y >= 0 && cur_y < height)
      {
        int idx = cur_y * width + x;
        back_buffer[idx].ch = "│";
        back_buffer[idx].fg = fg;
        back_buffer[idx].bg = bg;
        back_buffer[idx].bold = false;
        back_buffer[idx].inverted = false;
      }
    }
  }

  void Renderer::draw_window(int x, int y, int w, int h, const std::string &title, int title_color, int border_color, int bg)
  {
    if (w <= 0 || h <= 0)
      return;
    draw_hline(x + 1, y, w - 2, border_color, bg);
    draw_hline(x + 1, y + h - 1, w - 2, border_color, bg);
    draw_vline(x, y + 1, h - 2, border_color, bg);
    draw_vline(x + w - 1, y + 1, h - 2, border_color, bg);
    draw_text(x, y, "┌", border_color, bg);
    draw_text(x + w - 1, y, "┐", border_color, bg);
    draw_text(x, y + h - 1, "└", border_color, bg);
    draw_text(x + w - 1, y + h - 1, "┘", border_color, bg);

    if (!title.empty() && w > 4)
    {
      std::string formatted_title = " " + title + " ";
      draw_text(x + 2, y, formatted_title, title_color, bg);
    }
  }

  void Renderer::draw_gauge(int x, int y, int len, double percent, int fg_on, int fg_off)
  {
    if (y < 0 || y >= height)
      return;
    int filled = (int)(len * percent);
    for (int i = 0; i < len; ++i)
    {
      int cur_x = x + i;
      if (cur_x >= 0 && cur_x < width)
      {
        int idx = y * width + cur_x;
        int fg = (i < filled) ? fg_on : fg_off;
        back_buffer[idx].ch = "│";
        back_buffer[idx].fg = fg;
        back_buffer[idx].bg = 49;
        back_buffer[idx].bold = false;
        back_buffer[idx].inverted = false;
      }
    }
  }

  void Renderer::draw_block_gauge(int x, int y, int len, double percent, Gradient grad, int fg_on, int fg_off)
  {
    if (y < 0 || y >= height)
      return;
    int filled = (int)(len * percent);
    for (int i = 0; i < len; ++i)
    {
      int cur_x = x + i;
      if (cur_x >= 0 && cur_x < width)
      {
        int idx = y * width + cur_x;
        int fg = fg_off;
        if (i < filled)
        {
          if (grad != Gradient::None)
          {
            float t = (float)i / len;
            if (grad == Gradient::GreenToRed)
            {
              if (t <= 0.5f)
              {
                fg = renderer::rgb((int)((t * 2.0f) * 255), 255, 0);
              }
              else
              {
                fg = renderer::rgb(255, (int)((1.0f - (t - 0.5f) * 2.0f) * 255), 0);
              }
            }
            else if (grad == Gradient::RedToGreen)
            {
              if (t <= 0.5f)
              {
                fg = renderer::rgb(255, (int)((t * 2.0f) * 255), 0);
              }
              else
              {
                fg = renderer::rgb((int)((1.0f - (t - 0.5f) * 2.0f) * 255), 255, 0);
              }
            }
            else if (grad == Gradient::DarkRedToRed)
            {
              fg = renderer::rgb(100 + (int)(t * 155), 0, 0);
            }
            else if (grad == Gradient::DarkGreenToGreen)
            {
              fg = renderer::rgb(0, 100 + (int)(t * 155), 0);
            }
            else if (grad == Gradient::DarkCyanToCyan)
            {
              fg = renderer::rgb(0, 100 + (int)(t * 155), 100 + (int)(t * 155));
            }
          }
          else
          {
            fg = fg_on;
          }
        }
        back_buffer[idx].ch = "■";
        back_buffer[idx].fg = fg;
        back_buffer[idx].bg = 49;
        back_buffer[idx].bold = false;
        back_buffer[idx].inverted = false;
      }
    }
  }

  void Renderer::draw_graph(int x, int y, int w, int h, const std::vector<double> &history, int fg)
  {
    if (w <= 0 || h <= 0)
      return;
    int dot_w = w * 2;
    int dot_h = h * 4;

    std::vector<int> heights(dot_w, 0);
    int hist_len = history.size();
    if (hist_len > 0)
    {
      for (int i = 0; i < dot_w; ++i)
      {
        int idx = (i * hist_len) / dot_w;
        if (idx < hist_len)
        {
          heights[i] = (int)((history[idx] / 100.0) * dot_h);
        }
      }
    }

    const std::string braille_up[25] = { " ", "⢀", "⢠", "⢰", "⢸", "⡀", "⣀", "⣠", "⣰", "⣸", "⡄", "⣄", "⣤",
                                         "⣴", "⣼", "⡆", "⣆", "⣦", "⣶", "⣾", "⡇", "⣇", "⣧", "⣷", "⣿" };

    for (int cy = 0; cy < h; ++cy)
    {
      for (int cx = 0; cx < w; ++cx)
      {
        int dot_y_base = cy * 4;
        int dot_x_base = cx * 2;

        int left_val = 0;
        int right_val = 0;

        for (int dy = 0; dy < 4; ++dy)
        {
          int h_left = heights[dot_x_base];
          int h_right = heights[dot_x_base + 1];
          int pixel_y_from_bottom = dot_h - 1 - (dot_y_base + dy);
          if (pixel_y_from_bottom < h_left)
            left_val++;
          if (pixel_y_from_bottom < h_right)
            right_val++;
        }

        int idx = left_val * 5 + right_val;
        draw_text(x + cx, y + cy, braille_up[idx], fg, 49);
      }
    }
  }

  void Renderer::render()
  {
    std::string out;
    out.reserve(4096);
    int last_fg = -1;
    int last_bg = -1;
    bool last_bold = false;
    bool last_inv = false;
    int last_x = -2;
    int last_y = -2;

    for (int y = 0; y < height; ++y)
    {
      for (int x = 0; x < width; ++x)
      {
        int idx = y * width + x;
        const Cell &back = back_buffer[idx];
        Cell &front = front_buffer[idx];

        if (back != front)
        {
          // Move cursor only if not contiguous
          if (y != last_y || x != last_x + 1)
          {
            fmt::format_to(std::back_inserter(out), "\x1b[{};{}H", y + 1, x + 1);
          }
          last_x = x;
          last_y = y;

          // Set attributes if changed
          if (back.fg != last_fg || back.bg != last_bg || back.bold != last_bold || back.inverted != last_inv)
          {
            fmt::format_to(std::back_inserter(out), "\x1b[0");
            if (back.bold)
              fmt::format_to(std::back_inserter(out), ";1");
            if (back.inverted)
              fmt::format_to(std::back_inserter(out), ";7");

            if (back.fg & RGB_FLAG)
            {
              fmt::format_to(std::back_inserter(out), ";38;2;{};{};{}", (back.fg >> 16) & 0xFF, (back.fg >> 8) & 0xFF, back.fg & 0xFF);
            }
            else
            {
              fmt::format_to(std::back_inserter(out), ";{}", back.fg);
            }

            if (back.bg & RGB_FLAG)
            {
              fmt::format_to(std::back_inserter(out), ";48;2;{};{};{}", (back.bg >> 16) & 0xFF, (back.bg >> 8) & 0xFF, back.bg & 0xFF);
            }
            else
            {
              fmt::format_to(std::back_inserter(out), ";{}", back.bg);
            }

            fmt::format_to(std::back_inserter(out), "m");

            last_fg = back.fg;
            last_bg = back.bg;
            last_bold = back.bold;
            last_inv = back.inverted;
          }
          fmt::format_to(std::back_inserter(out), "{}", back.ch);
          front = back;
        }
      }
    }
    if (out.size() > 0)
    {
      std::cout.write(out.data(), out.size());
      std::cout.flush();
    }
  }

} // namespace renderer
