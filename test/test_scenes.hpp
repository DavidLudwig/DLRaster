// test_scenes.hpp - DL_Raster test-program scenes
// Public domain, Sep 25 2016, David Ludwig, dludwig@pobox.com. See "unlicense" statement at the end of this file.

enum DLRTest_TextureType {
    DLRTEST_TEXTURE_FROM_IMAGE          = 0,
    DLRTEST_TEXTURE_NONE                = 1,
    DLRTEST_TEXTURE_ALL_WHITE           = 2,
    DLRTEST_TEXTURE_VERTICAL_GRADIENT   = 3,
    DLRTEST_TEXTURE_HORIZONTAL_GRADIENT = 4,
};

SDL_Surface * DLRTest_CreateTexture256x256(const DLRTest_TextureType textureType) {
    SDL_Surface * textureSrc = NULL;
    switch (textureType) {
        case DLRTEST_TEXTURE_FROM_IMAGE: {
            textureSrc = SDL_LoadBMP_RW(SDL_RWFromMem(texture_bmp, texture_bmp_len), 1);
        } break;

        case DLRTEST_TEXTURE_NONE: {
            textureSrc = NULL;
        } break;

        case DLRTEST_TEXTURE_ALL_WHITE: {
            textureSrc = SDL_CreateRGBSurface(0, 256, 256, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
            SDL_FillRect(textureSrc, NULL, 0xFFFFFFFF);
        } break;

        case DLRTEST_TEXTURE_VERTICAL_GRADIENT: {
            textureSrc = SDL_CreateRGBSurface(0, 256, 256, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
            for (int y = 0; y < 256; ++y) {
                for (int x = 0; x < 256; ++x) {
                    Uint32 c = DLR_JoinARGB32(0xff, y, y, y);
                    //Uint32 c = (y == 100) ? 0xffffffff : 0x88888888;
                    DLR_WritePixel32_Raw(textureSrc->pixels, textureSrc->pitch, 4, x, y, c);
                }
            }
        } break;

        case DLRTEST_TEXTURE_HORIZONTAL_GRADIENT: {
            textureSrc = SDL_CreateRGBSurface(0, 256, 256, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
            for (int y = 0; y < 256; ++y) {
                for (int x = 0; x < 256; ++x) {
                    Uint32 c = DLR_JoinARGB32(0xff, x, x, x);
                    DLR_WritePixel32_Raw(textureSrc->pixels, textureSrc->pitch, 4, x, y, c);
                }
            }
        } break;
    }

    if (textureSrc) {
        DLRTest_Assert(textureSrc->w == 256);
        DLRTest_Assert(textureSrc->h == 256);
    }
    return textureSrc;
}

namespace DLRTest_Shape_Multi_Color_Triangle
{
    const DLR_Float originX = 20;
    const DLR_Float originY = 20;
    const struct { float a, r, g, b; } c[] = {
        {1, 1, 0, 0},
        {1, 0, 1, 0},
        {1, 0, 0, 1},
    };
    const DLR_VertexD vertices[] = {
        {originX + 150, originY + 300,    c[2].b, c[2].g, c[2].r, c[2].a,   0, 0},
        {originX + 300, originY + 150,    c[1].b, c[1].g, c[1].r, c[1].a,   0, 0},
        {originX      , originY      ,    c[0].b, c[0].g, c[0].r, c[0].a,   0, 0},
    };
}

namespace DLRTest_Shape_Textured_Square
{
    const DLR_Float originX = 200.51;
    const DLR_Float originY = 20.;
    const struct { DLR_Float b, g, r, a; } c[] = {
        { 0.3, 0.3,   1,   1},  // left  top
        { 0.3,   1, 0.3,   1},  // right top
        {   1, 0.3, 0.3,   1},  // right bottom
        {   1,   1,   1, 0.5},  // left  bottom
    };

    const int rectW = 256;
    const int rectH = 256;

    const DLR_VertexD vertices[] = {
        // TRIANGLE: top-right
        {originX + rectW, originY + rectH,    c[2].b, c[2].g, c[2].r, c[2].a,    1, 1},    // right bottom
        {originX + rectW, originY,            c[1].b, c[1].g, c[1].r, c[1].a,    1, 0},    // right top
        {originX        , originY,            c[0].b, c[0].g, c[0].r, c[0].a,    0, 0},    // left  top

        // TRIANGLE: bottom-left
        {originX        , originY        ,    c[0].b, c[0].g, c[0].r, c[0].a,    0, 0},    // left  top
        {originX        , originY + rectH,    c[3].b, c[3].g, c[3].r, c[3].a,    0, 1},    // left  bottom
        {originX + rectW, originY + rectH,    c[2].b, c[2].g, c[2].r, c[2].a,    1, 1},    // right bottom
    };
}

void DLRTest_Scene_Mix1(DLRTest_Scene * scene, DLRTest_Env * env)
{
    DLRTest_Clear(env, 0xff000000);

    {
        using namespace DLRTest_Shape_Multi_Color_Triangle;
        static DLR_State state;
        DLRTest_InitState(env, &state);
        DLRTest_DrawTriangles(env, &state, vertices, SDL_arraysize(vertices));
    }

    {
        using namespace DLRTest_Shape_Textured_Square;
        static DLR_State state;
        if (DLRTest_InitState(env, &state)) {
            SDL_Surface * textureSrc = DLRTest_CreateTexture256x256(DLRTEST_TEXTURE_FROM_IMAGE);
            state.texture = DLRTest_SDLToDLRSurfaceNoCopy(textureSrc);
            state.textureModulate = DLR_TEXTUREMODULATE_COLOR;
            state.blendMode = DLR_BLENDMODE_BLEND;
        }
        DLRTest_DrawTriangles(env, &state, vertices, SDL_arraysize(vertices));
    }
}

void DLRTest_Scene_Mix1Plain(DLRTest_Scene * scene, DLRTest_Env * env)
{
    DLRTest_Clear(env, 0xff000000);

    {
        using namespace DLRTest_Shape_Multi_Color_Triangle;
        static DLR_State state;
        DLRTest_InitState(env, &state);
        DLRTest_DrawTriangles(env, &state, vertices, SDL_arraysize(vertices));
    }

    {
        using namespace DLRTest_Shape_Textured_Square;
        static DLR_State state;
        DLRTest_InitState(env, &state);
        DLRTest_DrawTriangles(env, &state, vertices, SDL_arraysize(vertices));
    }
}

void DLRTest_Scene_LeftEdge(DLRTest_Scene * scene, DLRTest_Env * env)
{
    DLRTest_Clear(env, 0xff000000);

    {
        static DLR_State state;
        if (DLRTest_InitState(env, &state)) {
            state.fixedColorARGB = 0xff00ff00;
            state.srcColorMode = DLR_SRCCOLORMODE_FIXED;
        }

        const DLR_Float originX = 0;
        const DLR_Float originY = 0;
        const DLR_Float rectW = scene->w;
        const DLR_Float rectH = scene->h;

        const DLR_VertexD vertices[] = {
            // TRIANGLE: top-right
            {originX        , originY,            1, 1, 1, 1,    0, 0},    // left  top
            {originX + rectW, originY + rectH,    1, 1, 1, 1,    0, 0},    // right bottom
            {originX + rectW, originY,            1, 1, 1, 1,    0, 0},    // right top
        };

        DLRTest_DrawTriangles(env, &state, vertices, SDL_arraysize(vertices));
    }
}

void DLRTest_Scene_BigRect(DLRTest_Scene * scene, DLRTest_Env * env)
{
    DLRTest_Clear(env, 0xff000000);

    {
        static DLR_State state;
        if (DLRTest_InitState(env, &state)) {
            state.fixedColorARGB = 0xff00ff00;
            state.srcColorMode = DLR_SRCCOLORMODE_FIXED;
        }

        const DLR_Float originX = 0;
        const DLR_Float originY = 0;
        const DLR_Float rectW = scene->w;
        const DLR_Float rectH = scene->h;

        const DLR_VertexD vertices[] = {
            // TRIANGLE: top-right
            {originX        , originY,            1, 1, 1, 1,    0, 0},    // left  top
            {originX + rectW, originY + rectH,    1, 1, 1, 1,    0, 0},    // right bottom
            {originX + rectW, originY,            1, 1, 1, 1,    0, 0},    // right top

            // TRIANGLE: bottom-left
            {originX        , originY        ,    1, 1, 1, 1,    0, 0},    // left  top
            {originX        , originY + rectH,    1, 1, 1, 1,    0, 0},    // left  bottom
            {originX + rectW, originY + rectH,    1, 1, 1, 1,    0, 0},    // right bottom
        };

        DLRTest_DrawTriangles(env, &state, vertices, SDL_arraysize(vertices));
    }
}

static DLRTest_Scene allScenes[] = {
    { "Mix1",       &DLRTest_Scene_Mix1,        460,    400 },
    { "Mix1Plain",  &DLRTest_Scene_Mix1Plain,   460,    400 },
    { "LeftEdge",   &DLRTest_Scene_LeftEdge,    400,    400 },
    { "BigRect",    &DLRTest_Scene_BigRect,     768,    768 },
};

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
