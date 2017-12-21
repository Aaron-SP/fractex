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
#ifndef __TERRAIN_VERTEX__
#define __TERRAIN_VERTEX__

#include <cstring>
#include <min/mesh.h>
#include <min/vec4.h>
#include <min/window.h>

#ifndef USE_GS_RENDER
#include <game/work_queue.h>
#endif

namespace game
{

#ifdef USE_GS_RENDER

template <typename T, typename K, GLenum FLOAT_TYPE>
class terrain_vertex
{
  private:
    // These are the struct member sizes
    static constexpr size_t vertex_size = sizeof(min::vec4<T>);

    // Compute the size of struct in bytes
    static constexpr size_t width_bytes = vertex_size;

    // Compute the size of struct in floats
    static constexpr size_t width_size = width_bytes / sizeof(T);

  public:
    inline static void create()
    {
        // Specify the vertex attributes in location = 0, no offset
        glVertexAttribPointer(0, 4, FLOAT_TYPE, GL_FALSE, width_bytes, nullptr);
        glEnableVertexAttribArray(0);
    }
    inline static void check(const min::mesh<T, K> &m)
    {
        // Do nothing since only vertex data is valid
    }
    inline static void destroy()
    {
        // Disable the vertex attributes
        glDisableVertexAttribArray(0);
    }
    inline static void copy(std::vector<T> &data, const min::mesh<T, K> &m, const size_t mesh_offset)
    {
        const auto vert_size = m.vertex.size();
        for (size_t i = 0, j = mesh_offset; i < vert_size; i++, j += width_size)
        {
            // Copy the vertex data, 4 floats
            std::memcpy(&data[j], &m.vertex[i], vertex_size);
        }
    }
    inline static constexpr size_t width()
    {
        return width_size;
    }
    inline static constexpr GLenum buffer_type()
    {
        return GL_DYNAMIC_DRAW;
    }
};

#else

template <typename T, typename K, GLenum FLOAT_TYPE>
class terrain_vertex
{
  private:
    // Turn the Struct of Array (SoA) data into Array of Structs (AoS)

    // These are the struct member sizes
    static constexpr size_t vertex_size = sizeof(min::vec4<T>);
    static constexpr size_t uv_size = sizeof(min::vec2<T>);
    static constexpr size_t normal_size = sizeof(min::vec3<T>);

    // These are the struct member offsets in floats, not bytes
    static constexpr size_t uv_off = vertex_size / sizeof(T);
    static constexpr size_t normal_off = uv_off + (uv_size / sizeof(T));

    // Compute the size of struct in bytes
    static constexpr size_t width_bytes = vertex_size + uv_size + normal_size;

    // Compute the size of struct in floats
    static constexpr size_t width_size = width_bytes / sizeof(T);

  public:
    inline static void create()
    {
        // Specify the vertex attributes in location = 0, no offset
        glVertexAttribPointer(0, 4, FLOAT_TYPE, GL_FALSE, width_bytes, nullptr);
        glEnableVertexAttribArray(0);

        // Specify the uv attributes in location = 1, offset is in bytes
        glVertexAttribPointer(1, 2, FLOAT_TYPE, GL_FALSE, width_bytes, (GLvoid *)(uv_off * sizeof(T)));
        glEnableVertexAttribArray(1);

        // Specify the normal attributes in location = 2, offset is in bytes
        glVertexAttribPointer(2, 3, FLOAT_TYPE, GL_FALSE, width_bytes, (GLvoid *)(normal_off * sizeof(T)));
        glEnableVertexAttribArray(2);
    }
    inline static void check(const min::mesh<T, K> &m)
    {
        // Verify normal, tangent and bitangent sizes
        const auto vert_size = m.vertex.size();
        if (m.uv.size() != vert_size || m.normal.size() != vert_size)
        {
            throw std::runtime_error("terrain_vertex: vertex, uv, or normals invalid length");
        }
    }
    inline static void destroy()
    {
        // Disable the vertex attributes
        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
    }
    inline static void copy(std::vector<T> &data, const min::mesh<T, K> &m, const size_t mesh_offset)
    {
        // Parallelize on copying data
        const auto work = [&data, &m, mesh_offset](const size_t i) {

            // Calculate index into data buffer
            const size_t j = mesh_offset + (i * width_size);

            // Copy the vertex data, 4 floats
            std::memcpy(&data[j], &m.vertex[i], vertex_size);

            // Copy the uv data, 2 floats, offset is in number of floats
            std::memcpy(&data[j + uv_off], &m.uv[i], uv_size);

            // Copy the normal data, 3 floats, offset is in number of floats
            std::memcpy(&data[j + normal_off], &m.normal[i], normal_size);
        };

        // Convert cells to mesh in parallel
        work_queue::worker.run(work, 0, m.vertex.size());
    }
    inline static constexpr size_t width()
    {
        return width_size;
    }
    inline static constexpr GLenum buffer_type()
    {
        return GL_DYNAMIC_DRAW;
    }
};
#endif
}

#endif