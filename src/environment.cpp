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
#include "gltf.hpp"
#include "graphics.hpp"
#include "options.hpp"


using namespace dake;
using namespace dake::math;


#define EARTH_HORZ 256
#define EARTH_VERT 128


static GLTFObject *earth, *skybox;
static gl::array_texture *day_tex, *night_tex;
static gl::texture *cloud_normal_map, *aurora_bands, *moon_tex, *atmo_map;
static gl::cubemap *skybox_tex;
static gl::program *earth_prg, *cloud_prg, *atmob_prg, *atmof_prg, *atmoi_prg, *sun_prg, *moon_prg, *aurora_prg, *skybox_prg;
static gl::framebuffer *sub_atmo_fbo;
static gl::vertex_attrib *earth_tex_va;
static std::vector<gl::vertex_array *> aurora_vas;

// type \in \{ day, night \}
static int max_tex_per_type = 20;
static int min_lod = 0, max_lod = 8;


struct Tile {
    float s = 0.f, t = 0.f;

    void *source = nullptr, *alpha_source = nullptr;
    size_t source_size = 0, alpha_source_size = 0;

    gl::image *rgb_image = nullptr, *alpha_image = nullptr;
    gl::image *uncompressed_image = nullptr, *image = nullptr;
    gl::texture *texture = nullptr;

    int refcount = 0;
    int index = 0;

    void load_image(gl::image::channel_format compression =
                    gl::image::LINEAR_UINT8);
    void load_texture(void);
    void unload_image(void);
    void unload_texture(void);

    void load_source(const char *name, int lod, int si, int ti);
    void load_alpha_source(const char *name, int lod, int si, int ti);
};


struct LOD {
    int total_width, total_height;
    int horz_tiles, vert_tiles;

    std::vector<std::vector<Tile>> tiles;
};


static std::vector<LOD> day_lods, night_lods;
static std::vector<std::vector<int>> tile_lods;
static std::vector<Tile *> day_tex_tiles(max_tex_per_type), night_tex_tiles(max_tex_per_type);


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
        fclose(fp);
        throw std::runtime_error("Could not allocate memory");
    }

    if (fread(source, 1, source_size, fp) < source_size) {
        fclose(fp);
        free(source);
        source = nullptr;
        throw std::runtime_error("Failed to read tile from "
                                 + std::string(fname));
    }
    fclose(fp);
}


void Tile::load_alpha_source(const char *name, int lod, int si, int ti)
{
    char fname[64];
    sprintf(fname, "assets/%s/%i-%i-%i.jpg", name, lod, si, ti);
    FILE *fp = fopen(gl::find_resource_filename(fname).c_str(), "rb");
    if (!fp) {
        throw std::runtime_error("Could not open " + std::string(fname) + ": " + std::string(strerror(errno)));
    }

    fseek(fp, 0, SEEK_END);
    alpha_source_size = ftell(fp);
    rewind(fp);

    alpha_source = malloc(alpha_source_size);
    if (!alpha_source) {
        fclose(fp);
        throw std::runtime_error("Could not allocate memory");
    }

    if (fread(alpha_source, 1, alpha_source_size, fp) < alpha_source_size) {
        fclose(fp);
        free(alpha_source);
        alpha_source = nullptr;
        throw std::runtime_error("Failed to read tile alpha from "
                                 + std::string(fname));
    }
    fclose(fp);
}


void Tile::load_image(gl::image::channel_format compression)
{
    if (image) {
        return;
    }

    if (alpha_source) {
        rgb_image = new gl::image(source, source_size);
        alpha_image = new gl::image(alpha_source, alpha_source_size);

        if ((rgb_image->channels() != 3) || (alpha_image->channels() != 1)) {
            throw std::runtime_error("Images have invalid channel count");
        }

        uncompressed_image = new gl::image(*rgb_image, *alpha_image);
    } else {
        uncompressed_image = new gl::image(source, source_size);
    }

    image = new gl::image(*uncompressed_image, compression);
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

    delete uncompressed_image;
    uncompressed_image = nullptr;

    if (alpha_source) {
        delete rgb_image;
        delete alpha_image;

        rgb_image = nullptr;
        alpha_image = nullptr;
    }
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
        if (global_options.min_lod > 4) {
            throw std::runtime_error("Cannot use a min-lod > 4 without bindless texture support");
        }

        global_options.max_lod = 4;
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

    day_lods.resize(max_lod + 1);
    night_lods.resize(max_lod + 1);

    for (int lod = min_lod; lod <= max_lod; lod++) {
        night_lods[lod].total_width  = day_lods[lod].total_width  = 65536 >> lod;
        night_lods[lod].total_height = day_lods[lod].total_height = 32768 >> lod;

        day_lods[lod].horz_tiles = lod < 5 ? 32 >> lod : 1;
        day_lods[lod].vert_tiles = lod < 4 ? 16 >> lod : 1;

        if (lod >= 2) {
            night_lods[lod].horz_tiles = day_lods[lod].horz_tiles;
            night_lods[lod].vert_tiles = day_lods[lod].vert_tiles;
        } else {
            night_lods[lod].horz_tiles = night_lods[lod].vert_tiles = 0;
        }

        float s = 0.f;
        int si = 0;

        day_lods[lod].tiles.resize(day_lods[lod].horz_tiles);
        if (lod >= 2) {
            night_lods[lod].tiles.resize(night_lods[lod].horz_tiles);
        }

        for (int x = 0; x < day_lods[lod].horz_tiles; x++) {
            float t = 0.f;
            int ti = 0;

            day_lods[lod].tiles[x].resize(day_lods[lod].vert_tiles);
            if (lod >= 2) {
                night_lods[lod].tiles[x].resize(night_lods[lod].vert_tiles);
            }

            for (int y = 0; y < day_lods[lod].vert_tiles; y++) {
                day_lods[lod].tiles[x][y].s = s;
                day_lods[lod].tiles[x][y].t = t;

                if (lod >= 2) {
                    night_lods[lod].tiles[x][y].s = s;
                    night_lods[lod].tiles[x][y].t = t;
                }

                day_lods[lod].tiles[x][y].load_source("earth", lod, si, ti);
                day_lods[lod].tiles[x][y].load_alpha_source("clouds", lod, si, ti);

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
    earth = GLTFObject::load("assets/earth.gltf");

    earth_tex_va = earth->vertex_array(0).attrib(3);
    earth_tex_va->format(2, GL_INT);
    earth_tex_va->data(nullptr, static_cast<size_t>(-1), GL_DYNAMIC_DRAW);

    init_lods();

    if (!gl::glext.has_extension(gl::BINDLESS_TEXTURE)) {
        day_tex = new gl::array_texture;
        day_tex->wrap(GL_MIRRORED_REPEAT);
        day_tex->format(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, 2048, 2048,
                        max_tex_per_type);

        night_tex = new gl::array_texture;
        night_tex->tmu() = 1;
        night_tex->wrap(GL_MIRRORED_REPEAT);
        night_tex->format(GL_COMPRESSED_RG_RGTC2, 2048, 2048, max_tex_per_type);
    }

    cloud_normal_map = new gl::texture("assets/cloud_normals.jpg");
    cloud_normal_map->tmu() = 3;
    cloud_normal_map->wrap(GL_REPEAT);


    if (gl::glext.has_extension(gl::BINDLESS_TEXTURE)) {
        earth_prg = new gl::program {gl::shader::vert("shaders/earth_vert.glsl"),
                                     gl::shader::frag("shaders/earth_frag.glsl")};

        cloud_prg = new gl::program {gl::shader::vert("shaders/cloud_vert.glsl"),
                                     gl::shader::frag("shaders/cloud_frag.glsl")};
    } else {
        earth_prg = new gl::program {gl::shader::vert("shaders/earth_vert.glsl"),
                                     gl::shader::frag("shaders/earth_nbl_frag.glsl")};

        cloud_prg = new gl::program {gl::shader::vert("shaders/cloud_vert.glsl"),
                                     gl::shader::frag("shaders/cloud_nbl_frag.glsl")};
    }

    atmob_prg = new gl::program {gl::shader::vert("shaders/ptn_vert.glsl"),
                                 gl::shader::frag("shaders/atmob_frag.glsl")};

    atmof_prg = new gl::program {gl::shader::vert("shaders/atmof_vert.glsl"),
                                 gl::shader::frag("shaders/atmof_frag.glsl")};

    atmoi_prg = new gl::program {gl::shader::vert("shaders/scratch_vert.glsl"),
                                 gl::shader::frag("shaders/atmoi_frag.glsl")};

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


    sun_prg = new gl::program {gl::shader::vert("shaders/sun_vert.glsl"),
                               gl::shader::frag("shaders/sun_frag.glsl")};

    sun_prg->bind_attrib("va_position", 0);
    sun_prg->bind_frag("out_col", 0);

    moon_prg = new gl::program {gl::shader::vert("shaders/moon_vert.glsl"),
                                gl::shader::frag("shaders/moon_frag.glsl")};

    moon_prg->bind_attrib("va_position", 0);
    moon_prg->bind_frag("out_col", 0);

    moon_tex = new gl::texture("assets/moon.png");


    sub_atmo_fbo = new gl::framebuffer(2);
    sub_atmo_fbo->color_format(0, GL_R11F_G11F_B10F);
    sub_atmo_fbo->color_format(1, GL_R8);
    sub_atmo_fbo->depth().tmu() = 1;
    (*sub_atmo_fbo)[1].tmu() = 2;


    if (global_options.aurora) {
        aurora_prg = new gl::program {gl::shader::vert("shaders/aurora_vert.glsl"),
                                      gl::shader::geom("shaders/aurora_geom.glsl"),
                                      gl::shader::frag("shaders/aurora_frag.glsl")};

        aurora_prg->bind_attrib("va_position", 0);
        aurora_prg->bind_attrib("va_texcoord", 1);
        aurora_prg->bind_attrib("va_strength", 2);
        aurora_prg->bind_frag("out_col", 0);

        aurora_bands = new gl::texture("assets/aurora-bands.png");
        aurora_bands->wrap(GL_REPEAT);
    }


    skybox = GLTFObject::load("assets/skybox.gltf");

    skybox_tex = new gl::cubemap;
    skybox_tex->format(GL_COMPRESSED_RGB_S3TC_DXT1_EXT, global_options.star_map_resolution,
                                                        global_options.star_map_resolution);
    for (const auto &p: {std::make_pair(gl::cubemap::TOP,    "top"),
                         std::make_pair(gl::cubemap::BOTTOM, "bottom"),
                         std::make_pair(gl::cubemap::RIGHT,  "right"),
                         std::make_pair(gl::cubemap::LEFT,   "left"),
                         std::make_pair(gl::cubemap::FRONT,  "front"),
                         std::make_pair(gl::cubemap::BACK,   "back")})
    {
        std::string fname = std::string("assets/skybox-") + p.second + "-"
                          + std::to_string(global_options.star_map_resolution) + ".png";

        gl::image img(gl::image(fname), gl::image::COMPRESSED_S3TC_DXT1);
        skybox_tex->load_layer(p.first, img);
    }

    skybox_prg = new gl::program {gl::shader::vert("shaders/skybox_vert.glsl"),
                                  gl::shader::frag("shaders/skybox_frag.glsl")};

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
    int uniform_indices[2] = {0};

    for (int lod = min_lod; lod <= max_lod; lod++) {
        for (int x = 0; x < day_lods[lod].horz_tiles; x++) {
            for (int y = 0; y < day_lods[lod].vert_tiles; y++) {
                day_lods[lod].tiles[x][y].refcount = 0;
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

            int lods[2] = {
                tile_lods[tx][ty],
                helper::maximum(tile_lods[tx][ty], 2)
            };

            Tile *tiles[2] = {nullptr};

            for (int type = 0; type < 2; type++) {
                LOD &lod = type == 0 ?   day_lods[lods[type]]
                                     : night_lods[lods[type]];

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

            indices[vertex][0] = tiles[0]->index;
            indices[vertex][1] = tiles[1]->index;

            vertex++;
        }
    }

    assert(vertex == earth->vertex_count(0));
}


static bool lod_loading_complete = false, lod_unloading_complete = false;


static void lod_load_images(void)
{
    for (int lod = min_lod; lod <= max_lod; lod++) {
        for (int x = 0; x < day_lods[lod].horz_tiles; x++) {
            for (int y = 0; y < day_lods[lod].vert_tiles; y++) {
                if (day_lods[lod].tiles[x][y].refcount) {
                    day_lods[lod].tiles[x][y].
                        load_image(gl::image::COMPRESSED_S3TC_DXT5);
                }
                if ((lod >= 2) && night_lods[lod].tiles[x][y].refcount) {
                    night_lods[lod].tiles[x][y].
                        load_image(gl::image::COMPRESSED_RGTC_RG);
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

                    earth_prg->uniform<fvec4>("day_texture_params[" + si + "]")
                        = fvec4(static_cast<float>(day_lods[lod].horz_tiles),
                                static_cast<float>(day_lods[lod].vert_tiles),
                                tile.s,
                                tile.t);
                }

                if ((lod >= 2) && night_lods[lod].tiles[x][y].refcount) {
                    Tile &tile = night_lods[lod].tiles[x][y];

                    std::string si(std::to_string(tile.index));
                    if (gl::glext.has_extension(gl::BINDLESS_TEXTURE)) {
                        earth_prg->uniform<gl::texture>("night_textures[" + si + "]") = *tile.texture;
                    }

                    earth_prg->uniform<fvec4>("night_texture_params[" + si +
                                              "]")
                        = fvec4(static_cast<float>(night_lods[lod].horz_tiles),
                                static_cast<float>(night_lods[lod].vert_tiles),
                                tile.s,
                                tile.t);
                }
            }
        }
    }

    for (int lod = min_lod; lod <= max_lod; lod++) {
        for (int x = 0; x < day_lods[lod].horz_tiles; x++) {
            for (int y = 0; y < day_lods[lod].vert_tiles; y++) {
                if (day_lods[lod].tiles[x][y].refcount) {
                    Tile &tile = day_lods[lod].tiles[x][y];

                    std::string si(std::to_string(tile.index));
                    if (gl::glext.has_extension(gl::BINDLESS_TEXTURE)) {
                        cloud_prg->uniform<gl::texture>("day_textures[" + si + "]") = *tile.texture;
                    }

                    cloud_prg->uniform<fvec4>("day_texture_params[" + si + "]")
                        = fvec4(static_cast<float>(day_lods[lod].horz_tiles),
                                static_cast<float>(day_lods[lod].vert_tiles),
                                tile.s,
                                tile.t);
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


static void update_lods(const GraphicsStatus &gstat, const fmat4 &cur_earth_mv,
                        bool update)
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

        memcpy(earth_tex_va->map(), indices, earth->vertex_count(0) * sizeof(indices[0]));
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

    fmat3 norm_mat = fmat3(cur_earth_mv).transposed_inverse();
    bool changed = false;

    int eff_min_lod = min_lod, eff_max_lod = max_lod;
    if (gstat.time_speed_up >= 50) {
        // With high speed-up you can't see the earth very good anyway. Fix the
        // LOD to a constant level so there are no load/unload spikes.
        eff_min_lod = helper::minimum(max_lod, helper::maximum(min_lod, 3));
        eff_max_lod = eff_min_lod;
    }

    std::vector<std::tuple<float, int, int>> lod_list;

    for (int x = 0; x < 32; x++) {
        float xa = (x + .5f) / 32.f * 2.f * static_cast<float>(M_PI);

        for (int y = 0; y < 16; y++) {
            float ya = (y + .5f) / 16.f * static_cast<float>(M_PI);

            fvec3 lnrm = fvec3(sinf(ya) * cosf(xa), cosf(ya), sinf(ya) * sinf(xa));
            fvec3 nrm = (norm_mat * lnrm).approx_normalized();

            float pos_dot = dotp(nrm, (fvec3)gstat.camera_position);
            if (pos_dot < 0.f) {
                if (tile_lods[x][y] != -1) {
                    changed = true;
                    tile_lods[x][y] = -1;
                }
                continue;
            } else {
                if (tile_lods[x][y] == -1) {
                    changed = true;
                    tile_lods[x][y] = eff_max_lod; // will be fixed right away
                }
            }

            lod_list.emplace_back(pos_dot, x, y);
        }
    }

    std::sort(lod_list.begin(), lod_list.end(), [](const std::tuple<float, int, int> &x,
                                                   const std::tuple<float, int, int> &y)
                                                {
                                                    return std::get<0>(x) > std::get<0>(y);
                                                });

    float cam_ground_dist = (gstat.camera_position.length() - 6371e3f) * 1e-3f;
    int base_lod = log2f(cam_ground_dist / gstat.width) + 1.f;

    if (base_lod < eff_min_lod) {
        base_lod = eff_min_lod;
    } else if (base_lod > eff_max_lod) {
        base_lod = eff_max_lod;
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

            if (base_lod < eff_max_lod) {
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
        indices = new vec<2, int32_t>[earth->vertex_count(0)];

        perform_lod_update(indices);

        loading_thread = new std::thread(lod_load_images);
    }
}


void draw_environment(const GraphicsStatus &status, const WorldState &world)
{
    const ShipState &ship = world.ships[world.player_ship];

    if (global_options.aurora) {
        // Load dynamic data first, so it can be copied over the course of the function
        if (aurora_vas.empty()) {
            for (size_t i = 0; i < world.auroras.size(); i++) {
                gl::vertex_array *aurora_va = new gl::vertex_array;
                aurora_va->set_elements(world.auroras[i].samples().size());

                aurora_va->attrib(0)->format(2);
                aurora_va->attrib(0)->data(nullptr,
                                           world.auroras[i].samples().size()
                                           * sizeof(Aurora::Sample),
                                           GL_DYNAMIC_DRAW, false);

                aurora_va->attrib(1)->format(1);
                aurora_va->attrib(1)->reuse_buffer(aurora_va->attrib(0));

                aurora_va->attrib(2)->format(1);
                aurora_va->attrib(2)->reuse_buffer(aurora_va->attrib(0));

                aurora_vas.push_back(aurora_va);
            }
        }

        for (size_t i = 0; i < aurora_vas.size(); i++) {
            memcpy(aurora_vas[i]->attrib(0)->map(),
                   world.auroras[i].samples().data(),
                   world.auroras[i].samples().size() * sizeof(Aurora::Sample));

            aurora_vas[i]->attrib(0)->unmap();

            aurora_vas[i]->attrib(0)->load(sizeof(Aurora::Sample),
                                           offsetof(Aurora::Sample, position));
            aurora_vas[i]->attrib(1)->load(sizeof(Aurora::Sample),
                                           offsetof(Aurora::Sample, texcoord));
            aurora_vas[i]->attrib(2)->load(sizeof(Aurora::Sample),
                                           offsetof(Aurora::Sample, strength));
        }
    }


    fmat4 cur_cloud_mv  = world.earth_mv.scaled(fvec3(6381e3f / 6371e3f));
    fmat4 cur_atmo_mv   = world.earth_mv.scaled(fvec3(6441e3f / 6371e3f));
    fmat4 cur_aurora_mv = world.earth_mv.scaled(fvec3(6421e3f / 6371e3f));


    static float lod_update_timer;

    lod_update_timer += world.interval;
    update_lods(status, world.earth_mv, lod_update_timer >= .2f);
    lod_update_timer = fmodf(lod_update_timer, .2f);


    float sa_zn = (status.camera_position.length() - 6350e3f) / 1.5f;
    float sa_zf =  status.camera_position.length() + 7000e3f;

    fmat4 sa_proj = fmat4::projection(status.yfov, status.aspect, sa_zn, sa_zf);


    // everything is in front of the skybox
    glDisable(GL_DEPTH_TEST);


    skybox_tex->bind();

    fmat4 skybox_proj = fmat4::projection(status.yfov, status.aspect,
                                          1000e3f, 1500e3f);
    fmat4 skybox_mv   = status.world_to_camera;
    skybox_mv[3][3] = 1.f;

    skybox_prg->use();
    skybox_prg->uniform<fmat4>("mat_mvp") = skybox_proj * skybox_mv;
    skybox_prg->uniform<gl::cubemap>("skybox") = *skybox_tex;

    skybox->draw();


    fvec4 sun_pos = status.world_to_camera *
                    fvec4::position(149.6e9f * -world.sun_light_dir);
    fvec4 projected_sun = status.projection * sun_pos;
    projected_sun /= projected_sun.w();

    if (sun_pos.z() < 0.f) {
        float sun_radius = atanf(696.e6f / sun_pos.length()) * 2.f / status.yfov;

        sun_prg->use();
        sun_prg->uniform<fvec2>("sun_position") = projected_sun;
        sun_prg->uniform<fvec2>("sun_size") = fvec2(sun_radius * status.height
                                                    / status.width, sun_radius);

        quad_vertices->draw(GL_TRIANGLE_STRIP);
    }


    fvec3 moon_right = crossp(status.camera_forward, fvec3(0.f, 1.f, 0.f))
                       .approx_normalized();
    fvec3 moon_up = crossp(moon_right, status.camera_forward);

    moon_tex->bind();

    moon_prg->use();
    moon_prg->uniform<fvec3>("moon_position") = world.moon_pos;
    moon_prg->uniform<fvec3>("right") = moon_right;
    moon_prg->uniform<fvec3>("up") = moon_up;
    moon_prg->uniform<fmat3>("mat_nrm") =
        fmat3::rotation_normalized(-world.moon_angle_to_sun,
                                   fvec3(0.f, 1.f, 0.f));
    moon_prg->uniform<fmat4>("mat_mvp") = status.projection
                                          * status.world_to_camera;
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
    atmob_prg->uniform<fmat4>("mat_mv") = cur_atmo_mv;
    atmob_prg->uniform<fmat4>("mat_proj") = sa_proj * status.world_to_camera;

    earth->draw();


    sub_atmo_fbo->unmask(1);
    sub_atmo_fbo->bind();

    glDepthFunc(GL_LEQUAL);
    glCullFace(GL_BACK);

    earth_prg->use();
    earth_prg->uniform<fmat4>("mat_mv") = world.earth_mv;
    earth_prg->uniform<fmat4>("mat_proj") = sa_proj * status.world_to_camera;
    earth_prg->uniform<fmat3>("mat_nrm") = fmat3(world.earth_mv)
                                           .transposed_inverse();
    earth_prg->uniform<fvec3>("cam_pos") = status.camera_position;
    earth_prg->uniform<fvec3>("light_dir") = world.sun_light_dir;

    if (!gl::glext.has_extension(gl::BINDLESS_TEXTURE)) {
        day_tex->bind();
        night_tex->bind();

        earth_prg->uniform<gl::array_texture>("day_texture") = *day_tex;
        earth_prg->uniform<gl::array_texture>("night_texture") = *night_tex;
    }

    earth->draw();


    sub_atmo_fbo->mask(1);
    sub_atmo_fbo->bind();

    float height = status.camera_position.length() - 6371e3f;

    glDepthMask(GL_FALSE);
    if (height < 10e3f) {
        glCullFace(GL_FRONT);
    }
    if (height > 8e3f && height < 12e3f) {
        glDisable(GL_CULL_FACE);
    }

    cloud_normal_map->bind();

    cloud_prg->use();
    cloud_prg->uniform<fmat4>("mat_mv") = cur_cloud_mv;
    cloud_prg->uniform<fmat4>("mat_proj") = sa_proj * status.world_to_camera;
    cloud_prg->uniform<fmat3>("mat_nrm") = fmat3(cur_cloud_mv)
                                           .transposed_inverse();
    cloud_prg->uniform<fvec3>("light_dir") = world.sun_light_dir;
    cloud_prg->uniform<gl::texture>("cloud_normal_map") = *cloud_normal_map;

    if (!gl::glext.has_extension(gl::BINDLESS_TEXTURE)) {
        day_tex->bind();
        cloud_prg->uniform<gl::array_texture>("day_texture") = *day_tex;
    }

    earth->draw();

    if (height < 10e3f) {
        glCullFace(GL_BACK);
    }
    if (height > 8e3f && height < 12e3f) {
        glEnable(GL_CULL_FACE);
    }
    glDepthMask(GL_TRUE);


    main_fbo->bind();

    (*sub_atmo_fbo)[0].bind();
    (*sub_atmo_fbo)[1].bind();
    sub_atmo_fbo->depth().bind();
    atmo_map->bind();

    bool in_atmosphere = status.camera_position.length() <= 6441e3f;

    gl::program *draw_earth_prg = in_atmosphere ? atmoi_prg : atmof_prg;

    draw_earth_prg->use();
    if (!in_atmosphere) {
        draw_earth_prg->uniform<fmat4>("mat_mv") = cur_atmo_mv;
        draw_earth_prg->uniform<fmat4>("mat_proj") = status.projection
                                                     * status.world_to_camera;
        draw_earth_prg->uniform<fmat3>("mat_nrm") = fmat3(cur_atmo_mv)
                                                    .transposed_inverse();
    } else {
        draw_earth_prg->uniform<fvec3>("cam_right") = ship.right;
        draw_earth_prg->uniform<fvec3>("cam_up") = ship.up;
        draw_earth_prg->uniform<float>("height") = height / 70e3f;
        draw_earth_prg->uniform<float>("tan_xhfov") = tanf(status.yfov / 2.f)
                                                      * status.aspect;
        draw_earth_prg->uniform<float>("tan_yhfov") = tanf(status.yfov / 2.f);
    }
    draw_earth_prg->uniform<fvec3>("cam_pos") = status.camera_position;
    draw_earth_prg->uniform<fvec3>("cam_fwd") = status.camera_forward;
    draw_earth_prg->uniform<fvec3>("light_dir") = world.sun_light_dir;
    draw_earth_prg->uniform<fvec2>("sa_z_dim") = fvec2(sa_zn, sa_zf);
    draw_earth_prg->uniform<fvec2>("screen_dim") = fvec2(status.width,
                                                         status.height);
    draw_earth_prg->uniform<gl::texture>("color") = (*sub_atmo_fbo)[0];
    draw_earth_prg->uniform<gl::texture>("stencil") = (*sub_atmo_fbo)[1];
    draw_earth_prg->uniform<gl::texture>("depth") = sub_atmo_fbo->depth();
    draw_earth_prg->uniform<gl::texture>("atmo_map") = *atmo_map;

    if (in_atmosphere) {
        glDepthMask(GL_FALSE);
        quad_vertices->draw(GL_TRIANGLE_STRIP);
        glDepthMask(GL_TRUE);
    } else {
        earth->draw();
    }


    if (global_options.aurora) {
        aurora_bands->bind();
        (*sub_atmo_fbo)[1].bind();
        sub_atmo_fbo->depth().bind();

        aurora_prg->use();
        aurora_prg->uniform<fmat4>("mat_mv") = cur_aurora_mv;
        aurora_prg->uniform<fmat4>("mat_proj") = status.projection
                                                 * status.world_to_camera;
        aurora_prg->uniform<fvec3>("cam_pos") = status.camera_position;
        aurora_prg->uniform<fvec3>("cam_fwd") = status.camera_forward;
        aurora_prg->uniform<fvec2>("screen_dim") = fvec2(status.width,
                                                         status.height);
        aurora_prg->uniform<fvec2>("sa_z_dim") = fvec2(sa_zn, sa_zf);
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
}
