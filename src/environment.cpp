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


static XSMD *earth, *skybox;
static gl::array_texture *day_tex, *night_tex, *cloud_tex;
static gl::texture *cloud_normal_map, *aurora_bands, *moon_tex, *atmo_map;
static gl::cubemap *skybox_tex;
static gl::program *earth_prg, *cloud_prg, *atmob_prg, *atmof_prg, *sun_prg, *moon_prg, *aurora_prg, *skybox_prg;
static gl::framebuffer *sub_atmo_fbo;
static gl::vertex_attrib *earth_tex_va;
static std::vector<gl::vertex_array *> aurora_vas;
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


    earth_mv = mat4::identity().scaled(vec3(6371.f, 6371.f, 6371.f))
                               .rotated(.41f, vec3(1.f, 0.f, 0.f)); // axial tilt

    if (gl::glext.has_extension(gl::BINDLESS_TEXTURE)) {
        earth_prg = new gl::program {gl::shader::vert("shaders/earth_vert.glsl"), gl::shader::frag("shaders/earth_frag.glsl")};
        cloud_prg = new gl::program {gl::shader::vert("shaders/cloud_vert.glsl"), gl::shader::frag("shaders/cloud_frag.glsl")};
    } else {
        earth_prg = new gl::program {gl::shader::vert("shaders/earth_vert.glsl"), gl::shader::frag("shaders/earth_nbl_frag.glsl")};
        cloud_prg = new gl::program {gl::shader::vert("shaders/cloud_vert.glsl"), gl::shader::frag("shaders/cloud_nbl_frag.glsl")};
    }
    atmob_prg = new gl::program {gl::shader::vert("shaders/ptn_vert.glsl"),   gl::shader::frag("shaders/atmob_frag.glsl")};
    atmof_prg = new gl::program {gl::shader::vert("shaders/atmof_vert.glsl"), gl::shader::frag("shaders/atmof_frag.glsl")};

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


    atmo_map = new gl::texture("assets/atmosphere.png");
    atmo_map->wrap(GL_CLAMP_TO_EDGE);
    atmo_map->tmu() = 3;


    sun_prg = new gl::program {gl::shader::vert("shaders/sun_vert.glsl"), gl::shader::frag("shaders/sun_frag.glsl")};
    sun_prg->bind_attrib("va_position", 0);
    sun_prg->bind_frag("out_col", 0);

    moon_prg = new gl::program {gl::shader::vert("shaders/moon_vert.glsl"), gl::shader::frag("shaders/moon_frag.glsl")};
    moon_prg->bind_attrib("va_position", 0);
    moon_prg->bind_frag("out_col", 0);

    moon_tex = new gl::texture("assets/moon.png");


    sub_atmo_fbo = new gl::framebuffer(2);
    sub_atmo_fbo->color_format(0, GL_R11F_G11F_B10F);
    sub_atmo_fbo->color_format(1, GL_R8);
    sub_atmo_fbo->depth().tmu() = 1;
    (*sub_atmo_fbo)[1].tmu() = 2;


    aurora_prg = new gl::program {gl::shader::vert("shaders/aurora_vert.glsl"), gl::shader::geom("shaders/aurora_geom.glsl"), gl::shader::frag("shaders/aurora_frag.glsl")};
    aurora_prg->bind_attrib("va_position", 0);
    aurora_prg->bind_frag("out_col", 0);

    aurora_bands = new gl::texture("assets/aurora-bands.png");
    aurora_bands->wrap(GL_REPEAT);


    skybox = load_xsmd("assets/skybox.xsmd");
    skybox->make_vertex_array();

    gl::image skybox_templ("assets/skybox-top.png");
    skybox_tex = new gl::cubemap;
    skybox_tex->format(GL_RGB, skybox_templ.width(), skybox_templ.height());
    skybox_tex->load_layer(gl::cubemap::TOP,    gl::image("assets/skybox-top.png"   ));
    skybox_tex->load_layer(gl::cubemap::BOTTOM, gl::image("assets/skybox-bottom.png"));
    skybox_tex->load_layer(gl::cubemap::RIGHT,  gl::image("assets/skybox-right.png" ));
    skybox_tex->load_layer(gl::cubemap::LEFT,   gl::image("assets/skybox-left.png"  ));
    skybox_tex->load_layer(gl::cubemap::FRONT,  gl::image("assets/skybox-front.png" ));
    skybox_tex->load_layer(gl::cubemap::BACK,   gl::image("assets/skybox-back.png"  ));

    skybox_prg = new gl::program {gl::shader::vert("shaders/skybox_vert.glsl"), gl::shader::frag("shaders/skybox_frag.glsl")};
    skybox_prg->bind_attrib("va_position", 0);
    skybox_prg->bind_frag("out_col", 0);


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
                LOD &lod = type == 0 ?   day_lods[lods[type]]
                         : type == 1 ? night_lods[lods[type]]
                         :             cloud_lods[lods[type]];

                // I don't even myself
                int xtex = (31 - tx) * lod.horz_tiles / 32;
                int ytex =       ty  * lod.vert_tiles / 16;

                Tile &tile = lod.tiles[xtex][ytex];

                if (!tile.refcount++) {
                    if (uniform_indices[type] >= max_tex_per_type) {
                        throw std::runtime_error("Texture buffer overrun");
                    }

                    tile.index = uniform_indices[type]++;
                }

                tiles[type] = &tile;
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


static void covered_map_set(int *map, int x, int y, int lod)
{
    int w = helper::minimum(1 << lod, 32), h = helper::minimum(1 << lod, 16);
    int xs = x & ~(w - 1), ys = y & ~(h - 1);

    for (int ry = 0; ry < h; ry++) {
        for (int rx = 0; rx < w; rx++) {
            if (map[32 * (ry + ys) + rx + xs] == -1) {
                map[32 * (ry + ys) + rx + xs] = lod;
            }
        }
    }
}


static void covered_map_unset(int *map, int x, int y, int lod)
{
    int w = helper::minimum(1 << lod, 32), h = helper::minimum(1 << lod, 16);
    int xs = x & ~(w - 1), ys = y & ~(h - 1);

    for (int ry = 0; ry < h; ry++) {
        for (int rx = 0; rx < w; rx++) {
            if (map[32 * (ry + ys) + rx + xs] == lod) {
                map[32 * (ry + ys) + rx + xs] = -1;
            }
        }
    }
}


// full coverage requirement (number of slots required to cover the map
// completely with the given LOD)
static int covered_map_fcr(int *map, int lod)
{
    int w = helper::minimum(1 << lod, 32), h = helper::minimum(1 << lod, 16);
    int count = 0;

    for (int ys = 0; ys < 16; ys += h) {
        for (int xs = 0; xs < 32; xs += w) {
            bool found_uncovered = false;

            for (int ry = 0; (ry < h) && !found_uncovered; ry++) {
                for (int rx = 0; (rx < w) && !found_uncovered; rx++) {
                    if (map[32 * (ry + ys) + rx + xs] == -1) {
                        found_uncovered = true;
                    }
                }
            }

            count += found_uncovered;
        }
    }

    return count;
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
            } else {
                if (tile_lods[x][y] == -1) {
                    changed = true;
                    tile_lods[x][y] = max_lod; // will be fixed right away
                }
            }

            lod_list.emplace_back(pos_dot, x, y);
        }
    }

    std::sort(lod_list.begin(), lod_list.end(), [](const std::tuple<float, int, int> &x, const std::tuple<float, int, int> &y) { return std::get<0>(x) > std::get<0>(y); });

    float cam_ground_dist = gstat.camera_position.length() - 6357.f;
    int base_lod = log2f(cam_ground_dist / gstat.width) + 1.f;

    if (base_lod < min_lod) {
        base_lod = min_lod;
    } else if (base_lod > max_lod) {
        base_lod = max_lod;
    }

    int slots_to_keep;
    switch (base_lod) {
        case 0:  slots_to_keep = 11; break;
        case 1:  slots_to_keep =  5; break;
        default: slots_to_keep =  0; break;
    }

    int cover_lod = helper::maximum(base_lod, 3);

    int covered[32 * 16], used_slots = 0;

    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 32; x++) {
            if (tile_lods[x][y] < 0) {
                covered[y * 32 + x] = -2;
            } else {
                covered[y * 32 + x] = -1;
            }
        }
    }

    for (auto it = lod_list.begin(); it != lod_list.end(); ++it) {
        auto &tile = *it;
        int x = std::get<1>(tile), y = std::get<2>(tile);

        if (covered[32 * y + x] >= 0) {
            int lod = covered[32 * y + x];

            if (tile_lods[x][y] != lod) {
                changed = true;
                tile_lods[x][y] = lod;
            }

            continue;
        }

        covered_map_set(covered, x, y, base_lod);

        // safe cover correction
        int scc = base_lod < cover_lod ? covered_map_fcr(covered, cover_lod) : 0;
        if (++used_slots + scc > max_tex_per_type - slots_to_keep) {
            covered_map_unset(covered, x, y, base_lod);
            --it;
            used_slots--;

            if (base_lod < max_lod) {
                base_lod++;
            }

            if (base_lod == 1) {
                slots_to_keep = 5;
            } else {
                slots_to_keep = 0;
            }
        } else {
            if (tile_lods[x][y] != base_lod) {
                changed = true;
                tile_lods[x][y] = base_lod;
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
    // Load dynamic data first, so it can be copied over the course of the function
    if (aurora_vas.empty()) {
        for (size_t i = 0; i < world.auroras.size(); i++) {
            gl::vertex_array *aurora_va = new gl::vertex_array;
            aurora_va->set_elements(world.auroras[i].samples().size());
            aurora_va->attrib(0)->format(2);
            aurora_va->attrib(0)->data(nullptr, world.auroras[i].samples().size() * sizeof(vec2), GL_DYNAMIC_DRAW);

            aurora_vas.push_back(aurora_va);
        }
    }

    for (size_t i = 0; i < aurora_vas.size(); i++) {
        memcpy(aurora_vas[i]->attrib(0)->map(),
               world.auroras[i].samples().data(),
               world.auroras[i].samples().size() * sizeof(vec2));

        aurora_vas[i]->attrib(0)->unmap();
    }


    mat4 cur_earth_mv = earth_mv.rotated(world.earth_angle, vec3(0.f, 1.f, 0.f));
    mat4 cur_cloud_mv = cur_earth_mv.scaled(vec3(6381.f / 6371.f, 6381.f / 6371.f, 6381.f / 6371.f));
    mat4 cur_atmo_mv  = cur_earth_mv.scaled(vec3(6441.f / 6371.f, 6441.f / 6371.f, 6441.f / 6371.f));


    static float lod_update_timer;

    lod_update_timer += world.interval;
    update_lods(status, cur_earth_mv, lod_update_timer >= .2f);
    lod_update_timer = fmodf(lod_update_timer, .2f);


    float sa_zn = (status.camera_position.length() - 6400.f) / 1.5f;
    float sa_zf =  status.camera_position.length() + 7000.f;

    mat4 sa_proj = mat4::projection(status.yfov, status.aspect, sa_zn, sa_zf);


    // everything is in front of the skybox
    glDisable(GL_DEPTH_TEST);


    skybox_tex->bind();

    mat4 skybox_proj = mat4::projection(status.yfov, status.aspect, 1000.f, 1500.f);
    mat4 skybox_mv   = mat3(status.world_to_camera);
    skybox_mv[3][3] = 1.f;

    skybox_prg->use();
    skybox_prg->uniform<mat4>("mat_mvp") = skybox_proj * skybox_mv;
    skybox_prg->uniform<gl::cubemap>("skybox") = *skybox_tex;

    skybox->draw();


    vec4 sun_pos = status.world_to_camera * vec4::position(149.6e6f * -world.sun_light_dir);
    vec4 projected_sun = status.projection * sun_pos;
    projected_sun /= projected_sun.w();

    if (sun_pos.z() < 0.f) {
        float sun_radius = atanf(696.e3f / sun_pos.length()) * 2.f / status.yfov;

        sun_prg->use();
        sun_prg->uniform<vec2>("sun_position") = projected_sun;
        sun_prg->uniform<vec2>("sun_size") = vec2(sun_radius * status.height / status.width, sun_radius);

        quad_vertices->draw(GL_TRIANGLE_STRIP);
    }


    vec3 moon_right = (status.camera_forward.cross(vec3(0.f, 1.f, 0.f))).normalized();
    vec3 moon_up = moon_right.cross(status.camera_forward);

    moon_tex->bind();

    moon_prg->use();
    moon_prg->uniform<vec3>("moon_position") = world.moon_pos;
    moon_prg->uniform<vec3>("right") = moon_right;
    moon_prg->uniform<vec3>("up") = moon_up;
    moon_prg->uniform<mat3>("mat_nrm") = mat3(mat4::identity().rotated(-world.moon_angle_to_sun, vec3(0.f, 1.f, 0.f)));
    moon_prg->uniform<mat4>("mat_mvp") = status.projection * status.world_to_camera;
    moon_prg->uniform<gl::texture>("moon_tex") = *moon_tex;

    quad_vertices->draw(GL_TRIANGLE_STRIP);


    glEnable(GL_DEPTH_TEST);


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
    atmo_map->bind();

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
    atmof_prg->uniform<gl::texture>("atmo_map") = *atmo_map;

    earth->draw();


    aurora_bands->bind();
    (*sub_atmo_fbo)[1].bind();
    sub_atmo_fbo->depth().bind();

    aurora_prg->use();
    aurora_prg->uniform<mat4>("mat_mv") = cur_atmo_mv;
    aurora_prg->uniform<mat4>("mat_proj") = status.projection * status.world_to_camera;
    aurora_prg->uniform<vec3>("cam_pos") = status.camera_position;
    aurora_prg->uniform<vec3>("cam_fwd") = status.camera_forward;
    aurora_prg->uniform<vec2>("screen_dim") = vec2(status.width, status.height);
    aurora_prg->uniform<vec2>("sa_z_dim") = vec2(sa_zn, sa_zf);
    aurora_prg->uniform<gl::texture>("stencil") = (*sub_atmo_fbo)[1];
    aurora_prg->uniform<gl::texture>("depth") = sub_atmo_fbo->depth();
    aurora_prg->uniform<gl::texture>("bands") = *aurora_bands;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    for (gl::vertex_array *va: aurora_vas) {
        va->draw(GL_LINE_LOOP);
    }
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}
