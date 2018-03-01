/* Copyright [2013-2018] [Aaron Springstroh, Minimal Graphics Library]

This file is part of the Beyond Dying Skies.

Beyond Dying Skies is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Beyond Dying Skies is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Beyond Dying Skies.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef _UI_CONFIG__
#define _UI_CONFIG__

namespace game
{
// Everything scales on this value
static constexpr float _s_fg = 32.0;

// Base scale
static constexpr size_t _num_buttons = 8;
static constexpr size_t _num_half_buttons = _num_buttons / 2;
static constexpr float _s_inv = _s_fg / 2;
static constexpr float _s_bg = _s_fg + 8.0;

// Inventory
static constexpr float _store_dy = _s_bg + 8.0;
static constexpr float _tool_dy = _s_bg + 8.0;
static constexpr float _tool_space = _s_bg + 8.0;
static constexpr float _tool_start = -_tool_space * _num_half_buttons + _tool_space / 2;

// Hover
static constexpr float _s_hover_x = 320.0;
static constexpr float _s_hover_y = 190.0;

// Console and UI Text
static constexpr size_t _font_size = 28;
static constexpr size_t _ui_font_size = 14;
static constexpr float _console_dy = (_tool_dy * 2.0) + 4.0;
static constexpr float _s_console_x = 400.0;
static constexpr float _s_console_y = 40.0;
static constexpr float _x_console_wrap = _s_console_x;
static constexpr float _y_console_wrap = _font_size;
static constexpr float _y_ui_text = _console_dy + 24.0;
static constexpr float _energy_dx = (_tool_space * _num_half_buttons) + (_tool_space * 0.5) + 4.0;
static constexpr float _health_dx = _tool_start - _tool_space - 4.0;

// Experience Bar
static constexpr float _s_exp_x = _s_console_x;
static constexpr float _s_exp_y = 8.0;
static constexpr float _exp_dx = (_s_exp_y - _s_exp_x) * 0.5;
static constexpr float _exp_dy = _console_dy + 24.0;

// Alert Text
static constexpr float _x_alert_wrap = 600.0;
static constexpr float _y_alert_wrap = _font_size;
static constexpr float _alert_dy = -180.0;

// Extended UI
static constexpr float _cube_dx = -42.0;
static constexpr float _cube_dy = 518;
static constexpr float _cube_space = 42.0;
static constexpr float _stat_dx = 0.0;
static constexpr float _stat_dy = 458.0;
static constexpr float _stat_text_dx = 114.0;
static constexpr float _stat_text_dy = 597.0;
}

#endif