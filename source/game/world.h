/* Copyright [2013-2016] [Aaron Springstroh, Minimal Graphics Library]

This file is part of the MGLCraft.

MGLCraft is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

MGLCraft is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with MGLCraft.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef __WORLD__
#define __WORLD__

#include <cmath>
#include <cstdint>
#include <game/ai_path.h>
#include <game/cgrid.h>
#include <game/mob.h>
#include <game/particle.h>
#include <game/sky.h>
#include <game/terrain.h>
#include <min/camera.h>
#include <min/physics.h>
#include <min/tree.h>
#include <min/uniform_buffer.h>
#include <stdexcept>
#include <utility>
#include <vector>

namespace game
{

class world
{
  private:
    static constexpr size_t _pre_max_scale = 5;
    static constexpr size_t _pre_max_vol = _pre_max_scale * _pre_max_scale * _pre_max_scale;

    game::terrain _terrain;
    min::uniform_buffer<float> _preview;
    min::uniform_buffer<float> _geom;
    std::vector<size_t> _view_chunks;
    std::vector<min::aabbox<float, min::vec3>> _player_col_cells;
    std::vector<min::aabbox<float, min::vec3>> _mob_col_cells;

    // User stuff
    min::mesh<float, uint32_t> _terr_mesh;
    min::vec3<unsigned> _scale;
    min::vec3<int> _cached_offset;
    min::vec3<int> _preview_offset;

    // Grid
    cgrid _grid;

    // Physics stuff
    min::vec3<float> _gravity;
    min::physics<float, uint16_t, uint32_t, min::vec3, min::aabbox, min::aabbox, min::tree> _simulation;
    size_t _char_id;

    // Particle stuff
    particle *_particles;

    // Skybox
    sky _sky;

    // Pathing
    ai_path _ai_path;
    min::vec3<float> _dest;

    // Mob instances
    mob_instance _mobs;
    size_t _mob_start;

    // Operating modes
    bool _ai_mode;
    bool _edit_mode;

    // character_load should only be called once!
    inline void character_load(const std::pair<min::vec3<float>, bool> &state)
    {
        // Create a hitbox for character world collisions
        const min::vec3<float> half_extent(0.45, 0.95, 0.45);
        const min::vec3<float> &position = state.first;
        const min::aabbox<float, min::vec3> box(position - half_extent, position + half_extent);
        _char_id = _simulation.add_body(box, 10.0);

        // Get the physics body for editing
        min::body<float, min::vec3> &body = _simulation.get_body(_char_id);

        // Set this body to be unrotatable
        body.set_no_rotate();

        // Update recent chunk
        _grid.update_chunk(position);

        // Generate a new geometry mesh
        generate_terrain();

        // If this the first time we load
        if (!state.second)
        {
            // Set scale to 3x3x3
            _scale = min::vec3<unsigned>(3, 3, 3);

            // Snap to grid
            const min::vec3<float> snapped = cgrid::snap(position);

            // Get direction for particle spray, assume default look direction
            const min::vec3<float> direction = (position + min::vec3<float>(-1.0, 0.0, 0.0)).normalize();

            // Remove geometry around player, requires position snapped to grid and calculated direction vector
            remove_block(snapped, direction);

            // Reset scale to default value
            _scale = min::vec3<unsigned>(1, 1, 1);
        }
    }
    // Generate all geometry in grid and adds it to geometry buffer
    inline void generate_terrain()
    {
        // Get surrounding chunks for drawing
        _grid.get_view_chunks(_view_chunks);

        // Lambda that returns a mesh
        const auto f = [this](const size_t key) -> min::mesh<float, uint32_t> & {
            return _grid.get_chunk(key);
        };

        // Upload contents to the vertex buffer
        _terrain.upload_geometry(_view_chunks, f);
    }
    // Generates the preview geometry and adds it to preview buffer
    inline void generate_preview()
    {
        // Lock in the preview offset
        _preview_offset = _cached_offset;

        // Load data into mesh
        _grid.atlas_preview(_terr_mesh, _preview_offset, _scale);

        // Upload preview geometry
        _terrain.upload_preview(_terr_mesh);
    }
    inline void load_uniform()
    {
        // Load the uniform buffer with program we will use
        _preview.set_program(_terrain.get_program());
        _geom.set_program(_terrain.get_program());

        // Change light alpha for placemark
        const min::vec4<float> col1(1.0, 1.0, 1.0, 1.0);
        const min::vec4<float> pos1(0.0, 100.0, 0.0, 1.0);
        const min::vec4<float> pow1(0.3, 0.7, 0.0, 0.50);
        _preview.add_light(min::light<float>(col1, pos1, pow1));

        // Add light to scene
        const min::vec4<float> col2(1.0, 1.0, 1.0, 1.0);
        const min::vec4<float> pos2(0.0, 100.0, 0.0, 1.0);
        const min::vec4<float> pow2(0.3, 0.7, 0.0, 1.0);
        _geom.add_light(min::light<float>(col2, pos2, pow2));

        // Load projection and view matrix into uniform buffer
        _preview.add_matrix(min::mat4<float>());
        _preview.add_matrix(min::mat4<float>());
        _geom.add_matrix(min::mat4<float>());
        _geom.add_matrix(min::mat4<float>());

        // Load the buffer with data
        _preview.update();
        _geom.update();
    }
    inline void remove_block(const min::vec3<float> &point, const min::vec3<float> &direction)
    {
        // Try to remove geometry from the grid
        const unsigned removed = _grid.set_geometry(point, _scale, _preview_offset, -1);

        // If we removed geometry, play particles
        if (removed > 0)
        {
            // generate new mesh
            generate_terrain();

            // Add particle effects
            _particles->load(point, direction, 5.0);
        }
    }
    inline void update_uniform(min::camera<float> &cam)
    {
        // Calculate new placemark point
        const min::vec3<float> dest = cam.project_point(3.0);

        // Create a ray from camera to destination
        const min::ray<float, min::vec3> r(cam.get_position(), dest);

        // Trace a ray to the destination point to find placement position, return point is snapped
        const min::vec3<float> translate = _grid.ray_trace_prev(r, 6);

        // Update offset x-vector
        if (cam.get_forward().x() >= 0.0)
        {
            _cached_offset.x(1);
        }
        else
        {
            _cached_offset.x(-1);
        }

        // Update offset z-vector
        if (cam.get_forward().z() >= 0.0)
        {
            _cached_offset.z(1);
        }
        else
        {
            _cached_offset.z(-1);
        }

        // Update geom matrix uniforms
        _geom.set_matrix(cam.get_pv_matrix(), 0);
        _geom.update_matrix();

        // Update preview matrix uniforms
        _preview.set_matrix(cam.get_pv_matrix(), 0);
        _preview.set_matrix(translate, 1);
        _preview.update_matrix();
    }
    inline void update_world_physics(const float dt)
    {
        // Solve the AI path finding if toggled
        if (_ai_mode)
        {
            const size_t mob_size = _mobs.size();
            for (size_t i = 0; i < mob_size; i++)
            {
                // Get mob position, mob index offset taken care of
                const min::vec3<float> &mob_p = mob_position(i);

                // Create path data
                path_data data(mob_p, _dest);

                // Calculate step
                _ai_path.calculate(_grid, data);

                // mob index offset taken care of
                mob_path(_ai_path, data, i);
            }
        }

        // Get the player physics object
        min::body<float, min::vec3> &body = _simulation.get_body(_char_id);

        // Get player position
        const min::vec3<float> &p = body.get_position();

        // Calculate the timestep independent from frame rate, goal = 0.0016667
        const unsigned steps = std::ceil(dt / 0.0016667);
        const float time_step = dt / steps;

        // Damping coefficient 10@10 | 3.5@30
        const float damping = 13.25 - 0.325 * steps;
        const float friction = -20.0 / steps;

        // Solve the physics simulation
        for (size_t i = 0; i < steps; i++)
        {
            // Get all cells that could collide
            _grid.create_player_collision_cells(_player_col_cells, p);

            // Add friction force
            const min::vec3<float> &vel = body.get_linear_velocity();
            const min::vec3<float> xz(vel.x(), 0.0, vel.z());

            // Add friction force opposing lateral motion
            body.add_force(xz * body.get_mass() * friction);

            // Solve static collisions
            _simulation.solve_static(_player_col_cells, _char_id, time_step, damping);

            // Do mob collisions
            const size_t mob_size = _mobs.size();
            for (size_t i = 0; i < mob_size; i++)
            {
                // Get mob position, mob index offset taken care of
                const min::vec3<float> &mob_p = mob_position(i);

                // Get all cells that could collide
                _grid.create_mob_collision_cells(_mob_col_cells, mob_p);

                // Solve static collisions
                _simulation.solve_static(_mob_col_cells, _mob_start + i, time_step, damping);
            }
        }

        // Update mob positions
        const size_t mob_size = _mobs.size();
        for (size_t i = 0; i < mob_size; i++)
        {
            const min::body<float, min::vec3> &mob_body = _simulation.get_body(_mob_start + i);
            _mobs.update_position(mob_body.get_position(), i);
        }
    }

  public:
    world(const std::pair<min::vec3<float>, bool> &state, particle *const particles, const size_t grid_size, const size_t chunk_size, const size_t view_chunk_size)
        : _preview(1, 2),
          _geom(1, 2),
          _terr_mesh("atlas"),
          _scale(1, 1, 1),
          _cached_offset(1, 1, 1),
          _preview_offset(1, 1, 1),
          _grid(grid_size, chunk_size, view_chunk_size),
          _gravity(0.0, -10.0, 0.0),
          _simulation(_grid.get_world(), _gravity),
          _particles(particles),
          _sky(_geom, grid_size),
          _dest(state.first),
          _mob_start(1),
          _ai_mode(false),
          _edit_mode(false)
    {
        // Check if chunk_size is valid
        if (grid_size % chunk_size != 0)
        {
            throw std::runtime_error("world: chunk_size must be integer multiple of grid_size");
        }

        // Set the collision elasticity of the physics simulation
        _simulation.set_elasticity(0.1);

        // Load uniform buffers
        load_uniform();

        // Reserve space in the preview mesh
        _terr_mesh.vertex.reserve(_pre_max_vol);
        _terr_mesh.index.reserve(_pre_max_vol);

        // Generate the preview buffer
        generate_preview();

        // Load character
        character_load(state);

        // Reserve space for collision cells
        _player_col_cells.reserve(36);

        // Reserve space for collision cells
        _mob_col_cells.reserve(27);

        // Reserve space for view chunks
        _view_chunks.reserve(view_chunk_size * view_chunk_size * view_chunk_size);
    }
    inline void add_block(const min::ray<float, min::vec3> &r)
    {
        // Trace a ray to the destination point to find placement position, return point is snapped
        const min::vec3<float> traced = _grid.ray_trace_prev(r, 6);

        // Add to grid
        _grid.set_geometry(traced, _scale, _preview_offset, _grid.get_atlas());

        // generate new mesh
        generate_terrain();
    }
    inline void ray_block(const min::ray<float, min::vec3> &r)
    {
        // Shoot a ray of blocks
        _grid.ray_trace_atlas(r, 100);

        // generate new mesh
        generate_terrain();
    }
    inline int8_t remove_block(const min::ray<float, min::vec3> &r)
    {
        // Trace a ray to the destination point to find placement position, return point is snapped
        const min::vec3<float> traced = _grid.ray_trace_last(r, 6);

        // Get the atlas of target block, if hit a block remove it
        const int8_t value = _grid.grid_value(traced);
        if (value >= 0)
        {
            // Get direction for particle spray
            const min::vec3<float> direction = r.get_direction() * -1.0;

            // Remove the block
            remove_block(traced, direction);
        }

        // return the block atlas id
        return value;
    }
    inline size_t add_mob(const min::vec3<float> &p)
    {
        // Create a mob
        const size_t mob_id = _mobs.add_mob(p);

        // Add to physics simulation
        const min::aabbox<float, min::vec3> box = _mobs.mob_box(mob_id);
        const size_t mob_phys_id = _simulation.add_body(box, 10.0);

        // Get the physics body for editing
        min::body<float, min::vec3> &body = _simulation.get_body(mob_phys_id);

        // Set this body to be unrotatable
        body.set_no_rotate();

        return mob_id;
    }
    inline void character_jump(const min::vec3<float> &vel)
    {
        min::body<float, min::vec3> &body = _simulation.get_body(_char_id);

        // If not jumping or falling, allow jump
        if (std::abs(body.get_linear_velocity().y()) < 1.0)
        {
            // Add force to body
            body.add_force(vel * 4000.0 * body.get_mass());
        }
    }
    inline void character_move(const min::vec3<float> &vel)
    {
        min::body<float, min::vec3> &body = _simulation.get_body(_char_id);

        // Get the current position and set y movement to zero
        const min::vec3<float> dxz = min::vec3<float>(vel.x(), 0.0, vel.z()).normalize();

        // Add force to body
        body.add_force(dxz * 1E2 * body.get_mass());
    }
    inline const min::vec3<float> &character_position() const
    {
        const min::body<float, min::vec3> &body = _simulation.get_body(_char_id);

        // Return the character position
        return body.get_position();
    }
    inline void character_warp(const min::vec3<float> &p)
    {
        min::body<float, min::vec3> &body = _simulation.get_body(_char_id);

        // Warp character to new position
        body.set_position(p);
    }
    int8_t grappling(const min::ray<float, min::vec3> &r)
    {
        // Trace a ray to the destination point to find placement position, return point is snapped
        const min::vec3<float> traced = _grid.ray_trace_last(r, 100);

        // Get grid atlas at point
        const int8_t atlas = _grid.grid_value(traced);

        // See if we hit a block
        if (atlas != -1)
        {
            // Compute force along the way proportional to the distance
            const min::vec3<float> d = (traced - r.get_origin());

            // Get character body
            min::body<float, min::vec3> &body = _simulation.get_body(_char_id);

            // Calculate distance of ray
            const float mag = d.magnitude();

            // If the distance is too short give boost
            const float d_factor = (mag < 20.0) ? 1.0 : 0.5;

            // If the pointing down weaken
            const float y_factor = (r.get_direction().y() < -0.5) ? 0.25 : 1.0;

            // Add force to body
            body.add_force(d * 1E3 * d_factor * y_factor * body.get_mass());

            // Reset the scale
            reset_scale();

            // Destroy block
            remove_block(traced, r.get_direction());
        }

        return atlas;
    }
    inline void mob_path(const ai_path &path, const path_data &data, const size_t mob_index)
    {
        // Get the character rigid body
        min::body<float, min::vec3> &body = _simulation.get_body(_mob_start + mob_index);

        // Get remaining distance
        const float remain = data.get_remain();

        // Calculate speed slowing down as approaching goal
        const float speed = 2.75 * ((remain - 3.0) / (remain + 3.0) + 1.1);

        // Calculate the next step
        const min::vec3<float> &step = path.step() * speed;

        // Add velocity to the body
        body.set_linear_velocity(step);
    }
    inline const min::vec3<float> &mob_position(const size_t mob_index) const
    {
        const min::body<float, min::vec3> &body = _simulation.get_body(_mob_start + mob_index);

        // Return the character position
        return body.get_position();
    }
    inline void mob_warp(const min::vec3<float> &p, const size_t mob_index)
    {
        min::body<float, min::vec3> &body = _simulation.get_body(_mob_start + mob_index);

        // Warp character to new position
        body.set_position(p);
    }
    void update(min::camera<float> &cam, const float dt)
    {
        // Update the physics and AI in world
        update_world_physics(dt);

        // Get player position
        const min::vec3<float> &p = character_position();

        // Detect if we crossed a chunk boundary
        bool is_valid = true;
        const size_t current_chunk = _grid.chunk_key(p, is_valid);
        const size_t recent_chunk = _grid.get_recent_chunk();
        if (is_valid && recent_chunk != current_chunk)
        {
            // Update the recent chunk
            _grid.update_chunk(current_chunk);

            // Generate a new geometry mesh
            generate_terrain();
        }

        // Update the mobs
        _mobs.update(cam);

        // Update the particle buffer
        _particles->update(cam, dt);

        // update camera matrices
        update_uniform(cam);
    }
    void draw()
    {
        // Activate the uniform buffer
        _geom.bind();

        // Draw the sky, uses geometry uniform buffer
        _sky.draw();

        // Binds textures and uses program
        _terrain.bind();

        // Draw the world geometry
        _terrain.draw_terrain();

        // Only draw if toggled
        if (_edit_mode)
        {
            // Activate the uniform buffer
            _preview.bind();

            // Draw the placemark
            _terrain.draw_placemark();
        }

        // Draw the mobs
        _mobs.draw();

        // Draw the particles
        _particles->draw();
    }
    inline bool get_ai_mode()
    {
        return _ai_mode;
    }
    inline uint8_t get_atlas_id() const
    {
        return _grid.get_atlas();
    }
    inline bool get_edit_mode()
    {
        return _edit_mode;
    }
    inline const cgrid &get_grid() const
    {
        return _grid;
    }
    inline void reset_scale()
    {
        _scale = min::vec3<unsigned>(1, 1, 1);

        // Regenerate the preview mesh
        generate_preview();
    }
    inline void set_atlas_id(const int8_t id)
    {
        _grid.set_atlas(id);

        // Regenerate the preview mesh
        generate_preview();
    }
    void set_scale_x(unsigned dx)
    {
        if (_edit_mode)
        {
            if (_cached_offset.x() != _preview_offset.x())
            {
                // Regenerate the preview mesh
                generate_preview();
            }
            else if (_scale.x() < _pre_max_scale)
            {
                _scale.x(_scale.x() + dx);

                // Regenerate the preview mesh
                generate_preview();
            }
        }
    }
    void set_scale_y(unsigned dy)
    {
        if (_edit_mode)
        {
            if (_cached_offset.y() != _preview_offset.y())
            {
                // Regenerate the preview mesh
                generate_preview();
            }
            else if (_scale.y() < _pre_max_scale)
            {
                _scale.y(_scale.y() + dy);

                // Regenerate the preview mesh
                generate_preview();
            }
        }
    }
    void set_scale_z(unsigned dz)
    {
        if (_edit_mode)
        {
            if (_cached_offset.z() != _preview_offset.z())
            {
                // Regenerate the preview mesh
                generate_preview();
            }
            else if (_scale.z() < _pre_max_scale)
            {
                _scale.z(_scale.z() + dz);

                // Regenerate the preview mesh
                generate_preview();
            }
        }
    }
    inline void set_destination(const min::vec3<float> &dest)
    {
        // Set destination point
        _dest = dest;
    }
    inline bool toggle_ai_mode()
    {
        // Toggle flag and return result
        return (_ai_mode = !_ai_mode);
    }
    inline bool toggle_edit_mode()
    {
        // Toggle flag and return result
        return (_edit_mode = !_edit_mode);
    }
};
}

#endif
