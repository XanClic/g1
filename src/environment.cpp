#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include <dake/dake.hpp>

#include "environment.hpp"
#include "graphics.hpp"
#include "options.hpp"
#include "xsmd.hpp"


using namespace dake;
using namespace dake::math;


#define EARTH_HORZ 256
#define EARTH_VERT 128


static XSMD *earth;
static gl::array_texture *day_tex, *night_tex, *cloud_tex;
static gl::texture *cloud_normal_map;
static gl::program *earth_prg, *cloud_prg, *atmob_prg, *atmof_prg, *sun_prg;
static gl::framebuffer *sub_atmo_fbo;
static gl::vertex_array *sun_va;
static gl::vertex_attrib *earth_tex_va;
static mat4 earth_mv, cloud_mv, atmo_mv;

// type \in \{ day, night, clouds \}
static int max_tex_per_type = 20;
static int min_lod = 0, max_lod = 8;


struct Tile {
    float s = 0.f, t = 0.f;

    void *source = nullptr;
    size_t source_size = 0;

    gl::image *image = nullptr;
    gl::texture *texture = nullptr;

    int refcount = 0;
    int index = 0;

    void load_image(void);
    void load_texture(void);
    void unload_image(void);
    void unload_texture(void);

    void load_source(const char *name, int lod, int si, int ti);
};


struct LOD {
    int total_width, total_height;
    int horz_tiles, vert_tiles;

    std::vector<std::vector<Tile>> tiles;
};


static std::vector<LOD> day_lods, night_lods, cloud_lods;
static std::vector<std::vector<int>> tile_lods;
static std::vector<Tile *> day_tex_tiles(max_tex_per_type), night_tex_tiles(max_tex_per_type), cloud_tex_tiles(max_tex_per_type);


void Tile::load_source(const char *name, int lod, int si, int ti)
{
    char fname[64];
    sprintf(fname, "assets/%s/%i-%i-%i.jpg", name, lod, si, ti);
    FILE *fp = fopen(gl::find_resource_filename(fname).c_str(), "rb");
    if (!fp) {
        throw std::runtime_error("Could not open " + std::string(fname) + ": " + std::string(strerror(errno)));
    }

    fseek(fp, 0, SEEK_END);
    source_size = ftell(fp);
    rewind(fp);

    source = malloc(source_size);
    if (!source) {
        throw std::runtime_error("Could not allocate memory");
    }

    fread(source, 1, source_size, fp);
    fclose(fp);
}


void Tile::load_image(void)
{
    if (image) {
        return;
    }

    image = new gl::image(source, source_size);
}


void Tile::load_texture(void)
{
    if (texture) {
        return;
    }

    texture = new gl::texture(*image);
    texture->bind();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    texture->make_bindless();
}


void Tile::unload_image(void)
{
    delete image;
    image = nullptr;
}


void Tile::unload_texture(void)
{
    delete texture;
    texture = nullptr;
}


static void resize(unsigned w, unsigned h);


static void init_lods(void)
{
    if (!gl::glext.has_extension(gl::BINDLESS_TEXTURE) && (global_options.max_lod > 5)) {
        if (global_options.min_lod > 5) {
            throw std::runtime_error("Cannot use a min-lod > 5 without bindless texture support");
        }

        global_options.max_lod = 5;
    }

    min_lod = global_options.min_lod;
    max_lod = global_options.max_lod;

    if ((min_lod < 0) || (min_lod > 8) || (max_lod < 3) || (max_lod > 8) || (min_lod > max_lod)) {
        throw std::runtime_error("Invalid values given for min-lod and/or max-lod");
    }

    switch (min_lod) {
        case 0:  max_tex_per_type = 20; break;
        case 1:  max_tex_per_type = 16; break;
        case 2:  max_tex_per_type = 12; break;
        case 3:  max_tex_per_type =  8; break;
        case 4:  max_tex_per_type =  2; break;
        default: max_tex_per_type =  1;
    }

    memset(day_tex_tiles.data(), 0, max_tex_per_type * sizeof(Tile *));
    memset(night_tex_tiles.data(), 0, max_tex_per_type * sizeof(Tile *));
    memset(cloud_tex_tiles.data(), 0, max_tex_per_type * sizeof(Tile *));

    day_lods.resize(max_lod + 1);
    night_lods.resize(max_lod + 1);
    cloud_lods.resize(max_lod + 1);

    for (int lod = min_lod; lod <= max_lod; lod++) {
        cloud_lods[lod].total_width  = night_lods[lod].total_width  = day_lods[lod].total_width  = 65536 >> lod;
        cloud_lods[lod].total_height = night_lods[lod].total_height = day_lods[lod].total_height = 32768 >> lod;

        cloud_lods[lod].horz_tiles = day_lods[lod].horz_tiles = lod < 5 ? 32 >> lod : 1;
        cloud_lods[lod].vert_tiles = day_lods[lod].vert_tiles = lod < 4 ? 16 >> lod : 1;

        if (lod >= 2) {
            night_lods[lod].horz_tiles = day_lods[lod].horz_tiles;
            night_lods[lod].vert_tiles = day_lods[lod].vert_tiles;
        } else {
            night_lods[lod].horz_tiles = night_lods[lod].vert_tiles = 0;
        }

        float s = 0.f;
        int si = 0;

        day_lods[lod].tiles.resize(day_lods[lod].horz_tiles);
        cloud_lods[lod].tiles.resize(cloud_lods[lod].horz_tiles);
        if (lod >= 2) {
            night_lods[lod].tiles.resize(night_lods[lod].horz_tiles);
        }

        for (int x = 0; x < day_lods[lod].horz_tiles; x++) {
            float t = 0.f;
            int ti = 0;

            day_lods[lod].tiles[x].resize(day_lods[lod].vert_tiles);
            cloud_lods[lod].tiles[x].resize(cloud_lods[lod].vert_tiles);
            if (lod >= 2) {
                night_lods[lod].tiles[x].resize(night_lods[lod].vert_tiles);
            }

            for (int y = 0; y < day_lods[lod].vert_tiles; y++) {
                cloud_lods[lod].tiles[x][y].s = day_lods[lod].tiles[x][y].s = s;
                cloud_lods[lod].tiles[x][y].t = day_lods[lod].tiles[x][y].t = t;

                if (lod >= 2) {
                    night_lods[lod].tiles[x][y].s = s;
                    night_lods[lod].tiles[x][y].t = t;
                }

                day_lods[lod].tiles[x][y].load_source("earth", lod, si, ti);
                cloud_lods[lod].tiles[x][y].load_source("clouds", lod, si, ti);

                if (lod >= 2) {
                    night_lods[lod].tiles[x][y].load_source("night", lod, si, ti);
                }

                if (lod < 4) {
                    t += exp2f(lod - 4);
                    ti++;
                }
            }

            if (lod < 5) {
                s += exp2f(lod - 5);
                si++;
            }

        }
    }


    tile_lods.resize(32);
    for (std::vector<int> &vec: tile_lods) {
        vec.resize(16);
        for (int &lod: vec) {
            lod = -1;
        }
    }
}


void init_environment(void)
{
    earth = load_xsmd("assets/earth.xsmd");
    earth->make_vertex_array();

    earth_tex_va = earth->va->attrib(3);
    earth_tex_va->format(2, GL_INT);
    earth_tex_va->data(nullptr, static_cast<size_t>(-1), GL_DYNAMIC_DRAW);

    init_lods();

    if (!gl::glext.has_extension(gl::BINDLESS_TEXTURE)) {
        day_tex = new gl::array_texture;
        day_tex->wrap(GL_MIRRORED_REPEAT);
        day_tex->format(GL_RGB, 2048, 2048, max_tex_per_type);

        night_tex = new gl::array_texture;
        night_tex->tmu() = 1;
        night_tex->wrap(GL_MIRRORED_REPEAT);
        night_tex->format(GL_RG, 2048, 2048, max_tex_per_type);

        cloud_tex = new gl::array_texture;
        cloud_tex->tmu() = 2;
        cloud_tex->wrap(GL_MIRRORED_REPEAT);
        cloud_tex->format(GL_RED, 2048, 2048, max_tex_per_type);
    }

    cloud_normal_map = new gl::texture("assets/cloud_normals.jpg");
    cloud_normal_map->tmu() = 3;
    cloud_normal_map->wrap(GL_REPEAT);

    sun_va = new gl::vertex_array;
    sun_va->set_elements(4);

    vec2 sun_vertex_positions[] = {
        vec2(-1.f, 1.f), vec2(-1.f, -1.f), vec2(1.f, 1.f), vec2(1.f, -1.f)
    };

    gl::vertex_attrib *sun_pos = sun_va->attrib(0);
    sun_pos->format(2);
    sun_pos->data(sun_vertex_positions);
    sun_pos->load();


    earth_mv = mat4::identity().scaled(vec3(6378.f, 6357.f, 6378.f))
                               .rotated(.41f, vec3(1.f, 0.f, 0.f)); // axial tilt

    if (gl::glext.has_extension(gl::BINDLESS_TEXTURE)) {
        earth_prg = new gl::program {gl::shader::vert("assets/earth_vert.glsl"), gl::shader::frag("assets/earth_frag.glsl")};
        cloud_prg = new gl::program {gl::shader::vert("assets/cloud_vert.glsl"), gl::shader::frag("assets/cloud_frag.glsl")};
    } else {
        earth_prg = new gl::program {gl::shader::vert("assets/earth_vert.glsl"), gl::shader::frag("assets/earth_nbl_frag.glsl")};
        cloud_prg = new gl::program {gl::shader::vert("assets/cloud_vert.glsl"), gl::shader::frag("assets/cloud_nbl_frag.glsl")};
    }
    atmob_prg = new gl::program {gl::shader::vert("assets/ptn_vert.glsl"),   gl::shader::frag("assets/atmob_frag.glsl")};
    atmof_prg = new gl::program {gl::shader::vert("assets/atmof_vert.glsl"), gl::shader::frag("assets/atmof_frag.glsl")};

    earth->bind_program_vertex_attribs(*earth_prg);
    earth->bind_program_vertex_attribs(*cloud_prg);
    earth->bind_program_vertex_attribs(*atmob_prg);
    earth->bind_program_vertex_attribs(*atmof_prg);

    earth_prg->bind_attrib("va_textures", 3);
    cloud_prg->bind_attrib("va_textures", 3);

    earth_prg->bind_frag("out_col", 0);
    earth_prg->bind_frag("out_stencil", 1);
    cloud_prg->bind_frag("out_col", 0);
    atmob_prg->bind_frag("out_col", 0);
    atmof_prg->bind_frag("out_col", 0);


    sun_prg = new gl::program {gl::shader::vert("assets/sun_vert.glsl"), gl::shader::frag("assets/sun_frag.glsl")};
    sun_prg->bind_attrib("va_position", 0);
    sun_prg->bind_frag("out_col", 0);


    sub_atmo_fbo = new gl::framebuffer(2);
    sub_atmo_fbo->color_format(0, GL_R11F_G11F_B10F);
    sub_atmo_fbo->color_format(1, GL_R8);
    sub_atmo_fbo->depth().tmu() = 1;
    (*sub_atmo_fbo)[1].tmu() = 2;


    register_resize_handler(resize);
}


static void resize(unsigned w, unsigned h)
{
    sub_atmo_fbo->resize(w, h);
}


static void perform_lod_update(vec<2, int32_t> *indices)
{
    uint64_t vertex = 0;
    int uniform_indices[3] = {0};

    for (int lod = min_lod; lod <= max_lod; lod++) {
        for (int x = 0; x < day_lods[lod].horz_tiles; x++) {
            for (int y = 0; y < day_lods[lod].vert_tiles; y++) {
                day_lods[lod].tiles[x][y].refcount = 0;
                cloud_lods[lod].tiles[x][y].refcount = 0;
                if (lod >= 2) {
                    night_lods[lod].tiles[x][y].refcount = 0;
                }
            }
        }
    }

    // FIXME
    for (int x = 0; x < EARTH_HORZ; x++) {
        int tx = x / (EARTH_HORZ / 32);

        for (int y = (x ? -1 : 0); y < EARTH_VERT * 2 + (x < EARTH_HORZ - 1 ? 1 : 0); y++) {
            int ty = (y - 1) / (EARTH_VERT / 8);

            if (ty < 0) {
                ty = 0;
            } else if (ty > 15) {
                ty = 15;
            }

            if (tile_lods[tx][ty] < 0) {
                indices[vertex][0] = 0;
                indices[vertex][1] = 0;
                vertex++;
                continue;
            }

            int lods[3] = {
                tile_lods[tx][ty],
                helper::maximum(tile_lods[tx][ty], 2),
                tile_lods[tx][ty]
            };

            Tile *tiles[3] = {nullptr};

            for (int type = 0; type < 3; type++) {
                for (bool retrying = false;; retrying = true) {
                    LOD &lod = type == 0 ?   day_lods[lods[type]]
                             : type == 1 ? night_lods[lods[type]]
                             :             cloud_lods[lods[type]];

                    // I don't even myself
                    int xtex = (31 - tx) * lod.horz_tiles / 32;
                    int ytex =       ty  * lod.vert_tiles / 16;

                    Tile &tile = lod.tiles[xtex][ytex];

                    if (!tile.refcount++) {
                        if (!retrying && (uniform_indices[type] >= max_tex_per_type)) {
                            tile.refcount = 0;
                            lods[type] = helper::maximum(type == 1 ? 2 : 0, min_lod);
                            continue;
                        } else if (retrying) {
                            tile.refcount = 0;
                            if (++lods[type] >= max_lod + 1) {
                                throw std::runtime_error("Could not determine appropriate texture");
                            }
                            continue;
                        }

                        tile.index = uniform_indices[type]++;
                    }

                    tiles[type] = &tile;

                    break;
                }
            }

            assert(tiles[0]->index == tiles[2]->index);

            indices[vertex][0] = tiles[0]->index;
            indices[vertex][1] = tiles[1]->index;

            vertex++;
        }
    }

    assert(vertex == earth->hdr->vertex_count);
}


static bool lod_loading_complete = false, lod_unloading_complete = false;


static void lod_load_images(void)
{
    for (int lod = min_lod; lod <= max_lod; lod++) {
        for (int x = 0; x < day_lods[lod].horz_tiles; x++) {
            for (int y = 0; y < day_lods[lod].vert_tiles; y++) {
                if (day_lods[lod].tiles[x][y].refcount) {
                    day_lods[lod].tiles[x][y].load_image();
                }
                if ((lod >= 2) && night_lods[lod].tiles[x][y].refcount) {
                    night_lods[lod].tiles[x][y].load_image();
                }
                if (cloud_lods[lod].tiles[x][y].refcount) {
                    cloud_lods[lod].tiles[x][y].load_image();
                }
            }
        }
    }

    lod_loading_complete = true;
}


static void lod_load_textures(void)
{
    for (int lod = min_lod; lod <= max_lod; lod++) {
        for (int x = 0; x < day_lods[lod].horz_tiles; x++) {
            for (int y = 0; y < day_lods[lod].vert_tiles; y++) {
                Tile &day_tile = day_lods[lod].tiles[x][y];
                if (day_tile.refcount) {
                    if (gl::glext.has_extension(gl::BINDLESS_TEXTURE)) {
                        day_tile.load_texture();
                    } else if (day_tex_tiles[day_tile.index] != &day_tile) {
                        day_tex->load_layer(day_tile.index, *day_tile.image);
                        day_tex_tiles[day_tile.index] = &day_tile;
                    }
                }
            }
        }
    }

    for (int lod = helper::maximum(min_lod, 2); lod <= max_lod; lod++) {
        for (int x = 0; x < night_lods[lod].horz_tiles; x++) {
            for (int y = 0; y < night_lods[lod].vert_tiles; y++) {
                Tile &night_tile = night_lods[lod].tiles[x][y];
                if (night_tile.refcount) {
                    if (gl::glext.has_extension(gl::BINDLESS_TEXTURE)) {
                        night_tile.load_texture();
                    } else if (night_tex_tiles[night_tile.index] != &night_tile) {
                        night_tex->load_layer(night_tile.index, *night_tile.image);
                        night_tex_tiles[night_tile.index] = &night_tile;
                    }
                }
            }
        }
    }

    for (int lod = min_lod; lod <= max_lod; lod++) {
        for (int x = 0; x < cloud_lods[lod].horz_tiles; x++) {
            for (int y = 0; y < cloud_lods[lod].vert_tiles; y++) {
                Tile &cloud_tile = cloud_lods[lod].tiles[x][y];
                if (cloud_tile.refcount) {
                    if (gl::glext.has_extension(gl::BINDLESS_TEXTURE)) {
                        cloud_tile.load_texture();
                    } else if (cloud_tex_tiles[cloud_tile.index] != &cloud_tile) {
                        cloud_tex->load_layer(cloud_tile.index, *cloud_tile.image);
                        cloud_tex_tiles[cloud_tile.index] = &cloud_tile;
                    }
                }
            }
        }
    }
}


static void lod_unload_images(void)
{
    for (int lod = min_lod; lod <= max_lod; lod++) {
        for (int x = 0; x < day_lods[lod].horz_tiles; x++) {
            for (int y = 0; y < day_lods[lod].vert_tiles; y++) {
                if (!day_lods[lod].tiles[x][y].refcount) {
                    day_lods[lod].tiles[x][y].unload_image();
                }
                if ((lod >= 2) && !night_lods[lod].tiles[x][y].refcount) {
                    night_lods[lod].tiles[x][y].unload_image();
                }
                if (!cloud_lods[lod].tiles[x][y].refcount) {
                    cloud_lods[lod].tiles[x][y].unload_image();
                }
            }
        }
    }

    lod_unloading_complete = true;
}


static void lod_unload_textures(void)
{
    if (!gl::glext.has_extension(gl::BINDLESS_TEXTURE)) {
        return;
    }

    for (int lod = min_lod; lod <= max_lod; lod++) {
        for (int x = 0; x < day_lods[lod].horz_tiles; x++) {
            for (int y = 0; y < day_lods[lod].vert_tiles; y++) {
                if (!day_lods[lod].tiles[x][y].refcount) {
                    day_lods[lod].tiles[x][y].unload_texture();
                }
                if ((lod >= 2) && !night_lods[lod].tiles[x][y].refcount) {
                    night_lods[lod].tiles[x][y].unload_texture();
                }
                if (!cloud_lods[lod].tiles[x][y].refcount) {
                    cloud_lods[lod].tiles[x][y].unload_texture();
                }
            }
        }
    }

    lod_unloading_complete = true;
}


static void lod_set_uniforms(void)
{
    for (int lod = min_lod; lod <= max_lod; lod++) {
        for (int x = 0; x < day_lods[lod].horz_tiles; x++) {
            for (int y = 0; y < day_lods[lod].vert_tiles; y++) {
                if (day_lods[lod].tiles[x][y].refcount) {
                    Tile &tile = day_lods[lod].tiles[x][y];

                    std::string si(std::to_string(tile.index));
                    if (gl::glext.has_extension(gl::BINDLESS_TEXTURE)) {
                        earth_prg->uniform<gl::texture>("day_textures[" + si + "]") = *tile.texture;
                    }
                    earth_prg->uniform<vec4>("day_texture_params[" + si + "]") = vec4(static_cast<float>(day_lods[lod].horz_tiles), static_cast<float>(day_lods[lod].vert_tiles), tile.s, tile.t);
                }

                if ((lod >= 2) && night_lods[lod].tiles[x][y].refcount) {
                    Tile &tile = night_lods[lod].tiles[x][y];

                    std::string si(std::to_string(tile.index));
                    if (gl::glext.has_extension(gl::BINDLESS_TEXTURE)) {
                        earth_prg->uniform<gl::texture>("night_textures[" + si + "]") = *tile.texture;
                    }
                    earth_prg->uniform<vec4>("night_texture_params[" + si + "]") = vec4(static_cast<float>(night_lods[lod].horz_tiles), static_cast<float>(night_lods[lod].vert_tiles), tile.s, tile.t);
                }

                if (cloud_lods[lod].tiles[x][y].refcount) {
                    Tile &tile = cloud_lods[lod].tiles[x][y];

                    std::string si(std::to_string(tile.index));
                    if (gl::glext.has_extension(gl::BINDLESS_TEXTURE)) {
                        earth_prg->uniform<gl::texture>("cloud_textures[" + si + "]") = *tile.texture;
                    }
                    earth_prg->uniform<vec4>("cloud_texture_params[" + si + "]") = vec4(static_cast<float>(cloud_lods[lod].horz_tiles), static_cast<float>(cloud_lods[lod].vert_tiles), tile.s, tile.t);
                }
            }
        }
    }

    for (int lod = min_lod; lod <= max_lod; lod++) {
        for (int x = 0; x < day_lods[lod].horz_tiles; x++) {
            for (int y = 0; y < day_lods[lod].vert_tiles; y++) {
                if (cloud_lods[lod].tiles[x][y].refcount) {
                    Tile &tile = cloud_lods[lod].tiles[x][y];

                    std::string si(std::to_string(tile.index));
                    if (gl::glext.has_extension(gl::BINDLESS_TEXTURE)) {
                        cloud_prg->uniform<gl::texture>("cloud_textures[" + si + "]") = *tile.texture;
                    }
                    cloud_prg->uniform<vec4>("cloud_texture_params[" + si + "]") = vec4(static_cast<float>(cloud_lods[lod].horz_tiles), static_cast<float>(cloud_lods[lod].vert_tiles), tile.s, tile.t);
                }
            }
        }
    }
}


static void update_lods(const GraphicsStatus &gstat, const mat4 &cur_earth_mv, bool update)
{
    static std::thread *loading_thread = nullptr, *unloading_thread = nullptr;
    static vec<2, int32_t> *indices;

    if ((loading_thread && !lod_loading_complete) || (unloading_thread && !lod_unloading_complete)) {
        return;
    }

    if (loading_thread) {
        loading_thread->join();
        delete loading_thread;
        loading_thread = nullptr;

        lod_loading_complete = false;

        lod_load_textures();
        lod_set_uniforms();

        memcpy(earth_tex_va->map(), indices, earth->hdr->vertex_count * sizeof(indices[0]));
        earth_tex_va->unmap();
        delete[] indices;

        unloading_thread = new std::thread(lod_unload_images);

        return;
    } else if (unloading_thread) {
        unloading_thread->join();
        delete unloading_thread;
        unloading_thread = nullptr;

        lod_unloading_complete = false;

        lod_unload_textures();
    }

    if (!update) {
        return;
    }

    mat3 norm_mat = mat3(cur_earth_mv).transposed_inverse();
    bool changed = false;

    std::vector<std::tuple<float, int, int>> lod_list;

    for (int x = 0; x < 32; x++) {
        float xa = (x + .5f) / 32.f * 2.f * static_cast<float>(M_PI);

        for (int y = 0; y < 16; y++) {
            float ya = (y + .5f) / 16.f * static_cast<float>(M_PI);

            vec3 lnrm = vec3(sinf(ya) * cosf(xa), cosf(ya), sinf(ya) * sinf(xa));
            vec3 nrm = (norm_mat * lnrm).normalized();

            float pos_dot = nrm.dot(gstat.camera_position);
            if (pos_dot < 0.f) {
                if (tile_lods[x][y] != -1) {
                    changed = true;
                    tile_lods[x][y] = -1;
                }
                continue;
            }

            lod_list.emplace_back(pos_dot, x, y);
        }
    }

    std::sort(lod_list.begin(), lod_list.end(), [](const std::tuple<float, int, int> &x, const std::tuple<float, int, int> &y) { return std::get<0>(x) > std::get<0>(y); });

    float cam_ground_dist = gstat.camera_position.length() - 6357.f;
    int base_lod = log2f(cam_ground_dist * gstat.width) - 19.5f;

    if (base_lod < min_lod) {
        base_lod = min_lod;
    } else if (base_lod > max_lod) {
        base_lod = max_lod;
    }

    int lod_tiles, lod_tiles_i = 0;

    // highly technolaged algorithm
    int lod_falloff = base_lod;

    switch (lod_falloff) {
        case 0:  lod_tiles = 2; break;
        case 1:  lod_tiles = 16; break;
        case 2:  lod_tiles = 64; break;
        default: lod_tiles = 32 * 16;
    }

    for (auto &tile: lod_list) {
        int x = std::get<1>(tile), y = std::get<2>(tile);

        if (tile_lods[x][y] != base_lod) {
            changed = true;
            tile_lods[x][y] = base_lod;
        }

        if (++lod_tiles_i >= lod_tiles) {
            lod_tiles_i = 0;

            if (base_lod < max_lod) {
                base_lod++;
            }

            switch (lod_falloff) {
                case 0:
                    switch (base_lod) {
                        case 1:  lod_tiles = 10; break;
                        case 2:  lod_tiles = 20; break;
                        default: lod_tiles = 32 * 16; break;
                    }
                    break;

                case 1:
                    if (base_lod == 2) {
                        lod_tiles = 40;
                    } else {
                        lod_tiles = 32 * 16;
                    }
                    break;

                default:
                    lod_tiles = 32 * 16;
            }
        }
    }

    if (changed) {
        indices = new vec<2, int32_t>[earth->hdr->vertex_count];

        perform_lod_update(indices);

        loading_thread = new std::thread(lod_load_images);
    }
}


void draw_environment(const GraphicsStatus &status, const WorldState &world)
{
    mat4 cur_earth_mv = earth_mv.rotated(world.earth_angle, vec3(0.f, 1.f, 0.f));
    mat4 cur_cloud_mv = cur_earth_mv.scaled(vec3(6388.f / 6378.f, 6367.f / 6357.f, 6388.f / 6378.f));
    mat4 cur_atmo_mv  = cur_earth_mv.scaled(vec3(6448.f / 6378.f, 6427.f / 6357.f, 6448.f / 6378.f));


    static float lod_update_timer;

    lod_update_timer += world.interval;
    update_lods(status, cur_earth_mv, lod_update_timer >= .2f);
    lod_update_timer = fmodf(lod_update_timer, .2f);


    float sa_zn = (status.camera_position.length() - 6400.f) / 1.5f;
    float sa_zf =  status.camera_position.length() + 7000.f;

    mat4 sa_proj = mat4::projection(status.yfov, static_cast<float>(status.width) / status.height, sa_zn, sa_zf);


    vec4 sun_pos = 149.6e6f * -world.sun_light_dir;
    sun_pos.w() = 1.f;
    sun_pos = status.world_to_camera * sun_pos;

    vec4 projected_sun = status.projection * sun_pos;
    projected_sun /= projected_sun.w();

    if (sun_pos.z() < 0.f) {
        float sun_radius = atanf(696.e3f / sun_pos.length()) * 2.f / status.yfov;

        // artificial correction (pre-blur)
        sun_radius *= 2.f;

        // everything is in front of the sun
        glDisable(GL_DEPTH_TEST);

        sun_prg->use();
        sun_prg->uniform<vec2>("sun_position") = projected_sun;
        sun_prg->uniform<vec2>("sun_size") = vec2(sun_radius * status.height / status.width, sun_radius);

        sun_va->draw(GL_TRIANGLE_STRIP);

        glEnable(GL_DEPTH_TEST);
    }


    gl::framebuffer *main_fbo = gl::framebuffer::current();

    sub_atmo_fbo->unmask(1);
    sub_atmo_fbo->bind();
    glClearDepth(0.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearDepth(1.f);


    sub_atmo_fbo->mask(1);
    sub_atmo_fbo->bind();

    glDepthFunc(GL_ALWAYS);
    glCullFace(GL_FRONT);

    atmob_prg->use();
    atmob_prg->uniform<mat4>("mat_mv") = cur_atmo_mv;
    atmob_prg->uniform<mat4>("mat_proj") = sa_proj * status.world_to_camera;

    earth->draw();


    sub_atmo_fbo->unmask(1);
    sub_atmo_fbo->bind();

    glDepthFunc(GL_LEQUAL);
    glCullFace(GL_BACK);

    earth_prg->use();
    earth_prg->uniform<mat4>("mat_mv") = cur_earth_mv;
    earth_prg->uniform<mat4>("mat_proj") = sa_proj * status.world_to_camera;
    earth_prg->uniform<mat3>("mat_nrm") = mat3(cur_earth_mv).transposed_inverse();
    earth_prg->uniform<vec3>("cam_pos") = status.camera_position;
    earth_prg->uniform<vec3>("light_dir") = world.sun_light_dir;

    if (!gl::glext.has_extension(gl::BINDLESS_TEXTURE)) {
        day_tex->bind();
        night_tex->bind();
        cloud_tex->bind();

        earth_prg->uniform<gl::array_texture>("day_texture") = *day_tex;
        earth_prg->uniform<gl::array_texture>("night_texture") = *night_tex;
        earth_prg->uniform<gl::array_texture>("cloud_texture") = *cloud_tex;
    }

    earth->draw();


    sub_atmo_fbo->mask(1);
    sub_atmo_fbo->bind();

    glDepthMask(GL_FALSE);

    cloud_normal_map->bind();

    cloud_prg->use();
    cloud_prg->uniform<mat4>("mat_mv") = cur_cloud_mv;
    cloud_prg->uniform<mat4>("mat_proj") = sa_proj * status.world_to_camera;
    cloud_prg->uniform<mat3>("mat_nrm") = mat3(cur_cloud_mv).transposed_inverse();
    cloud_prg->uniform<vec3>("light_dir") = world.sun_light_dir;
    cloud_prg->uniform<gl::texture>("cloud_normal_map") = *cloud_normal_map;

    if (!gl::glext.has_extension(gl::BINDLESS_TEXTURE)) {
        cloud_tex->bind();
        cloud_prg->uniform<gl::array_texture>("cloud_texture") = *cloud_tex;
    }

    earth->draw();

    glDepthMask(GL_TRUE);


    main_fbo->bind();

    (*sub_atmo_fbo)[0].bind();
    (*sub_atmo_fbo)[1].bind();
    sub_atmo_fbo->depth().bind();

    atmof_prg->use();
    atmof_prg->uniform<mat4>("mat_mv") = cur_atmo_mv;
    atmof_prg->uniform<mat4>("mat_proj") = status.projection * status.world_to_camera;
    atmof_prg->uniform<mat3>("mat_nrm") = mat3(cur_atmo_mv).transposed_inverse();
    atmof_prg->uniform<vec3>("cam_pos") = status.camera_position;
    atmof_prg->uniform<vec3>("cam_fwd") = status.camera_forward;
    atmof_prg->uniform<vec3>("light_dir") = world.sun_light_dir;
    atmof_prg->uniform<vec2>("sa_z_dim") = vec2(sa_zn, sa_zf);
    atmof_prg->uniform<vec2>("screen_dim") = vec2(status.width, status.height);
    atmof_prg->uniform<gl::texture>("color") = (*sub_atmo_fbo)[0];
    atmof_prg->uniform<gl::texture>("stencil") = (*sub_atmo_fbo)[1];
    atmof_prg->uniform<gl::texture>("depth") = sub_atmo_fbo->depth();

    earth->draw();
}
