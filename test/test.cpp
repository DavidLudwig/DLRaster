// test.cpp - an interactive test program for the "DLRaster" 2d rasterization library; BUGGY!
// Public domain, Sep 25 2016, David Ludwig, dludwig@pobox.com. See "unlicense" statement at the end of this file.

// Disable deprecated C function warnings from MSVC:
#define _CRT_SECURE_NO_WARNINGS

#include <SDL.h>
// Make sure SDL doesn't redefine main
#undef main
#define GL_GLEXT_PROTOTYPES
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>
#pragma comment(lib, "opengl32.lib")
//#include <random>
//#include <mutex>
#include <stdio.h>

#define GB_MATH_IMPLEMENTATION
#include "gb_math.h"

#define DL_RASTER_IMPLEMENTATION
#include "DL_Raster.h"

typedef enum DLR_Test_RendererType {
    DLRTEST_TYPE_SOFTWARE = 0,
    DLRTEST_TYPE_OPENGL1
} DLR_TestRenderer;

enum DLRTest_Env_Flags {
    DLRTEST_ENV_DEFAULTS    = 0,
    DLRTEST_ENV_COMPARE     = (1 << 0),
};

struct DLRTest_Env {
    DLR_Test_RendererType type;
    const char * window_title;  // window initial title
    int window_x;               // window initial X
    int window_y;               // window initial Y
    int flags;                  // DLRTest_Env_Flags
    SDL_Window * window;        // the SDL_Window, set on app-init (to a valid, non-NULL SDL_Window *)
    union {
        struct {
            SDL_Surface * bg;           // background surface (in main memory)
            SDL_Texture * bgTex;        // background texture (near/on GPU)
            SDL_Texture * crosshairs;   // crosshairs texture, to display locked position
        } sw;
        struct {
            SDL_GLContext gl;       // OpenGL context
            SDL_Surface * exported; // Exported pixels
        } gl;
    } inner;
};

static int winW = 460;
static int winH = 400;

static DLRTest_Env envs[] = {
    {
        DLRTEST_TYPE_SOFTWARE,
        "DLRTest SW",
        0, 60,              // window initial X and Y
        0,
    },
    {
        DLRTEST_TYPE_OPENGL1,
        "DLRTest OGL",
        winW + 10, 60,            // window initial X and Y
        0,
    },
    {
        DLRTEST_TYPE_SOFTWARE,
        "DLRTest CMP",
        (winW + 10) * 2, 60,            // window initial X and Y
        DLRTEST_ENV_COMPARE,
    },
};

static const int num_envs = SDL_arraysize(envs);

enum DLRTest_Compare : Uint8 {
    DLRTEST_COMPARE_A       = (1 << 0),
    DLRTEST_COMPARE_R       = (1 << 1),
    DLRTEST_COMPARE_G       = (1 << 2),
    DLRTEST_COMPARE_B       = (1 << 3),
    DLRTEST_COMPARE_RGB     = (DLRTEST_COMPARE_R | DLRTEST_COMPARE_G | DLRTEST_COMPARE_B),
    DLRTEST_COMPARE_ARGB    = (DLRTEST_COMPARE_A | DLRTEST_COMPARE_R | DLRTEST_COMPARE_G | DLRTEST_COMPARE_B),
};
static Uint8 compare = DLRTEST_COMPARE_ARGB;
static int compare_threshold = 1;

PFNGLBLENDFUNCSEPARATEPROC _glBlendFuncSeparate = NULL;

#define DLRTest_CheckGL() { GLenum glerr = glGetError(); SDL_assert(glerr == GL_NO_ERROR); }

SDL_Surface * DLRTest_GetSurfaceForView(DLRTest_Env * env)
{
    switch (env->type) {
        case DLRTEST_TYPE_SOFTWARE: {
            return env->inner.sw.bg;
        } break;

        case DLRTEST_TYPE_OPENGL1: {
            SDL_GL_MakeCurrent(env->window, env->inner.gl.gl);
            SDL_Surface * output = env->inner.gl.exported;
            if (output) {
                if (output->flags & SDL_PREALLOC) {
                    free(output->pixels);
                }
                SDL_FreeSurface(output);
                env->inner.gl.exported = output = NULL;
            }
            uint8_t * srcPixels = (uint8_t *) malloc(winW * winH * 4);
            glReadPixels(
                0, 0,
                winW, winH,
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                srcPixels
            );
            DLRTest_CheckGL();
            uint8_t * flippedPixels = (uint8_t *) calloc(winW * winH * 4, 1);
            int pitch = winW * 4;
            for (int y = 0; y < winH; ++y) {
                memcpy(
                    flippedPixels + ((winH - y - 1) * pitch),
                    srcPixels + (y * pitch), 
                    pitch
                );
            }
            free(srcPixels);
            SDL_Surface * tmp = SDL_CreateRGBSurfaceFrom(
                flippedPixels,
                winW, winH,
                32,
                pitch,
                0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000
            );
            SDL_PixelFormat * targetFormat = SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888);
            output = SDL_ConvertSurface(tmp, targetFormat, 0);
            SDL_FreeSurface(tmp);
            if (output->pixels != flippedPixels) {
                free(flippedPixels);
            }
            SDL_FreeFormat(targetFormat);
            env->inner.gl.exported = output;
            return output;
        } break;
    }

    return NULL;
}

static int lockedX = -1;
static int lockedY = -1;

static void DLRTest_UpdateWindowTitles()
{
    const char * compare_name = "";
    switch (compare) {
        case DLRTEST_COMPARE_ARGB: {
            compare_name = "argb";
        } break;
        case DLRTEST_COMPARE_A: {
            compare_name = "a";
        } break;
        case DLRTEST_COMPARE_R: {
            compare_name = "r";
        } break;
        case DLRTEST_COMPARE_G: {
            compare_name = "g";
        } break;
        case DLRTEST_COMPARE_B: {
            compare_name = "b";
        } break;
        case DLRTEST_COMPARE_RGB: {
            compare_name = "rgb";
        } break;
    }

#define DLRTEST_IS_LOCKED() (lockedX >= 0 && lockedY >= 0)

    int curX, curY;
    if (DLRTEST_IS_LOCKED()) {
        curX = lockedX;
        curY = lockedY;
    } else {
        if (SDL_GetMouseFocus()) {
            SDL_GetMouseState(&curX, &curY);
        } else {
            curX = curY = -1;
        }
    }

    char window_title[128];
    for (int i = 0; i < num_envs; ++i) {
        SDL_Surface * bg = DLRTest_GetSurfaceForView(&envs[i]);
        Uint32 c = (curX < 0) ? 0 : DLR_GetPixel32(bg->pixels, bg->pitch, 4, curX, curY);
        Uint8 a, r, g, b;
        DLR_SplitARGB32(c, a, r, g, b);
        
        const char * lockedText = DLRTEST_IS_LOCKED() ? ",lock" : "";

        if (envs[i].flags & DLRTEST_ENV_COMPARE) {
            SDL_snprintf(window_title, SDL_arraysize(window_title), "%s %s +/-:%d p:%d,%d%s a,rgb:%02x,%02x%02x%02x",
                envs[i].window_title, compare_name, compare_threshold, curX, curY, lockedText,
                a, r, g, b);
        } else {
            SDL_snprintf(window_title, SDL_arraysize(window_title), "%s p:(%d,%d)%s a,rgb:0x%02x,0x%02x%02x%02x",
                envs[i].window_title, curX, curY, lockedText,
                a, r, g, b);
        }
        window_title[SDL_arraysize(window_title)-1] = '\0';
        SDL_SetWindowTitle(envs[i].window, window_title);
    }
}

void DLRTest_DrawTriangles_OpenGL1(
    DLR_State * state,
    DLR_Vertex * vertices,
    size_t vertexCount)
{
    DLRTest_CheckGL();
    glCullFace(GL_FRONT_AND_BACK);
    DLRTest_CheckGL();

    switch (state->blendMode) {
        case DLR_BLENDMODE_NONE: {
            glDisable(GL_BLEND);
            DLRTest_CheckGL();
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
            DLRTest_CheckGL();
        } break;
        case DLR_BLENDMODE_BLEND: {
            glEnable(GL_BLEND);
            DLRTest_CheckGL();
            //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            _glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            DLRTest_CheckGL();
            //glAlphaFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            //DLRTest_CheckGL();
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            DLRTest_CheckGL();
        } break;
    }

    static GLuint tex = 0;
    if (tex == 0) {
        glGenTextures(1, &tex);
        DLRTest_CheckGL();
    }
    if ( ! state->texture) {
        glDisable(GL_TEXTURE_2D);
        DLRTest_CheckGL();
        glBindTexture(GL_TEXTURE_2D, 0);
        DLRTest_CheckGL();
    } else {
        glEnable(GL_TEXTURE_2D);
        DLRTest_CheckGL();
        glBindTexture(GL_TEXTURE_2D, tex);
        DLRTest_CheckGL();
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        DLRTest_CheckGL();
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        DLRTest_CheckGL();
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        DLRTest_CheckGL();
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        DLRTest_CheckGL();

        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA8,
            state->texture->w,
            state->texture->h,
            0,
            GL_BGRA,
            //GL_RGBA,
            GL_UNSIGNED_BYTE,
            state->texture->pixels
        );
        DLRTest_CheckGL();
    }

    glBegin(GL_TRIANGLES);
    for (size_t i = 0; i < vertexCount; ++i) {
        glColor4d(vertices[i].r, vertices[i].g, vertices[i].b, vertices[i].a);
        glTexCoord2d(vertices[i].uv, vertices[i].uw);
        glVertex2d(vertices[i].x, vertices[i].y);
    }
    glEnd();
}

void DLRTest_Clear(
    DLR_TestRenderer renderer,
    DLR_State * state,
    Uint32 color)
{
    switch (renderer) {
        case DLRTEST_TYPE_SOFTWARE: {
            DLR_Clear(state, color);
        } return;
        case DLRTEST_TYPE_OPENGL1: {
            int na, nr, ng, nb;
            DLR_SplitARGB32(color, na, nr, ng, nb);
            float fa, fr, fg, fb;
            fa = (na / 255.f);
            fr = (nr / 255.f);
            fg = (ng / 255.f);
            fb = (nb / 255.f);
            glClearColor(fr, fg, fb, fa);
            glClear(GL_COLOR_BUFFER_BIT);
        } return;
    }
}

void DLRTest_DrawTriangles(
    DLR_TestRenderer renderer,
    DLR_State * state,
    DLR_Vertex * vertices,
    size_t vertexCount)
{
    switch (renderer) {
        case DLRTEST_TYPE_SOFTWARE:
            DLR_DrawTriangles(state, vertices, vertexCount);
            return;
        case DLRTEST_TYPE_OPENGL1:
            DLRTest_DrawTriangles_OpenGL1(state, vertices, vertexCount);
            return;
    }
}

void DLRTest_DrawScene(DLRTest_Env * env)
{
    SDL_Surface * bg = (env->type == DLRTEST_TYPE_SOFTWARE) ? env->inner.sw.bg : NULL;

    {
        static DLR_State state;
        state.dest = bg;
        DLRTest_Clear(env->type, &state, 0xff000000);
    }

    //DLR_Vertex a = {100.f, 100.f, 1, 0, 0};
    //DLR_Vertex b = {500.f, 300.f, 0, 1, 0};
    //DLR_Vertex c = {300.f, 500.f, 0, 0, 1};
    //DLRTest_DrawTriangle(bg, a, b, c);

    //
    // Multi-color triangle
    //
    if (1) {
        static DLR_State state;
        static int didInitState = 0;
        if (!didInitState) {
            memset(&state, 0, sizeof(state));
            didInitState = 1;
        }
        state.dest = bg;

        double originX = 20;
        double originY = 20;
        struct { float a, r, g, b; } c[] = {
#if 1
            {1, 1, 0, 0},
            {1, 0, 1, 0},
            {1, 0, 0, 1},
#else
            {1, 1, 1, 1},
            {1, 1, 1, 1},
            {1, 1, 1, 1},
#endif
        };
        DLR_Vertex vertices[] = {
            {originX      , originY      ,    c[0].a, c[0].r, c[0].g, c[0].b,   0, 0},
            {originX + 300, originY + 150,    c[1].a, c[1].r, c[1].g, c[1].b,   0, 0},
            {originX + 150, originY + 300,    c[2].a, c[2].r, c[2].g, c[2].b,   0, 0},
        };
        DLRTest_DrawTriangles(env->type, &state, vertices, SDL_arraysize(vertices));
    }

    //
    // Textured-Square
    //
    if (1) {
        static DLR_State state;
        static int didInitState = 0;
        if (!didInitState) {
            memset(&state, 0, sizeof(state));

            enum DLRTest_TextureType {
                DLRTEST_TEXTURE_FROM_IMAGE          = 0,
                DLRTEST_TEXTURE_NONE                = 1,
                DLRTEST_TEXTURE_ALL_WHITE           = 2,
                DLRTEST_TEXTURE_VERTICAL_GRADIENT   = 3,
                DLRTEST_TEXTURE_HORIZONTAL_GRADIENT = 4,
            };
            static const DLRTest_TextureType textureType = DLRTEST_TEXTURE_FROM_IMAGE;
            //static const DLRTest_TextureType textureType = (DLRTest_TextureType) 1;
            switch (textureType) {
                case DLRTEST_TEXTURE_FROM_IMAGE: {
                    state.texture = SDL_LoadBMP("texture.bmp");
                    if ( ! state.texture) {
                        state.texture = SDL_LoadBMP("../../texture.bmp");   // for OSX, via test-DLRaster Xcode project
                    }
                } break;

                case DLRTEST_TEXTURE_NONE: {
                    state.texture = NULL;
                } break;

                case DLRTEST_TEXTURE_ALL_WHITE: {
                    state.texture = SDL_CreateRGBSurface(0, 256, 256, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
                    SDL_FillRect(state.texture, NULL, 0x88888888);
                } break;

                case DLRTEST_TEXTURE_VERTICAL_GRADIENT: {
                    state.texture = SDL_CreateRGBSurface(0, 256, 256, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
                    for (int y = 0; y < 256; ++y) {
                        for (int x = 0; x < 256; ++x) {
                            Uint32 c = DLR_JoinARGB32(0xff, y, y, y);
                            //Uint32 c = (y == 100) ? 0xffffffff : 0x88888888;
                            DLR_SetPixel32(state.texture->pixels, state.texture->pitch, 4, x, y, c);
                        }
                    }
                } break;

                case DLRTEST_TEXTURE_HORIZONTAL_GRADIENT: {
                    state.texture = SDL_CreateRGBSurface(0, 256, 256, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
                    for (int y = 0; y < 256; ++y) {
                        for (int x = 0; x < 256; ++x) {
                            Uint32 c = DLR_JoinARGB32(0xff, x, x, x);
                            DLR_SetPixel32(state.texture->pixels, state.texture->pitch, 4, x, y, c);
                        }
                    }
                } break;
            }

            didInitState = 1;
        }
        state.dest = bg;
        state.textureModulate = DLR_TEXTUREMODULATE_COLOR;
        state.blendMode = DLR_BLENDMODE_BLEND;

        int texW, texH;
        if (state.texture) {
            texW = state.texture->w;
            texH = state.texture->h;
        } else {
            texW = texH = 256;
        }

        double originX = 200.51;
        double originY = 20.;
        struct { float a, r, g, b; } c[] = {
#if 1    // multi-color
            {   1,   1, 0.3, 0.3},  // left  top
            {   1, 0.3,   1, 0.3},  // right top
            {   1, 0.3, 0.3,   1},  // right bottom
            { 0.5,   1,   1,   1},  // left  bottom
#elif 0  // this one is slightly-off in OpenGL
            { 0.5,   0,   1,   0},  // left  top
            { 0.5,   1,   0,   0},  // right top (X)
            { 0.5,   1,   0,   0},  // right bottom
            { 0.5,   0,   1,   0},  // left  bottom
#else    // all white
            {   1,   1,   1,   1},  // left  top
            {   1,   1,   1,   1},  // right top
            {   1,   1,   1,   1},  // right bottom
            {   1,   1,   1,   1},  // left  bottom
#endif
        };

        DLR_Vertex vertices[] = {
#if 1
            // TRIANGLE: top-right
            {originX       , originY,           c[0].a, c[0].r, c[0].g, c[0].b,    0, 0},    // left  top
            {originX + texW, originY,           c[1].a, c[1].r, c[1].g, c[1].b,    1, 0},    // right top
            {originX + texW, originY + texH,    c[2].a, c[2].r, c[2].g, c[2].b,    1, 1},    // right bottom
#endif

#if 1
            // TRIANGLE: bottom-left
            {originX + texW, originY + texH,    c[2].a, c[2].r, c[2].g, c[2].b,    1, 1},    // right bottom
            {originX       , originY + texH,    c[3].a, c[3].r, c[3].g, c[3].b,    0, 1},    // left  bottom
            {originX       , originY       ,    c[0].a, c[0].r, c[0].g, c[0].b,    0, 0},    // left  top
#endif
        };
        DLRTest_DrawTriangles(env->type, &state, vertices, SDL_arraysize(vertices));
    }
}

void DLR_Test_ProcessEvent(const SDL_Event & e)
{
    switch (e.type) {
        case SDL_MOUSEMOTION: {
            DLRTest_UpdateWindowTitles();
        } break;
        
        case SDL_MOUSEBUTTONDOWN: {
            if (DLRTEST_IS_LOCKED()) {
                lockedX = -1;
                lockedY = -1;
            } else {
                lockedX = e.button.x;
                lockedY = e.button.y;
            }
            DLRTest_UpdateWindowTitles();
        } break;

        case SDL_KEYDOWN: {
            switch (e.key.keysym.sym) {
                case SDLK_q:
                case SDLK_ESCAPE: {
                    exit(0);
                } break;
                
                case SDLK_RIGHT: if (DLRTEST_IS_LOCKED()) { lockedX = SDL_min(lockedX + 1, winW - 1); DLRTest_UpdateWindowTitles(); } break;
                case SDLK_LEFT:  if (DLRTEST_IS_LOCKED()) { lockedX = SDL_max(lockedX - 1,        0); DLRTest_UpdateWindowTitles(); } break;
                case SDLK_DOWN:  if (DLRTEST_IS_LOCKED()) { lockedY = SDL_min(lockedY + 1, winH - 1); DLRTest_UpdateWindowTitles(); } break;
                case SDLK_UP:    if (DLRTEST_IS_LOCKED()) { lockedY = SDL_max(lockedY - 1,        0); DLRTest_UpdateWindowTitles(); } break;

                case SDLK_0:
                case SDLK_1:
                case SDLK_2:
                case SDLK_3:
                case SDLK_4:
                case SDLK_5:
                case SDLK_6:
                case SDLK_7:
                case SDLK_8:
                case SDLK_9: {
                    if (e.key.keysym.mod & KMOD_CTRL) {
                        char file[32];
                        for (int i = 0; i < num_envs; ++i) {
                            SDL_snprintf(file, sizeof(file), "output%s-%d.bmp", SDL_GetKeyName(e.key.keysym.sym), i);
                            SDL_Surface * output = DLRTest_GetSurfaceForView(&envs[i]);
                            if (output) {
                                if (SDL_SaveBMP(output, file) == 0) {
                                    SDL_Log("Saved %s", file);
                                } else {
                                    SDL_Log("Unable to save %s: \"%s\"", file, SDL_GetError());
                                }
                            }
                        }
                    } else {
                        compare_threshold = SDL_atoi(SDL_GetKeyName(e.key.keysym.sym));
                        DLRTest_UpdateWindowTitles();
                    }
                } break;

                case SDLK_c: compare = DLRTEST_COMPARE_ARGB; DLRTest_UpdateWindowTitles(); break;
                case SDLK_a: compare = DLRTEST_COMPARE_A;    DLRTest_UpdateWindowTitles(); break;
                case SDLK_r: compare = DLRTEST_COMPARE_R;    DLRTest_UpdateWindowTitles(); break;
                case SDLK_g: compare = DLRTEST_COMPARE_G;    DLRTest_UpdateWindowTitles(); break;
                case SDLK_b: compare = DLRTEST_COMPARE_B;    DLRTest_UpdateWindowTitles(); break;
                case SDLK_x: compare = DLRTEST_COMPARE_RGB;  DLRTest_UpdateWindowTitles(); break;
            }
        } break;
        case SDL_WINDOWEVENT: {
            switch (e.window.event) {
                case SDL_WINDOWEVENT_CLOSE: {
                    exit(0);
                } break;
            }
        } break;
    }
}

int main(int argc, char *argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("Can't init SDL: %s", SDL_GetError());
        return -1;
    }

    for (int i = 0; i < num_envs; ++i) {
        int window_flags = 0;
        if (envs[i].type == DLRTEST_TYPE_OPENGL1) {
            window_flags |= SDL_WINDOW_OPENGL;
        }

        envs[i].window = SDL_CreateWindow(
            envs[i].window_title,
            envs[i].window_x,
            envs[i].window_y,
            winW, winH,
            window_flags
        );
        if ( ! envs[i].window) {
            SDL_Log("SDL_CreateWindow failed, err=%s", SDL_GetError());
            return -1;
        }
        switch (envs[i].type) {
            case DLRTEST_TYPE_SOFTWARE: {
                SDL_Renderer * r = SDL_CreateRenderer(envs[i].window, 0, -1);
                if ( ! r) {
                    SDL_Log("SDL_CreateRenderer failed, err=%s", SDL_GetError());
                    return -1;
                }
                envs[i].inner.sw.bgTex = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, winW, winH);
                if ( ! envs[i].inner.sw.bgTex) {
                    SDL_Log("Can't create background texture: %s", SDL_GetError());
                    return -1;
                }
                envs[i].inner.sw.bg = SDL_CreateRGBSurface(0, winW, winH, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
                if ( ! envs[i].inner.sw.bg) {
                    SDL_Log("Can't create background surface: %s", SDL_GetError());
                    return -1;
                }
                SDL_FillRect(envs[i].inner.sw.bg, NULL, 0xFF000000);
            } break;

            case DLRTEST_TYPE_OPENGL1: {
                envs[i].inner.gl.gl = SDL_GL_CreateContext(envs[i].window);
                if ( ! envs[i].inner.gl.gl) {
                    SDL_Log("SDL_GL_CreateContext failed, err=%s", SDL_GetError());
                    return -1;
                }

                if ( ! _glBlendFuncSeparate) {
                    _glBlendFuncSeparate = (PFNGLBLENDFUNCSEPARATEPROC) SDL_GL_GetProcAddress("glBlendFuncSeparate");
                    if ( ! _glBlendFuncSeparate) {
                        SDL_Log("SDL_GL_GetProcAddress(\"glBlendFuncSeparate\") failed, err=%s", SDL_GetError());
                        return -1;
                    }
                }
            } break;

            default:
                SDL_Log("unknown renderer type");
                return -1;
        }
    } // end of init loop
    DLRTest_UpdateWindowTitles();

    while (1) {
        // Process events
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            DLR_Test_ProcessEvent(e);
        }

        // Draw
        for (int i = 0; i < num_envs; ++i) {
            DLRTest_Env * env = &envs[i];

            // Init Scene
            switch (env->type) {
                case DLRTEST_TYPE_SOFTWARE: {
                } break;
                case DLRTEST_TYPE_OPENGL1: {
                    SDL_GL_MakeCurrent(envs[i].window, env->inner.gl.gl);
                    glClearColor(0, 0, 0, 1);
                    glClear(GL_COLOR_BUFFER_BIT);
                    gbMat4 rotateX;
                    gb_mat4_rotate(&rotateX, {1, 0, 0}, (float)M_PI);
                    gbMat4 translateXY;
                    gb_mat4_translate(&translateXY, {-1.f, -1.f, 0});
                    gbMat4 scaleXY;
                    gb_mat4_scale(&scaleXY, {2.f/(float)winW, 2.f/(float)winH, 1});
                    gbMat4 finalM;
                    gb_mat4_mul(&finalM, &rotateX, &translateXY);
                    gb_mat4_mul(&finalM, &finalM, &scaleXY);
                    glLoadMatrixf(finalM.e);
                } break;
            }

            // Render Scene
            if ( ! (env->flags & DLRTEST_ENV_COMPARE)) {
                DLRTest_DrawScene(&envs[i]);
            } else {
                // Draw diff between scenes 0 and 1
                SDL_Surface * dest = DLRTest_GetSurfaceForView(&envs[i]);
                if (dest) {
                    SDL_Surface * A = DLRTest_GetSurfaceForView(&envs[0]);
                    SDL_Surface * B = DLRTest_GetSurfaceForView(&envs[1]);
                    for (int y = 0; y < dest->h; ++y) {
                        for (int x = 0; x < dest->w; ++x) {
                            Uint32 ac = DLR_GetPixel32(A->pixels, A->pitch, 4, x, y);
                            int aa, ar, ag, ab;
                            DLR_SplitARGB32(ac, aa, ar, ag, ab);

                            Uint32 bc = DLR_GetPixel32(B->pixels, B->pitch, 4, x, y);
                            int ba, br, bg, bb;
                            DLR_SplitARGB32(bc, ba, br, bg, bb);

                            int beyond = 0;
                            if (compare & DLRTEST_COMPARE_A) {
                                beyond |= abs(aa - ba) > compare_threshold;
                            }
                            if (compare & DLRTEST_COMPARE_R) {
                                beyond |= abs(ar - br) > compare_threshold;
                            }
                            if (compare & DLRTEST_COMPARE_G) {
                                beyond |= abs(ag - bg) > compare_threshold;
                            }
                            if (compare & DLRTEST_COMPARE_B) {
                                beyond |= abs(ab - bb) > compare_threshold;
                            }

                            Uint32 dc = beyond ? 0xffffffff : 0x00000000;
                            DLR_SetPixel32(dest->pixels, dest->pitch, 4, x, y, dc);
                        }
                    }
                }
            }

            // Present rendered scene
            switch (env->type) {
                case DLRTEST_TYPE_SOFTWARE: {
                    SDL_Renderer * r = SDL_GetRenderer(envs[i].window);
                    SDL_UpdateTexture(env->inner.sw.bgTex, NULL, env->inner.sw.bg->pixels, env->inner.sw.bg->pitch);
                    SDL_SetRenderDrawColor(r, 0, 0, 0, 0xff);
                    SDL_RenderClear(r);
                    SDL_RenderCopy(r, envs[i].inner.sw.bgTex, NULL, NULL);

                    // Draw locked position
                    if (DLRTEST_IS_LOCKED()) {
                        const int w = 15;
                        const int h = 15;
                        SDL_Rect box;
                        if ( ! envs[i].inner.sw.crosshairs) {
                            SDL_Surface * src = SDL_CreateRGBSurface(0, w, h, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
                            if (src) {
                                // Clear entire surface:
                                SDL_FillRect(src, NULL, 0x00000000);

                                // Draw vertical bar:
                                box = {w/2, 0, 1, h};
                                SDL_FillRect(src, &box, 0xffff0000);

                                // Draw horizontal bar:
                                box = {0, h/2, w, 1};
                                SDL_FillRect(src, &box, 0xffff0000);

                                // Draw outside of inner box:
                                box = {(w/2)-3, (h/2)-3, 7, 7};
                                SDL_FillRect(src, &box, 0xffff0000);

                                // Clear inside of inner box:
                                box = {(w/2)-1, (h/2)-1, 3, 3};
                                SDL_FillRect(src, &box, 0x00000000);

                                // Convert to texture:
                                envs[i].inner.sw.crosshairs = SDL_CreateTextureFromSurface(r, src);
                                SDL_FreeSurface(src);
                            }
                        }
                        if (envs[i].inner.sw.crosshairs) {
                            box = {lockedX-(w/2), lockedY-(h/2), w, h};
                            SDL_RenderCopy(r, envs[i].inner.sw.crosshairs, NULL, &box);
                        }
                    }

                    SDL_RenderPresent(r);
                } break;
                case DLRTEST_TYPE_OPENGL1: {
                    SDL_GL_SwapWindow(envs[i].window);
                } break;
            }
        }
    }


    return 0;
}

//
// This is free and unencumbered software released into the public domain.
// 
// Anyone is free to copy, modify, publish, use, compile, sell, or
// distribute this software, either in source code form or as a compiled
// binary, for any purpose, commercial or non-commercial, and by any
// means.
// 
// In jurisdictions that recognize copyright laws, the author or authors
// of this software dedicate any and all copyright interest in the
// software to the public domain. We make this dedication for the benefit
// of the public at large and to the detriment of our heirs and
// successors. We intend this dedication to be an overt act of
// relinquishment in perpetuity of all present and future rights to this
// software under copyright law.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
// 
// For more information, please refer to <http://unlicense.org/>
//
