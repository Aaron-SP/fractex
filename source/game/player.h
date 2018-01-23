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
#ifndef __PLAYER__
#define __PLAYER__

#include <game/inventory.h>
#include <game/skills.h>
#include <min/aabbox.h>
#include <min/grid.h>
#include <min/physics_nt.h>
#include <min/vec3.h>

namespace game
{
class player
{
  private:
    static constexpr float _air_threshold = 1.0;
    static constexpr float _fall_threshold = -1.0;
    static constexpr float _grav_mag = 10.0;
    static constexpr float _health_cap = 100.0;
    static constexpr float _jet_cost = 0.25;

    min::physics<float, uint16_t, uint32_t, min::vec3, min::aabbox, min::aabbox, min::grid> *_sim;
    std::vector<std::pair<min::aabbox<float, min::vec3>, int8_t>> _col_cells;
    inventory _inv;
    min::vec3<float> _hook;
    min::vec3<float> _land_vel;
    float _hook_length;
    unsigned _jump_count;
    size_t _land_count;
    int8_t _explode_id;
    bool _airborn;
    bool _exploded;
    bool _falling;
    bool _hooked;
    bool _jet;
    bool _landed;
    size_t _body_id;
    skills _skills;
    float _health;
    bool _dead;

    void reserve_memory()
    {
        _col_cells.reserve(36);
    }
    inline void swing()
    {
        // Get player position
        const min::vec3<float> &p = position();

        // Get player velocity
        const min::vec3<float> &vel = velocity();

        // Compute the hook vector
        const min::vec3<float> hook_dir = (_hook - p);
        const float d = hook_dir.magnitude();

        // Calculate swing direction
        if (d > 1.0)
        {
            // Normalize swing direction
            const min::vec3<float> swing_dir = hook_dir * (1.0 / d);

            // Compute pendulum double spring force
            // Spring F=-k(x-x0)
            const float over = _hook_length + 1.0;
            const float under = _hook_length - 1.0;
            if (d > over)
            {
                const float k = 30.0;
                const min::vec3<float> x = swing_dir * (d - over);
                force(x * k);
            }
            else if (d < under)
            {
                const float k = 15.0;
                const min::vec3<float> x = swing_dir * (d - under);
                force(x * k);
            }

            // Calculate the square velocity
            const float vt = vel.magnitude();
            const float vt2 = vt * vt * 1.25;

            // Gravity acceleration, a = g*cos_theta
            // cos_theta = -swing_dir.dot(gravity) == swing_dir.y()
            const float a1 = _grav_mag * swing_dir.y();

            // Centripetal acceleration a = (vt^2)/L
            const float a2 = vt2 / _hook_length;

            // Combined both forces, along the radius
            const min::vec3<float> tension = swing_dir * ((a1 + a2));

            // Pendulum F=-mg.dot(r) + mvt^2/L
            force(tension);
        }
    }
    void update_land(const bool landed)
    {
        // Reset the jump condition if collided with cell, and moving in Y axis
        const min::vec3<float> &v = velocity();
        _airborn = std::abs(v.y()) >= _air_threshold;
        _falling = v.y() <= _fall_threshold;

        // If we collided with a block and we are not falling, signal landing
        if (landed && !_falling)
        {
            _land_count++;

            // If we have just landed
            if (_land_count == 1)
            {
                // Reset the jump count
                _jump_count = 0;

                // Flag we just landed
                _landed = true;

                // Check for fall damage
                const float speed = _land_vel.magnitude();
                if (speed > 20.0)
                {
                    // Lethal damage
                    consume_health(100.0);
                }
                else if (speed > 10.0)
                {
                    consume_health(50.0);
                }
            }
        }
        else if (!_landed && _falling)
        {
            _landed = false;
            _land_count = 0;

            // Cache the land velocity at time of landing
            _land_vel = v;
        }
    }
    void update_position(const float friction)
    {
        // If hooked, add hook forces
        if (_hooked)
        {
            // Calculate forces to make character swing
            swing();
        }
        else if (_jet)
        {
            const bool consumed = _skills.will_consume(_jet_cost);
            if (consumed)
            {
                // Add force to player body
                force(min::vec3<float>(0.0, 11.0, 0.0));
            }
            else
            {
                _jet = false;
            }
        }
        else
        {
            // Add friction force
            const min::vec3<float> &vel = velocity();
            const min::vec3<float> xz(vel.x(), 0.0, vel.z());

            // Add friction force opposing lateral motion
            force(xz * friction);
        }
    }

  public:
    player(min::physics<float, uint16_t, uint32_t, min::vec3, min::aabbox, min::aabbox, min::grid> *sim, const size_t body_id)
        : _sim(sim), _hook_length(0.0), _jump_count(0), _land_count(0), _explode_id(-1),
          _airborn(false), _exploded(false), _falling(false), _hooked(false), _jet(false), _landed(false),
          _body_id(body_id), _health(_health_cap), _dead(false)
    {
        // Reserve space for collision cells
        reserve_memory();
    }

    inline void add_health(float health)
    {
        if (_health < _health_cap)
        {
            _health += health;

            // Cap health at health_cap;
            if (_health > _health_cap)
            {
                _health = _health_cap;
            }
        }
    }
    inline const min::body<float, min::vec3> &body() const
    {
        return _sim->get_body(_body_id);
    }
    inline min::body<float, min::vec3> &body()
    {
        return _sim->get_body(_body_id);
    }
    inline void consume_health(float health)
    {
        _health -= health;
        if (_health <= 0.0)
        {
            _dead = true;
        }
    }
    inline void drone_collide(const min::vec3<float> &p)
    {
        // Consume some of players health
        consume_health(5);

        // Calculate collision direction towards player
        //const min::vec3<float> dir = (position() - p).normalize();

        // Add a kick force
        //force(dir * 1000.0);
    }
    inline void explode(const min::vec3<float> &direction, const float factor, const int8_t value)
    {
        // Signal explode signal
        _exploded = true;

        // Record what we hit
        _explode_id = value;

        // Apply force to the player body
        force(direction * factor);
    }
    inline void force(const min::vec3<float> &f)
    {
        // Apply force to the body per mass
        body().add_force(f * mass());
    }
    inline int8_t get_explode_id() const
    {
        return _explode_id;
    }
    inline const skills &get_skills() const
    {
        return _skills;
    }
    inline skills &get_skills()
    {
        return _skills;
    }
    inline bool has_landed()
    {
        const bool landed = _landed;

        // Reset the landed flag
        _landed = false;

        // Return landed state
        return landed;
    }
    inline float get_health() const
    {
        return _health;
    }
    inline float get_health_percent() const
    {
        return _health / _health_cap;
    }
    inline inventory &get_inventory()
    {
        return _inv;
    }
    inline const inventory &get_inventory() const
    {
        return _inv;
    }
    void hook_abort()
    {
        // abort hooking
        _hooked = false;
    }
    bool is_airborn() const
    {
        return _airborn;
    }
    bool is_exploded() const
    {
        return _exploded;
    }
    bool is_hooked() const
    {
        return _hooked;
    }
    bool is_falling() const
    {
        return _falling;
    }
    bool is_jet() const
    {
        return _jet;
    }
    inline void jump()
    {
        // If not hooked
        if (!_hooked)
        {
            // Allow user to jump and user boosters
            if (_jump_count == 0 && !_airborn)
            {
                // Increment jump count
                _jump_count++;

                // Add force to the player body
                force(min::vec3<float>(0.0, 900.0, 0.0));
            }
            else if (_jump_count == 1)
            {
                // Increment jump count
                _jump_count++;

                // Add force to the player body
                force(min::vec3<float>(0.0, 900.0, 0.0));
            }
        }
    }
    inline bool is_dead() const
    {
        return _dead;
    }
    inline const min::vec3<float> &land_velocity() const
    {
        return _land_vel;
    }
    inline float mass() const
    {
        return body().get_mass();
    }
    inline void move(const min::vec3<float> &vel)
    {
        // If not hooked
        if (!_hooked && !_jet)
        {
            // Get the current position and set y movement to zero
            const min::vec3<float> dxz = min::vec3<float>(vel.x(), 0.0, vel.z()).normalize();

            // Add force to player body
            force(dxz * 30.0);
        }
    }
    inline const min::vec3<float> &position() const
    {
        // Return the character position
        return body().get_position();
    }
    inline void reset_explode()
    {
        _exploded = false;
        _explode_id = -1;
    }
    inline void respawn()
    {
        // Reset explode settings
        reset_explode();

        // Stop hooking for jetting
        _hooked = false;
        _jet = false;

        // Reset energy
        _skills.set_energy(0.0);

        // Reset dead flag
        _dead = false;

        // Reset health
        _health = _health_cap;
    }
    inline void set_hook(const min::vec3<float> &hook, const float hook_length)
    {
        // Set hook point
        _hook = hook;

        // Enable hooking
        _hooked = true;

        // Calculate the hook length
        _hook_length = hook_length;
    }
    inline void set_jet(const bool flag)
    {
        _jet = flag;
    }
    inline const min::vec3<float> &velocity() const
    {
        // Return the character position
        return body().get_linear_velocity();
    }
    inline void velocity(const min::vec3<float> &v)
    {
        // Warp character to new position
        body().set_linear_velocity(v);
    }
    inline void warp(const min::vec3<float> &p)
    {
        // Warp character to new position
        body().set_position(p);
    }
    void update_frame(const cgrid &grid, const float friction,
                      const std::function<void(const std::pair<min::aabbox<float, min::vec3>, int8_t> &)> &ex)
    {
        // Check if player is still in the grid
        const min::vec3<float> &p = position();

        // Get all cells that could collide
        grid.player_collision_cells(_col_cells, p);

        // Solve static collisions
        bool landed = false;
        for (const auto &cell : _col_cells)
        {
            // Did we collide with block?
            const bool collide = _sim->collide(_body_id, cell.first);

            // Detect if player has landed
            if (collide)
            {
                const min::vec3<float> center = cell.first.get_center();
                if (center.y() < p.y())
                {
                    landed = true;
                }
            }

            // If we collided with cell
            if (collide && !is_exploded())
            {
                // Call explosion callback
                ex(cell);
            }
        }

        // Update the landed state
        update_land(landed);

        // Update the player position
        update_position(friction);
    }
};
}

#endif
