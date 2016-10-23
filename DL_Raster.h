// DL_Raster.h - SUPER-EARLY WORK-IN-PROGRESS on a new, 2d rasterization library; BUGGY!
// Public domain, Sep 25 2016, David Ludwig, dludwig@pobox.com. See "unlicense" statement at the end of this file.
//
// Usage:
//
// Include this file in whatever places need to refer to it. In ONE C/C++
// file, write:
//
//   #define DL_RASTER_IMPLEMENTATION
//
// ... before the #include of this file. This expands out the actual
// implementation into that C/C++ file.  #define'ing DL_RASTER_IMPLEMENTATION
// in more than one file, or in a header file that many C/C++ file(s) include,
// will likely lead to build errors from the linker.
//    
// All other files should just #include "DL_Raster.h" without the #define.
//

#ifndef DL_RASTER_H
#define DL_RASTER_H

typedef enum {
    DLR_BLENDMODE_NONE      = 0,
    DLR_BLENDMODE_BLEND     = 1,    /* alpha blending
                                     * dC = (sC * sA) + (dC * (1 - sA))
                                     * dA =       sA  + (dA * (1 - sA))
                                     */

} DLR_BlendMode;

typedef enum {
    DLR_TEXTUREMODULATE_NONE    = (1 << 0),
    DLR_TEXTUREMODULATE_COLOR   = (1 << 1),
    //DLR_TEXTUREMODULATE_ALPHA   = (1 << 2)
} DLR_TextureModulate;

typedef double DLR_Float;       // huh, double can be faster than float, at least on a high-end-ish x64 CPU

typedef struct DLR_Point {
    DLR_Float x;
    DLR_Float y;
} DLR_Point;

typedef struct DLR_Vertex {
    DLR_Float x;
    DLR_Float y;
    DLR_Float a;
    DLR_Float r;
    DLR_Float g;
    DLR_Float b;
    DLR_Float uv;
    DLR_Float uw;
} DLR_Vertex;

typedef struct DLR_SurfaceRef {
    void * pixels;
    int w;
    int h;
    int pitch;
} DLR_SurfaceRef;

typedef struct DLR_State {
    DLR_SurfaceRef dest;        // a NON-owning pointer!
    DLR_SurfaceRef texture;     // a NON-owning pointer!
    DLR_BlendMode blendMode;
    DLR_TextureModulate textureModulate;
} DLR_State;

#define DLR_GetPixel32(PIXELS, PITCH, BYTESPP, X, Y) \
    (*(Uint32 *)((Uint8 *)(PIXELS) + (((Y) * (PITCH)) + ((X) * (BYTESPP)))))

#define DLR_SetPixel32(PIXELS, PITCH, BYTESPP, X, Y, COLOR) \
    *(Uint32 *)((Uint8 *)(PIXELS) + (((Y) * (PITCH)) + ((X) * (BYTESPP)))) = (COLOR)

#define DLR_SplitARGB32(SRC, A, R, G, B) \
    (A) = (((SRC) >> 24) & 0xff), \
    (R) = (((SRC) >> 16) & 0xff), \
    (G) = (((SRC) >>  8) & 0xff), \
    (B) = (((SRC)      ) & 0xff)

#define DLR_JoinARGB32(A, R, G, B) \
    ((((A) & 0xff) << 24) | \
     (((R) & 0xff) << 16) | \
     (((G) & 0xff) <<  8) | \
     (((B) & 0xff)      ))

#define DLR_Min(X, Y) (((X) < (Y)) ? (X) : (Y))
#define DLR_Max(X, Y) (((X) > (Y)) ? (X) : (Y))

#if !defined(NDEBUG) && (defined(DEBUG) || defined(_DEBUG))
    #include <assert.h>
    #define DLR_Assert(X) assert(X)
#else
    #define DLR_Assert(X)
#endif

#ifdef __cplusplus
    #define DLR_EXTERN_C extern "C"
#else
    #define DLR_EXTERN_C
#endif

DLR_EXTERN_C void DLR_Clear(
    DLR_State * state,
    Uint32 color);

DLR_EXTERN_C void DLR_DrawTriangle(
    DLR_State * state,
    DLR_Vertex v0,
    DLR_Vertex v1,
    DLR_Vertex v2);

DLR_EXTERN_C void DLR_DrawTriangles(
    DLR_State * state,
    DLR_Vertex * vertices,
    size_t vertexCount);

#endif // ifndef DL_RASTER_H

#ifdef DL_RASTER_IMPLEMENTATION

DLR_EXTERN_C void DLR_Clear(
    DLR_State * state,
    Uint32 color)
{
    for (int y = 0; y < state->dest.h; ++y) {
        for (int x = 0; x <= (state->dest.pitch - 4); x += 4) {
            *((uint32_t *)(((uint8_t *)state->dest.pixels) + (y * state->dest.pitch) + x)) = color;
        }
    }
}

template <typename DLR_ColorComponent>
struct DLR_Color {
    DLR_ColorComponent A;
    DLR_ColorComponent R;
    DLR_ColorComponent G;
    DLR_ColorComponent B;

    DLR_Color() = default;

    DLR_Color(DLR_ColorComponent a_, DLR_ColorComponent r_, DLR_ColorComponent g_, DLR_ColorComponent b_) {
        A = a_;
        R = r_;
        G = g_;
        B = b_;
    }

    DLR_Color(Uint32 argb) {
        A = (argb >> 24) & 0xff;
        R = (argb >> 16) & 0xff;
        G = (argb >>  8) & 0xff;
        B = (argb      ) & 0xff;
    }

    template <typename DLR_DestColorComponent>
    explicit DLR_Color(DLR_Color<DLR_DestColorComponent> other) {
        A = (DLR_ColorComponent) other.A;
        R = (DLR_ColorComponent) other.R;
        G = (DLR_ColorComponent) other.G;
        B = (DLR_ColorComponent) other.B;
    }

    DLR_Color operator + (DLR_Color other) const { return { A + other.A, R + other.R, G + other.G, B + other.B }; }
    DLR_Color operator * (DLR_Color other) const { return { A * other.A, R * other.R, G * other.G, B * other.B }; }

    template <typename DLR_Coefficient>
    DLR_Color operator * (DLR_Coefficient coeff) const { return {A * coeff, R * coeff, G * coeff, B * coeff}; }

    template <typename DLR_Coefficient>
    DLR_Color operator / (DLR_Coefficient coeff) const { return {A / coeff, R / coeff, G / coeff, B / coeff}; }
};

static Uint32 DLR_Join(DLR_Color<Uint8> c) {
    return DLR_JoinARGB32(c.A, c.R, c.G, c.B);
}

static DLR_Color<Uint8> DLR_Round(DLR_Color<DLR_Float> c) {
    return {
        (Uint8)(c.A + 0.5),
        (Uint8)(c.R + 0.5),
        (Uint8)(c.G + 0.5),
        (Uint8)(c.B + 0.5),
    };
}

#define DLR_AssertValidColor8888(C) \
    DLR_Assert(C.A >= 0 && C.A <= 255); \
    DLR_Assert(C.R >= 0 && C.R <= 255); \
    DLR_Assert(C.G >= 0 && C.G <= 255); \
    DLR_Assert(C.B >= 0 && C.B <= 255);

DLR_Color<DLR_Float> & DLR_VertexColor(DLR_Vertex & v) {
    DLR_Float * cptr1 = &(v.a);
    DLR_Color<DLR_Float> * cptr2 = (DLR_Color<DLR_Float> *) cptr1;
    return * cptr2;
}

//DLR_Float DLR_CalculateOrientation(DLR_Vertex a, DLR_Vertex b, DLR_Vertex c) {
//    //return (b.x-a.x) * (c.y-a.y) - (b.y-a.y) * (c.x-a.x));
//    return (b.x-a.x)*(c.y-a.y) - (b.y-a.y)*(c.x-a.x);
//}

static void DLR_CalculateBarycentricCoordinates(
    DLR_Vertex p,
    DLR_Vertex a,
    DLR_Vertex b,
    DLR_Vertex c,
    DLR_Float *lambdaA,
    DLR_Float *lambdaB,
    DLR_Float *lambdaC)
{
    *lambdaA =
        ((b.y - c.y) * (p.x - c.x) + (c.x - b.x) * (p.y - c.y)) /
        ((b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y));

    *lambdaB =
        ((c.y - a.y) * (p.x - c.x) + (a.x - c.x) * (p.y - c.y)) /
        ((b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y));

    *lambdaC = 1. - *lambdaA - *lambdaB;
}

static inline bool DLR_WithinEdgeAreaClockwise(DLR_Float barycentric, DLR_Float edgeX, DLR_Float edgeY)
{
    if (barycentric < 0) {
        return false;
    } else if (barycentric == 0) {
        if (edgeY == 0 && edgeX > 0) {
            return true;
        } else if (edgeY < 0) {
            return true;
        } else {
            return false;
        }
    } else {
        return true;
    }
}

DLR_EXTERN_C void DLR_DrawTriangle(DLR_State * state, DLR_Vertex v0, DLR_Vertex v1, DLR_Vertex v2)
{
    // TODO: consider adding +1 to *max vars, to prevent clipping.  This'll depend on how we determine if a pixel is lit.
    int ymin = (int) DLR_Min(v0.y, DLR_Min(v1.y, v2.y));
    int ymax = (int) DLR_Max(v0.y, DLR_Max(v1.y, v2.y));
    int xmin = (int) DLR_Min(v0.x, DLR_Min(v1.x, v2.x));
    int xmax = (int) DLR_Max(v0.x, DLR_Max(v1.x, v2.x));

    //xmax++;
    //ymax++;

    ymin = DLR_Max(ymin, 0);
    ymax = DLR_Min(ymax, (state->dest.h - 1));
    xmin = DLR_Max(xmin, 0);
    xmax = DLR_Min(xmax, (state->dest.w - 1));

    DLR_Float lambda0;
    DLR_Float lambda1;
    DLR_Float lambda2;

    for (int y = ymin; y <= ymax; ++y) {
        for (int x = 0; x <= xmax; ++x) {
            DLR_Vertex p = {(DLR_Float)x, (DLR_Float)y};
            p.x += 0.5; // Use the center of the pixel, to determine whether to rasterize
            p.y += 0.5;
            DLR_CalculateBarycentricCoordinates(p, v0, v1, v2, &lambda0, &lambda1, &lambda2);

            DLR_Vertex edge0 = {v2.x-v1.x, v2.y-v1.y}; // v2 - v1
            DLR_Vertex edge1 = {v0.x-v2.x, v0.y-v2.y}; // v0 - v2
            DLR_Vertex edge2 = {v1.x-v0.x, v1.y-v0.y}; // v1 - v0
            bool overlaps = true;
            overlaps &= DLR_WithinEdgeAreaClockwise(lambda0, edge0.x, edge0.y);
            overlaps &= DLR_WithinEdgeAreaClockwise(lambda1, edge1.x, edge1.y);
            overlaps &= DLR_WithinEdgeAreaClockwise(lambda2, edge2.x, edge2.y);
            if (overlaps) {
                DLR_Color<DLR_Float> fincomingC = \
                    (DLR_VertexColor(v0) * lambda0) +
                    (DLR_VertexColor(v1) * lambda1) +
                    (DLR_VertexColor(v2) * lambda2);
                DLR_Color<Uint8> nincomingC = (DLR_Color<Uint8>) DLR_Round(fincomingC * 255.f);
                DLR_AssertValidColor8888(nincomingC);

                DLR_Color<Uint8> nfinalC = {0xff, 0xff, 0x00, 0xff};  // default to ugly color
                DLR_Float uv = 0.;
                DLR_Float uw = 0.;
                DLR_Float ftexX = 0.;
                DLR_Float ftexY = 0.;
                int ntexX = 0;
                int ntexY = 0;
                DLR_Color<Uint8> ntexC = {0, 0, 0, 0};
                DLR_Color<DLR_Float> ftexC = {0., 0., 0., 0.};
                DLR_Color<DLR_Float> ffinalC = {0., 0., 0., 0.};
                
                if (state->texture.pixels) {
                    uv = (lambda0 * v0.uv) + (lambda1 * v1.uv) + (lambda2 * v2.uv);
                    uw = (lambda0 * v0.uw) + (lambda1 * v1.uw) + (lambda2 * v2.uw);
                    ftexX = (uv * (DLR_Float)(state->texture.w /* - 1*/));
                    ftexY = (uw * (DLR_Float)(state->texture.h /* - 1*/));
                    ntexX = DLR_Min((int)/*round*/(ftexX), state->texture.w - 1);
                    ntexY = DLR_Min((int)/*round*/(ftexY), state->texture.h - 1);
                    DLR_Assert(ntexX >= 0 && ntexX < state->texture.w);
                    DLR_Assert(ntexY >= 0 && ntexY < state->texture.h);
                    ntexC = DLR_GetPixel32(state->texture.pixels, state->texture.pitch, 4, ntexX, ntexY);
                    ftexC = (DLR_Color<DLR_Float>)ntexC / 255.f;
                    if (state->textureModulate & DLR_TEXTUREMODULATE_COLOR) {
                        ffinalC = ftexC * fincomingC;
                        nfinalC = (DLR_Color<Uint8>) DLR_Round(ffinalC * 255.f);
                        DLR_AssertValidColor8888(nfinalC);
                    } else {
                        nfinalC = ntexC;
                        ffinalC = ftexC;
                    }
                } else {
                    // texture is NULL
                    DLR_Assert(state->texture.pixels == NULL);
                    nfinalC = nincomingC;
                    ffinalC = fincomingC;
                } // if tex ... ; else ...

                // color is ARGB
                switch (state->blendMode) {
                    case DLR_BLENDMODE_NONE: {
                        DLR_SetPixel32(state->dest.pixels, state->dest.pitch, 4, x, y, DLR_Join(nfinalC));
                    } break;

                    case DLR_BLENDMODE_BLEND: {
                        DLR_Color<DLR_Float> fsrc = ffinalC;
                        DLR_Color<Uint8> ndest = (DLR_Color<Uint8>) DLR_GetPixel32(state->dest.pixels, state->dest.pitch, 4, x, y);
                        DLR_Color<DLR_Float> fdest = (DLR_Color<DLR_Float>)ndest / 255.f;

                        // dstRGB = (srcRGB * srcA) + (dstRGB * (1-srcA))
                        fdest.R = (fsrc.R * fsrc.A) + (fdest.R * (1. - fsrc.A));
                        fdest.G = (fsrc.G * fsrc.A) + (fdest.G * (1. - fsrc.A));
                        fdest.B = (fsrc.B * fsrc.A) + (fdest.B * (1. - fsrc.A));

                        // dstA = srcA + (dstA * (1-srcA))
                        fdest.A = fsrc.A + (fdest.A * (1. - fsrc.A));

                        ndest = (DLR_Color<Uint8>) DLR_Round(fdest * 255.f);
                        DLR_AssertValidColor8888(ndest);
                        DLR_SetPixel32(state->dest.pixels, state->dest.pitch, 4, x, y, DLR_Join(ndest));
                    } break;
                }
            } // if (overlaps)
        }
    }
}

DLR_EXTERN_C void DLR_DrawTriangles(DLR_State * state, DLR_Vertex * vertices, size_t vertexCount)
{
    for (size_t i = 0; (i + 2) < vertexCount; i += 3) {
        DLR_DrawTriangle(state, vertices[i], vertices[i+1], vertices[i+2]);
    }
}

#endif // ifdef DL_RASTER_IMPLEMENTATION

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

