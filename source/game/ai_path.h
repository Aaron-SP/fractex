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
#ifndef __AI_PATH__
#define __AI_PATH__

#include <algorithm>
#include <game/cgrid.h>
#include <game/file.h>
#include <min/intersect.h>
#include <min/vec3.h>
#include <mml/nnet.h>
#include <mml/vec.h>

namespace game
{

class path_data
{
  private:
    min::vec3<float> _destination;
    min::vec3<float> _direction;
    min::vec3<float> _position;
    min::vec3<float> _start;
    float _remain;
    float _travel;

    void update_direction()
    {
        // Update direction vector
        _direction = _destination - _position;

        // Normalize direction if needed
        _remain = _direction.magnitude();
        if (_remain > 1E-4)
        {
            const float inv_mag = 1.0 / _remain;
            _direction *= inv_mag;
        }
    }

  public:
    path_data(const min::vec3<float> &p, const min::vec3<float> &d)
        : _destination(d), _position(p), _start(p),
          _remain(0.0), _travel(0.0)
    {
        // Update direction
        update_direction();
    }
    const min::vec3<float> &get_destination() const
    {
        return _destination;
    }
    const min::vec3<float> &get_direction() const
    {
        return _direction;
    }
    const min::vec3<float> &get_position() const
    {
        return _position;
    }
    const min::vec3<float> &get_start() const
    {
        return _start;
    }
    float get_remain() const
    {
        return _remain;
    }
    float get_travel() const
    {
        return _travel;
    }
    min::vec3<float> step(const min::vec3<float> &dir, const float step_size) const
    {
        // return new position
        return _position + (dir * step_size);
    }
    void update(const min::vec3<float> &p)
    {
        // Update the position
        _position = p;

        // Update the direction
        update_direction();

        // Update distance travelled
        _travel = (_position - _start).magnitude();
    }
};

class ai_path
{
  private:
    static constexpr size_t IN = 7;
    static constexpr size_t OUT = 3;
    static constexpr float _step_size = 0.5;
    static constexpr unsigned _total_moves = 20;
    mml::nnet<float, IN, OUT> _net;

    static size_t collisions(const cgrid &grid, const min::vec3<float> &p)
    {
        size_t out = 0;

        // Create player mesh at location
        const min::vec3<float> half_extent(0.45, 0.95, 0.45);
        const min::aabbox<float, min::vec3> player(p - half_extent, p + half_extent);

        // Create collision blocks
        const std::vector<min::aabbox<float, min::vec3>> blocks = grid.create_collision_cells(p);
        for (const auto &b : blocks)
        {
            if (min::intersect(player, b))
            {
                out++;
            }
        }

        return out;
    }
    min::vec3<float> model(const cgrid &grid, const path_data &data)
    {
        // Must be 27 in size
        const std::vector<int8_t> neighbors = grid.get_neighbors(data.get_position());
        if (neighbors.size() != 27)
        {
            throw std::runtime_error("ai_path: shit is broken");
        }

        // Calculate search direction with respect to gradient
        const min::vec3<float> &dir = data.get_direction();

        // Create output
        min::vec3<float> change;

        // Check x collisions
        bool x_flag = true;
        {
            change.x(dir.x());
            if (dir.x() > 0.0)
            {
                x_flag = x_flag && (neighbors[21] == -1);
                x_flag = x_flag && (neighbors[22] == -1);
                x_flag = x_flag && (neighbors[23] == -1);
            }
            else
            {
                x_flag = x_flag && (neighbors[3] == -1);
                x_flag = x_flag && (neighbors[4] == -1);
                x_flag = x_flag && (neighbors[5] == -1);
            }

            // Zero out x
            if (!x_flag)
            {
                change.x(0.0);
            }
        }

        // Check y collisions
        bool y_flag = true;
        {
            change.y(dir.y());
            if (dir.y() > 0.0)
            {
                y_flag = y_flag && (neighbors[16] == -1);
            }
            else
            {
                y_flag = y_flag && (neighbors[10] == -1);
            }

            // Zero out y
            if (!y_flag)
            {
                change.y(0.0);
            }
        }

        // Check z collisions
        bool z_flag = true;
        {
            change.z(dir.z());
            if (dir.z() > 0.0)
            {
                z_flag = z_flag && (neighbors[5] == -1);
                z_flag = z_flag && (neighbors[14] == -1);
                z_flag = z_flag && (neighbors[23] == -1);
            }
            else
            {
                z_flag = z_flag && (neighbors[3] == -1);
                z_flag = z_flag && (neighbors[12] == -1);
                z_flag = z_flag && (neighbors[21] == -1);
            }

            // Zero out z
            if (!z_flag)
            {
                change.z(0.0);
            }
        }

        // Choose the smallest of X or Z to move around corners
        if (!x_flag && std::abs(dir.x()) <= std::abs(dir.z()))
        {
            change.x(dir.x());
        }
        else if (!z_flag && std::abs(dir.z()) <= std::abs(dir.x()))
        {
            change.z(dir.z());
        }

        // Hurdle obstacle
        {
            bool hurdle = false;
            hurdle = hurdle || (neighbors[9] != -1 && neighbors[18] == -1);
            hurdle = hurdle || (neighbors[10] != -1 && neighbors[19] == -1);
            hurdle = hurdle || (neighbors[11] != -1 && neighbors[20] == -1);
            hurdle = hurdle || (neighbors[12] != -1 && neighbors[21] == -1);
            hurdle = hurdle || (neighbors[13] != -1 && neighbors[22] == -1);
            hurdle = hurdle || (neighbors[14] != -1 && neighbors[23] == -1);
            hurdle = hurdle || (neighbors[15] != -1 && neighbors[24] == -1);
            hurdle = hurdle || (neighbors[16] != -1 && neighbors[25] == -1);

            const bool moving_x = (std::abs(dir.x()) > 0.1);
            const bool moving_z = (std::abs(dir.z()) > 0.1);
            if (hurdle && (!moving_x || !moving_z))
            {
                change.y(1.0);
            }
        }

        // Normalize the output, default to zero
        change.normalize_safe(min::vec3<float>());

        // Override settings if reached goal
        if (data.get_remain() < 0.25)
        {
            change = min::vec3<float>();
        }

        // return new direction
        return change;
    }
    void load(const cgrid &grid, const path_data &data)
    {
        // Set inputs
        mml::vector<float, IN> in;
        const min::vec3<float> &dest = data.get_destination();
        const min::vec3<float> &position = data.get_position();

        // Calculate input
        const float mag = 1.0 / 128.0;
        const min::vec3<float> d = dest * mag;
        const min::vec3<float> s = position * mag;
        in[0] = d.x();
        in[1] = d.y();
        in[2] = d.z();
        in[3] = s.x();
        in[4] = s.y();
        in[5] = s.z();

        // Map data from [-1, 1] to [0, 1] range
        in[0] = 0.5 * (1.0 + in[0]);
        in[1] = 0.5 * (1.0 + in[1]);
        in[2] = 0.5 * (1.0 + in[2]);
        in[3] = 0.5 * (1.0 + in[3]);
        in[4] = 0.5 * (1.0 + in[4]);
        in[5] = 0.5 * (1.0 + in[5]);

        // Set input and calculate output
        _net.set_input(in);
    }
    min::vec3<float> unload(const mml::vector<float, OUT> &output)
    {
        // Calculate direction to move
        const float x = (output[0] * 2.0) - 1.0;
        const float y = (output[1] * 2.0) - 1.0;
        const float z = (output[2] * 2.0) - 1.0;
        return min::vec3<float>(x, y, z) * _step_size;
    }
    min::vec3<float> solve(const cgrid &grid, const path_data &data)
    {
        // Load neural net
        load(grid, data);

        // Calculate output
        const mml::vector<float, OUT> out = _net.calculate();

        // Unload output
        return unload(out);
    }

  public:
    ai_path()
    {
        // Initialize top_net
        _net.add_layer(3);
        _net.add_layer(3);
        _net.finalize();
    }
    ai_path(const mml::nnet<float, IN, OUT> &net)
    {
        _net = net;
    }
    static inline ai_path breed(const ai_path &p1, const ai_path &p2)
    {
        return ai_path(mml::nnet<float, IN, OUT>::breed(p1._net, p2._net));
    }
    inline void deserialize(const std::vector<uint8_t> &stream)
    {
        // read data from stream into float format
        size_t next = 0;
        const std::vector<float> data = min::read_le_vector<float>(stream, next);

        // Must definalize the net to deserialize it
        _net.reset();
        _net.deserialize(data);
    }
    inline void mutate(mml::net_rng<float> &rng)
    {
        _net.mutate(rng);
    }
    inline float fitness(const cgrid &grid, const min::vec3<float> &start, const min::vec3<float> &dest)
    {
        path_data p_data(start, dest);
        float score = 0.0;

        // For N moves
        for (size_t i = 0; i < _total_moves; i++)
        {
            // Get new travel direction
            const min::vec3<float> dir = solve(grid, p_data);

            // Calculate distance to starting point
            const min::vec3<float> next = p_data.step(dir, _step_size);

            // Check if we picked a bad direction and return start position
            const int8_t atlas = grid.grid_value(next);
            if (atlas != -1)
            {
                // Punish collision
                score -= 1.0;
            }
            else
            {
                p_data.update(next);
            }

            // Discourage zero moves
            const float travel = p_data.get_travel();
            if (travel < 1.0)
            {
                score--;
            }

            // Reward venture out
            // Are we getting closer to destination
            const float remain = p_data.get_remain();
            score += travel / (remain + 1.0);

            // Punish collisions with walls
            //score -= collisions(grid, current);
        }

        // return fitness score
        return score;
    }
    inline float optimize(mml::net_rng<float> &rng, const cgrid &grid, const min::vec3<float> &start, const min::vec3<float> &dest)
    {
        float error = 0.0;
        path_data p_data(start, dest);

        // Do number of steps on path
        for (size_t i = 0; i < _total_moves; i++)
        {
            // Create deviation from path for training
            const min::vec3<float> offset(rng.random(), rng.random(), rng.random());
            const min::vec3<float> p = ((dest - start) * (i / 10000.0)) + offset;

            // Update position
            p_data.update(p);
            const min::vec3<float> &dir = p_data.get_direction();

            // Load input to neural net
            load(grid, p_data);

            // Calculate the net
            _net.calculate();

            mml::vector<float, OUT> set_point;
            set_point[0] = 0.5 * (1.0 + dir.x());
            set_point[1] = 0.5 * (1.0 + dir.y());
            set_point[2] = 0.5 * (1.0 + dir.z());

            // Train input to be output
            _net.backprop(set_point);

            // Check that the train works
            const mml::vector<float, 3> output = _net.calculate();

            // set the error term
            error = (output - set_point).square_magnitude();
        }

        return error;
    }
    inline void randomize(mml::net_rng<float> &rng)
    {
        _net.randomize(rng);
    }
    inline std::vector<float> serialize() const
    {
        return _net.serialize();
    }
};
}

#endif
