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
#ifndef __EXPLODEPARTICLE__
#define __EXPLODEPARTICLE__

#include <min/camera.h>
#include <min/dds.h>
#include <min/emitter_buffer.h>
#include <min/mat4.h>
#include <min/texture_buffer.h>
#include <min/uniform_buffer.h>

namespace game
{

class explode_particle
{
  private:
    // Opengl stuff
    min::shader _vertex;
    min::shader _fragment;
    min::program _prog;

    // Texture stuff
    min::texture_buffer _tbuffer;
    GLuint _dds_id;

    // Uniform stuff
    min::uniform_buffer<float> _ubuffer;

    // Particle stuff
    min::emitter_buffer<float, GL_FLOAT> _ebuffer;
    float _time;

    void load_textures()
    {
        // Load textures
        const min::dds b = min::dds("data/texture/stone.dds");

        // Load texture buffer
        _dds_id = _tbuffer.add_dds_texture(b);
    }
    void load_uniforms()
    {
        // Load projection matrix into uniform buffer
        _ubuffer.add_matrix(min::mat4<float>());

        // Load camera position matrix into uniform buffer
        _ubuffer.add_matrix(min::mat4<float>());

        // Load the uniform buffer with program we will use
        _ubuffer.set_program(_prog);

        // Load the buffer with data
        _ubuffer.update();
    }
    void step(const float dt)
    {
        // Update the particle positions
        _ebuffer.step(dt);

        // Upload data to GPU
        _ebuffer.upload();
    }
    void update_uniform(min::camera<float> &cam)
    {
        // Update geom matrix uniforms
        _ubuffer.set_matrix(cam.get_pv_matrix(), 0);
        _ubuffer.set_matrix(min::mat4<float>(cam.get_position()), 1);
        _ubuffer.update_matrix();
    }

  public:
    explode_particle()
        : _vertex("data/shader/emitter.vertex", GL_VERTEX_SHADER),
          _fragment("data/shader/emitter.fragment", GL_FRAGMENT_SHADER),
          _prog(_vertex, _fragment),
          _ubuffer(0, 2),
          _ebuffer(min::vec3<float>(), 50, 5, 0.10, 5.0, 5.0),
          _time(0.0)
    {
        // Load textures
        load_textures();

        // Load uniforms
        load_uniforms();

        // Set the particle gravity
        _ebuffer.set_gravity(min::vec3<float>(0.0, -10.0, 0.0));
    }
    void draw(min::camera<float> &cam, const float dt)
    {
        // Update the emitter buffer
        step(dt);

        if (_time > 0.0)
        {
            // Remove some of the time
            _time -= dt;

            // Bind VAO
            _ebuffer.bind();

            // Bind this texture for drawing
            _tbuffer.bind(_dds_id, 0);

            // Use the shader program to draw models
            _prog.use();

            // Update camera position
            update_uniform(cam);

            // Bind the uniform buffer
            _ubuffer.bind();

            // Draw the particles
            _ebuffer.draw();
        }
    }
    void load(const min::vec3<float> &position, const min::vec3<float> &direction, const float time)
    {
        // Set speed direction
        _ebuffer.set_speed(direction);

        // Update the start position
        _ebuffer.set_position(position);

        // Reset the particle animation
        _ebuffer.reset();

        // Add more time to the clock
        _time = time;
    }
};
}

#endif
