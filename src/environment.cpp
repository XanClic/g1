#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <tuple>
#include <vector>

#include <dake/dake.hpp>

#include "environment.hpp"
#include "graphics.hpp"
#include "xsmd.hpp"


using namespace dake;
using namespace dake::math;


#define EARTH_HORZ 256
#define EARTH_VERT 128

// type \in \{ day, night, clouds \}
#define MAX_TEX_PER_TYPE 20


static XSMD *earth;
static gl::program *earth_prg, *cloud_prg, *atmob_prg, *atmof_prg, *sun_prg;
static gl::framebuffer *sub_atmo_fbo;
static gl::vertex_array *sun_va;
static gl::vertex_attrib *earth_tex_va;
static mat4 earth_mv, cloud_mv, atmo_mv;


struct Tile {
    float s = 0.f, t = 0.f;

    void *source = nullptr;
    size_t source_size = 0;

    gl::image *image = nullptr;
    gl::texture *texture = nullptr;

    int refcount = 0;
    int index = 0;

    void load(void);
    void unload(void);

    void load_source(const char *name, int lod, int si, int ti);
};


struct LOD {
    int total_width, total_height;
    int horz_tiles, vert_tiles;

    std::vector<std::vector<Tile>> tiles;
};


std::vector<LOD> day_lods, night_lods, cloud_lods;
std::vector<std::vector<int>> tile_lods;


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


void Tile::load(void)
{
    if (image) {
        return;
    }

    image = new gl::image(source, source_size);
    texture = new gl::texture(*image);
    texture->bind();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    texture->make_bindless();
}


void Tile::unload(void)
{
    delete image;
    delete texture;

    image = nullptr;
    texture = nullptr;
}


static void resize(unsigned w, unsigned h);


void init_environment(void)
{
    earth = load_xsmd("assets/earth.xsmd");
    earth->make_vertex_array();

    earth_tex_va = earth->va->attrib(3);
    earth_tex_va->format(2, GL_INT);
    earth_tex_va->data(nullptr, static_cast<size_t>(-1), GL_DYNAMIC_DRAW);

    day_lods.resize(9);
    night_lods.resize(9);
    cloud_lods.resize(9);

    for (int lod = 0; lod <= 8; lod++) {
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


    sun_va = new gl::vertex_array;
    sun_va->set_elements(4);

    vec2 sun_vertex_positions[] = {
        vec2(-1.f, 1.f), vec2(-1.f, -1.f), vec2(1.f, 1.f), vec2(1.f, -1.f)
    };

    gl::vertex_attrib *sun_pos = sun_va->attrib(0);
    sun_pos->format(2);
    sun_pos->data(sun_vertex_positions);
    sun_pos->load();


    earth_mv = mat4::identity().scale(vec3(6378.f, 6357.f, 6378.f));
    cloud_mv = mat4::identity().scale(vec3(6388.f, 6367.f, 6388.f));
    atmo_mv  = mat4::identity().scale(vec3(6478.f, 6457.f, 6478.f));


    earth_prg = new gl::program {gl::shader::vert("assets/earth_vert.glsl"), gl::shader::frag("assets/earth_frag.glsl")};
    cloud_prg = new gl::program {gl::shader::vert("assets/earth_vert.glsl"), gl::shader::frag("assets/cloud_frag.glsl")};
    atmob_prg = new gl::program {gl::shader::vert("assets/ptn_vert.glsl"),   gl::shader::frag("assets/atmob_frag.glsl")};
    atmof_prg = new gl::program {gl::shader::vert("assets/atmof_vert.glsl"), gl::shader::frag("assets/atmof_frag.glsl")};

    earth->bind_program_vertex_attribs(*earth_prg);
    earth->bind_program_vertex_attribs(*cloud_prg);
    earth->bind_program_vertex_attribs(*atmob_prg);
    earth->bind_program_vertex_attribs(*atmof_prg);

    earth_prg->bind_attrib("va_textures", 3);
    cloud_prg->bind_attrib("va_textures", 3);

    earth_prg->bind_frag("out_col", 0);
    cloud_prg->bind_frag("out_col", 0);
    atmob_prg->bind_frag("out_col", 0);
    atmof_prg->bind_frag("out_col", 0);


    sun_prg = new gl::program {gl::shader::vert("assets/sun_vert.glsl"), gl::shader::frag("assets/sun_frag.glsl")};
    sun_prg->bind_attrib("va_position", 0);
    sun_prg->bind_frag("out_col", 0);


    sub_atmo_fbo = new gl::framebuffer(1);
    sub_atmo_fbo->depth().tmu() = 1;


    register_resize_handler(resize);
}


static void resize(unsigned w, unsigned h)
{
    sub_atmo_fbo->resize(w, h);
}


static void update_lods(const GraphicsStatus &gstat)
{
    mat3 norm_mat = mat3(earth_mv).transposed_inverse();
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
                if (tile_lods[x][y] != 8) {
                    changed = true;
                    tile_lods[x][y] = 8;
                }
                continue;
            }

            lod_list.emplace_back(pos_dot, x, y);
        }
    }

    std::sort(lod_list.begin(), lod_list.end(), [](const std::tuple<float, int, int> &x, const std::tuple<float, int, int> &y) { return std::get<0>(x) > std::get<0>(y); });

    float cam_ground_dist = gstat.camera_position.length() - 6357.f;
    int base_lod = (log2f(cam_ground_dist) - 8.f) / 1.5f;

    if (base_lod < 0) {
        base_lod = 0;
    } else if (base_lod > 8) {
        base_lod = 8;
    }

    int lod_tiles = 1 << base_lod, lod_tiles_i = 0;

    for (auto &tile: lod_list) {
        int x = std::get<1>(tile), y = std::get<2>(tile);

        if (tile_lods[x][y] != base_lod) {
            changed = true;
            tile_lods[x][y] = base_lod;
        }

        if (++lod_tiles_i >= lod_tiles) {
            lod_tiles_i = 0;
            lod_tiles *= 4;

            if (base_lod < 8) {
                base_lod++;
            }
        }
    }

    if (changed) {
        vec<2, int32_t> *mapping = static_cast<vec<2, int32_t> *>(earth_tex_va->map());
        uint64_t vertex = 0;
        int uniform_indices[3] = {0};

        for (int lod = 0; lod <= 8; lod++) {
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
                            // FIXME this is not working
                            if (!retrying && (uniform_indices[type] >= MAX_TEX_PER_TYPE)) {
                                tile.refcount = 0;
                                lods[type] = type == 1 ? 2 : 0;
                                continue;
                            } else if (retrying) {
                                tile.refcount = 0;
                                if (++lods[type] >= 9) {
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

                mapping[vertex][0] = tiles[0]->index;
                mapping[vertex][1] = tiles[1]->index;

                vertex++;
            }
        }

        assert(vertex == earth->hdr->vertex_count);

        earth_tex_va->unmap();
        earth_tex_va->load();

        for (int lod = 0; lod <= 8; lod++) {
            for (int x = 0; x < day_lods[lod].horz_tiles; x++) {
                for (int y = 0; y < day_lods[lod].vert_tiles; y++) {
                    if (!day_lods[lod].tiles[x][y].refcount) {
                        day_lods[lod].tiles[x][y].unload();
                    }
                    if ((lod >= 2) && !night_lods[lod].tiles[x][y].refcount) {
                        night_lods[lod].tiles[x][y].unload();
                    }
                    if (!cloud_lods[lod].tiles[x][y].refcount) {
                        cloud_lods[lod].tiles[x][y].unload();
                    }
                }
            }
        }

        for (int lod = 0; lod <= 8; lod++) {
            for (int x = 0; x < day_lods[lod].horz_tiles; x++) {
                for (int y = 0; y < day_lods[lod].vert_tiles; y++) {
                    if (day_lods[lod].tiles[x][y].refcount) {
                        Tile &tile = day_lods[lod].tiles[x][y];

                        tile.load();

                        std::string si(std::to_string(tile.index));
                        earth_prg->uniform<gl::texture>("day_textures[" + si + "]") = *tile.texture;
                        earth_prg->uniform<vec4>("day_texture_params[" + si + "]") = vec4(static_cast<float>(day_lods[lod].horz_tiles), static_cast<float>(day_lods[lod].vert_tiles), tile.s, tile.t);
                    }

                    if ((lod >= 2) && night_lods[lod].tiles[x][y].refcount) {
                        Tile &tile = night_lods[lod].tiles[x][y];

                        tile.load();

                        std::string si(std::to_string(tile.index));
                        earth_prg->uniform<gl::texture>("night_textures[" + si + "]") = *tile.texture;
                        earth_prg->uniform<vec4>("night_texture_params[" + si + "]") = vec4(static_cast<float>(night_lods[lod].horz_tiles), static_cast<float>(night_lods[lod].vert_tiles), tile.s, tile.t);
                    }
                }
            }
        }

        for (int lod = 0; lod <= 8; lod++) {
            for (int x = 0; x < day_lods[lod].horz_tiles; x++) {
                for (int y = 0; y < day_lods[lod].vert_tiles; y++) {
                    if (cloud_lods[lod].tiles[x][y].refcount) {
                        Tile &tile = cloud_lods[lod].tiles[x][y];

                        tile.load();

                        std::string si(std::to_string(tile.index));
                        cloud_prg->uniform<gl::texture>("cloud_textures[" + si + "]") = *tile.texture;
                        cloud_prg->uniform<vec4>("cloud_texture_params[" + si + "]") = vec4(static_cast<float>(cloud_lods[lod].horz_tiles), static_cast<float>(cloud_lods[lod].vert_tiles), tile.s, tile.t);
                    }
                }
            }
        }
    }
}


void draw_environment(const GraphicsStatus &status)
{
    atmo_mv .rotate(.0004f, vec3(0.f, 1.f, 0.f));
    earth_mv.rotate(.0004f, vec3(0.f, 1.f, 0.f));
    cloud_mv.rotate(.0004f, vec3(0.f, 1.f, 0.f));


    update_lods(status);


    static float step = static_cast<float>(M_PI);

    vec3 light_dir = vec3(sinf(step), 0.f, cosf(step));
    step -= .001f;


    float sa_zn = (status.camera_position.length() - 6400.f) / 1.5f;
    float sa_zf =  status.camera_position.length() + 7000.f;

    mat4 sa_proj = mat4::projection(status.yfov, static_cast<float>(status.width) / status.height, sa_zn, sa_zf);


    vec4 sun_pos = 149.6e6f * -light_dir;
    sun_pos.w() = 1.f;
    sun_pos = status.world_to_camera * sun_pos;

    vec4 projected_sun = status.projection * sun_pos;
    projected_sun /= projected_sun.w();

    if (sun_pos.z() < 0.f) {
        float sun_radius = atanf(696.e3f / sun_pos.length()) * 2.f / status.yfov;

        // artificial correction (pre-blur)
        sun_radius *= 3.f;

        // everything is in front of the sun
        glDisable(GL_DEPTH_TEST);

        sun_prg->use();
        sun_prg->uniform<vec2>("sun_position") = projected_sun;
        sun_prg->uniform<vec2>("sun_size") = vec2(sun_radius * status.height / status.width, sun_radius);

        sun_va->draw(GL_TRIANGLE_STRIP);

        glEnable(GL_DEPTH_TEST);
    }


    gl::framebuffer *main_fbo = gl::framebuffer::current();

    sub_atmo_fbo->bind();
    glClearDepth(0.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearDepth(1.f);


    glDepthFunc(GL_ALWAYS);
    glCullFace(GL_FRONT);

    atmob_prg->use();
    atmob_prg->uniform<mat4>("mat_mv") = atmo_mv;
    atmob_prg->uniform<mat4>("mat_proj") = sa_proj * status.world_to_camera;

    earth->draw();


    glDepthFunc(GL_LEQUAL);
    glCullFace(GL_BACK);

    earth_prg->use();
    earth_prg->uniform<mat4>("mat_mv") = earth_mv;
    earth_prg->uniform<mat4>("mat_proj") = sa_proj * status.world_to_camera;
    earth_prg->uniform<mat3>("mat_nrm") = mat3(earth_mv).transposed_inverse();
    earth_prg->uniform<vec3>("cam_pos") = status.camera_position;
    earth_prg->uniform<vec3>("light_dir") = light_dir;

    earth->draw();


    glDepthMask(GL_FALSE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);

    cloud_prg->use();
    cloud_prg->uniform<mat4>("mat_mv") = cloud_mv;
    cloud_prg->uniform<mat4>("mat_proj") = sa_proj * status.world_to_camera;
    cloud_prg->uniform<mat3>("mat_nrm") = mat3(cloud_mv).transposed_inverse();
    cloud_prg->uniform<vec3>("light_dir") = light_dir;

    earth->draw();

    glDepthMask(GL_TRUE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);


    main_fbo->bind();

    (*sub_atmo_fbo)[0].bind();
    sub_atmo_fbo->depth().bind();

    atmof_prg->use();
    atmof_prg->uniform<mat4>("mat_mv") = atmo_mv;
    atmof_prg->uniform<mat4>("mat_proj") = status.projection * status.world_to_camera;
    atmof_prg->uniform<mat3>("mat_nrm") = mat3(atmo_mv).transposed_inverse();
    atmof_prg->uniform<vec3>("cam_pos") = status.camera_position;
    atmof_prg->uniform<vec3>("cam_fwd") = status.camera_forward;
    atmof_prg->uniform<vec3>("light_dir") = light_dir;
    atmof_prg->uniform<vec2>("sa_z_dim") = vec2(sa_zn, sa_zf);
    atmof_prg->uniform<vec2>("screen_dim") = vec2(status.width, status.height);
    atmof_prg->uniform<gl::texture>("color") = (*sub_atmo_fbo)[0];
    atmof_prg->uniform<gl::texture>("depth") = sub_atmo_fbo->depth();

    earth->draw();
}
