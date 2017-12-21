/* Copyright [2013-2018] [Aaron Springstroh, Minimal Graphics Library]

This file is part of the Fractex.

Fractex is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Fractex is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Fractex.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef __GUN_STATE__
#define __GUN_STATE__

#include <chrono>
#include <game/character.h>
#include <min/sample.h>

namespace game
{

class gun_state
{
  private:
    std::chrono::high_resolution_clock::time_point _charge;
    std::chrono::high_resolution_clock::time_point _cool;
    uint32_t _energy;
    bool _beam_mode;
    bool _fire_mode;
    bool _shoot_cooldown;

    inline double get_charge_time() const
    {
        // Get the current time
        const std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();

        // return time since last sync
        return std::chrono::duration<double, std::milli>(now - _charge).count();
    }
    inline double get_cool_time() const
    {
        // Get the current time
        const std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();

        // return time since last sync
        return std::chrono::duration<double, std::milli>(now - _cool).count();
    }

  public:
    gun_state() : _energy(0), _beam_mode(true), _fire_mode(true), _shoot_cooldown(false) {}
    inline void absorb(const int8_t atlas_id)
    {
        // Absorb this amount of energy
        const uint32_t value = 0x1 << (atlas_id);
        _energy += value;
    }
    inline bool can_consume(const int8_t atlas_id)
    {
        // Try to consume energy
        const uint32_t value = 0x2 << (atlas_id);
        if (_energy >= value)
        {
            return true;
        }

        // Not enough energy
        return false;
    }
    inline void consume(const int8_t atlas_id)
    {
        // Consume energy
        const uint32_t value = 0x2 << (atlas_id);
        _energy -= value;
    }
    inline bool will_consume(const int8_t atlas_id)
    {
        // Try to consume energy
        const uint32_t value = 0x2 << (atlas_id);
        if (_energy >= value)
        {
            _energy -= value;
            return true;
        }

        // Not enough energy
        return false;
    }
    inline uint32_t get_energy() const
    {
        return _energy;
    }
    inline bool get_fire_mode() const
    {
        return _fire_mode;
    }
    inline bool is_beam_charged() const
    {
        return _beam_mode && !_shoot_cooldown && get_charge_time() > 1000.0;
    }
    inline bool is_beam_mode() const
    {
        return _beam_mode;
    }
    inline bool is_missile_mode() const
    {
        return !_beam_mode;
    }
    inline bool check_cooldown()
    {
        if (_shoot_cooldown)
        {
            const double dt = get_cool_time();
            if (dt > 2000.0)
            {
                _shoot_cooldown = !_shoot_cooldown;
            }
        }

        return !_shoot_cooldown;
    }
    inline void start_charge()
    {
        _charge = std::chrono::high_resolution_clock::now();
    }
    inline void start_cooldown()
    {
        // Start the cooldown timer
        _shoot_cooldown = true;

        // Update start time
        _cool = std::chrono::high_resolution_clock::now();
    }
    inline void set_fire_mode(const bool mode)
    {
        _fire_mode = mode;
    }
    inline void toggle_beam_mode()
    {
        _beam_mode = !_beam_mode;
    }
};
}

#endif