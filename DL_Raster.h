// DL_Raster.h - SUPER-EARLY WORK-IN-PROGRESS on a new, 2d rasterization library; BUGGY!
// Public domain, Sep 25 2016, David Ludwig, dludwig@pobox.com. See "unlicense" statement at the end of this file.
//
//
// Setup:
//
// Include this file in whatever places need to refer to it. In ONE C/C++
// file, write:
//
//   #define DL_RASTER_IMPLEMENTATION
//
// ... before the #include of this file. This expands out the actual
// implementation into that C/C++ file.  #define'ing DL_RASTER_IMPLEMENTATION
// in more than one file, or in a header file that many C/C++ file(s) include,
// will likely lead to build errors (from the linker).
//    
// All other files should just #include "DL_Raster.h" without the #define.
//

#ifndef DL_RASTER_H
#define DL_RASTER_H

//#define DLR_USE_DOUBLE 1

#define DLR_FIXED_PRECISION 14

#define DLR_abs(A) (((A) > 0) ? (A) : -(A))

template <typename DLR_Integer, int DLR_Precision, typename DLR_IntegerBig>
struct DLR_FixedT {
    DLR_Integer data;

#ifdef __cplusplus
    DLR_FixedT() {}
    explicit DLR_FixedT(float src) { data = (DLR_Integer)(src * (1 << DLR_Precision)); }
    explicit DLR_FixedT(double src) { data = (DLR_Integer)(src * (1 << DLR_Precision)); }
    explicit DLR_FixedT(uint8_t src) { data = ((DLR_Integer)src) << DLR_Precision; }
    explicit DLR_FixedT(int src) { data = ((DLR_Integer)src) << DLR_Precision; }

    explicit operator int() const { return (int)(data >> DLR_Precision); }
    explicit operator uint8_t() const { return (uint8_t)(data >> DLR_Precision); }
    explicit operator float() const { return ((float)(data)) / float(1 << DLR_Precision); }

    static DLR_FixedT FromRaw(DLR_Integer raw) { DLR_FixedT c; c.data = raw; return c; }

    template <typename DLR_IntegerSrc, int DLR_PrecisionSrc, typename DLR_IntegerBigSrc>
    explicit DLR_FixedT(DLR_FixedT<DLR_IntegerSrc, DLR_PrecisionSrc, DLR_IntegerBigSrc> src) {
        if (DLR_PrecisionSrc < DLR_Precision) {
            data = ((DLR_Integer)src.data) << DLR_abs(DLR_PrecisionSrc - DLR_Precision);
        } else if (DLR_PrecisionSrc > DLR_Precision) {
            data = (DLR_Integer)(src.data >> DLR_abs(DLR_Precision - DLR_PrecisionSrc));
        } else {
            data = (DLR_Integer)src.data;
        }
        // bit-shifts are getting wrapped in DLR_abs() in order to prevent compiler warnings regarding negative shifts
    }

    inline DLR_FixedT operator+(DLR_FixedT b) const { return FromRaw(data + b.data); }
    inline DLR_FixedT operator-(DLR_FixedT b) const { return FromRaw(data - b.data); }
    inline DLR_FixedT operator/(DLR_FixedT b) const { return FromRaw(((DLR_IntegerBig)data << DLR_Precision) / (DLR_IntegerBig)b.data); }
    inline DLR_FixedT operator*(DLR_FixedT b) const { return FromRaw(((DLR_IntegerBig)data * (DLR_IntegerBig)b.data) >> DLR_Precision); }
    inline DLR_FixedT operator>>(unsigned int shift) const { return FromRaw(data >> shift); }
    inline DLR_FixedT operator<<(unsigned int shift) const { return FromRaw(data << shift); }
    inline bool operator==(DLR_FixedT b) const { return data == b.data; }
    inline bool operator!=(DLR_FixedT b) const { return data != b.data; }
    inline bool operator<(DLR_FixedT b) const { return data < b.data; }
    inline bool operator>(DLR_FixedT b) const { return data > b.data; }
    inline bool operator<=(DLR_FixedT b) const { return data <= b.data; }
    inline bool operator>=(DLR_FixedT b) const { return data >= b.data; }
#endif
};

typedef DLR_FixedT<int32_t, DLR_FIXED_PRECISION, int64_t> DLR_Fixed;
typedef DLR_FixedT<int64_t, DLR_FIXED_PRECISION * 2, int64_t> DLR_FixedBig;


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

typedef struct DLR_PointX {
    DLR_Fixed x;
    DLR_Fixed y;
} DLR_PointX;

typedef struct DLR_VertexX {
    DLR_Fixed x;
    DLR_Fixed y;
    DLR_Fixed a;
    DLR_Fixed r;
    DLR_Fixed g;
    DLR_Fixed b;
    DLR_Fixed uv;
    DLR_Fixed uw;
} DLR_VertexX;

typedef struct DLR_VertexD {
    double x;
    double y;
    double a;
    double r;
    double g;
    double b;
    double uv;
    double uw;
} DLR_VertexD;


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
    (*(Uint32 *)((Uint8 *)(PIXELS) + (((Y) * (PITCH)) + ((X) * (BYTESPP))))) = (COLOR)

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

DLR_EXTERN_C void DLR_DrawTriangleX(
    DLR_State * state,
    DLR_VertexX v0,
    DLR_VertexX v1,
    DLR_VertexX v2);

DLR_EXTERN_C void DLR_DrawTrianglesX(
    DLR_State * state,
    DLR_VertexX * vertices,
    size_t vertexCount);

DLR_EXTERN_C void DLR_DrawTrianglesD(
    DLR_State * state,
    DLR_VertexD * vertices,
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

    DLR_Color operator >> (unsigned int shift) const { return {A >> shift, R >> shift, G >> shift, B >> shift}; }
    DLR_Color operator << (unsigned int shift) const { return {A << shift, R << shift, G << shift, B << shift}; }
};

static Uint32 DLR_Join(DLR_Color<Uint8> c) {
    return DLR_JoinARGB32(c.A, c.R, c.G, c.B);
}

template <typename DLR_Number>
static DLR_Color<Uint8> DLR_Round(DLR_Color<DLR_Number> c) {
    return {
        (Uint8)(c.A + (DLR_Number)0.5f),
        (Uint8)(c.R + (DLR_Number)0.5f),
        (Uint8)(c.G + (DLR_Number)0.5f),
        (Uint8)(c.B + (DLR_Number)0.5f),
    };
}


#define DLR_AssertValidColor8888(C) \
    DLR_Assert(C.A >= 0 && C.A <= 255); \
    DLR_Assert(C.R >= 0 && C.R <= 255); \
    DLR_Assert(C.G >= 0 && C.G <= 255); \
    DLR_Assert(C.B >= 0 && C.B <= 255);

template <typename DLR_Number, typename DLR_Vertex>
DLR_Color<DLR_Number> & DLR_VertexColor(DLR_Vertex & v) {
    return * (DLR_Color<DLR_Number> *) &(v.a);
}

// template <typename DLR_Number, typename DLR_Vertex>
// DLR_Number DLR_CalculateOrientation(DLR_Vertex b, DLR_Vertex c, DLR_Vertex p) {
//     return (b.x - c.x) * (p.y - c.y) - (b.y - c.y) * (p.x - c.x);
// }

template <typename DLR_Number>
static inline bool DLR_WithinEdgeAreaClockwise(DLR_Number barycentric, DLR_Number edgeX, DLR_Number edgeY)
{
    if (barycentric < (DLR_Number)0) {
        return false;
    } else if (barycentric == (DLR_Number)0) {
        if (edgeY == (DLR_Number)0 && edgeX > (DLR_Number)0) {
            return true;
        } else if (edgeY < (DLR_Number)0) {
            return true;
        } else {
            return false;
        }
    } else {
        return true;
    }
}

#define DLR_MAX_COLOR_COMPONENT 256

template <typename DLR_Number>
static inline DLR_Color<Uint8> DLR_ConvertColorToBytes(DLR_Color<DLR_Number> in) {
    return DLR_Round(in * (DLR_Number)DLR_MAX_COLOR_COMPONENT);
//    return DLR_Round(in << 8);
}

template <typename DLR_Number>
static inline DLR_Color<DLR_Number> DLR_ConvertColorFromBytes(DLR_Color<Uint8> in) {
    return (DLR_Color<DLR_Number>)in / (DLR_Number)DLR_MAX_COLOR_COMPONENT;
//    return ((DLR_Color<DLR_Number>)(in)) >> 8;
}

template <typename DLR_Number, typename DLR_Vertex, typename DLR_NumberBig>
void DLR_DrawTriangleT(DLR_State * state, DLR_Vertex v0, DLR_Vertex v1, DLR_Vertex v2)
{
    // TODO: consider adding +1 to *max vars, to prevent clipping.  This'll depend on how we determine if a pixel is lit.
    int ymin = (int)(DLR_Min(v0.y, DLR_Min(v1.y, v2.y)));
    int ymax = (int)(DLR_Max(v0.y, DLR_Max(v1.y, v2.y)));
    int xmin = (int)(DLR_Min(v0.x, DLR_Min(v1.x, v2.x)));
    int xmax = (int)(DLR_Max(v0.x, DLR_Max(v1.x, v2.x)));

    //xmax++;
    //ymax++;

    ymin = DLR_Max(ymin, 0);
    ymax = DLR_Min(ymax, (state->dest.h - 1));
    xmin = DLR_Max(xmin, 0);
    xmax = DLR_Min(xmax, (state->dest.w - 1));

    DLR_Number lambda0;
    DLR_Number lambda1;
    DLR_Number lambda2;

    // Precompute a common part of the triangle's barycentric coordinates,
    // in a form that won't require division later-on.
    const DLR_NumberBig barycentric_conversion_factor = ( (DLR_NumberBig)1 / (DLR_NumberBig)((v1.y - v2.y) * (v0.x - v2.x) + (v2.x - v1.x) * (v0.y - v2.y)));

    const DLR_Number xy_offset = (DLR_Number)0.5f;
    for (int y = ymin; y <= ymax; ++y) {
        for (int x = 0; x <= xmax; ++x) {
            DLR_Vertex p = {(DLR_Number)x, (DLR_Number)y};
            p.x = p.x + xy_offset; // Use the center of the pixel, to determine whether to rasterize
            p.y = p.y + xy_offset;

            // Calculate barycentric coordinates.  Use 'big' numbers, with enough precision to help prevent underflow.
            lambda0 = (DLR_Number)(((DLR_NumberBig) (((v1.y - v2.y) * ( p.x - v2.x) + (v2.x - v1.x) * ( p.y - v2.y))) ) * barycentric_conversion_factor);
            lambda1 = (DLR_Number)(((DLR_NumberBig) (((v2.y - v0.y) * ( p.x - v2.x) + (v0.x - v2.x) * ( p.y - v2.y))) ) * barycentric_conversion_factor);
            //lambda2 = (DLR_Number)(((DLR_NumberBig) (((v0.y - v1.y) * ( p.x - v0.x) + (v1.x - v0.x) * ( p.y - v0.y))) ) * barycentric_conversion_factor);
            lambda2 = (DLR_Number)1 - lambda0 - lambda1;

            DLR_Vertex edge0 = {v2.x-v1.x, v2.y-v1.y}; // v2 - v1
            DLR_Vertex edge1 = {v0.x-v2.x, v0.y-v2.y}; // v0 - v2
            DLR_Vertex edge2 = {v1.x-v0.x, v1.y-v0.y}; // v1 - v0
            bool overlaps = true;
            overlaps &= DLR_WithinEdgeAreaClockwise(lambda0, edge0.x, edge0.y);
            overlaps &= DLR_WithinEdgeAreaClockwise(lambda1, edge1.x, edge1.y);
            overlaps &= DLR_WithinEdgeAreaClockwise(lambda2, edge2.x, edge2.y);
            if (overlaps) {
                DLR_Color<DLR_Number> fincomingC = \
                    (DLR_VertexColor<DLR_Number, DLR_Vertex>(v0) * lambda0) +
                    (DLR_VertexColor<DLR_Number, DLR_Vertex>(v1) * lambda1) +
                    (DLR_VertexColor<DLR_Number, DLR_Vertex>(v2) * lambda2);
                DLR_Color<Uint8> nincomingC = DLR_ConvertColorToBytes(fincomingC);
                DLR_AssertValidColor8888(nincomingC);

                DLR_Color<Uint8> nfinalC = {0xff, 0xff, 0x00, 0xff};  // default to ugly color
                DLR_Number uv = (DLR_Number)0;
                DLR_Number uw = (DLR_Number)0;
                DLR_Number ftexX = (DLR_Number)0;
                DLR_Number ftexY = (DLR_Number)0;
                int ntexX = 0;
                int ntexY = 0;
                DLR_Color<Uint8> ntexC = {0, 0, 0, 0};
                DLR_Color<DLR_Number> ftexC = {(DLR_Number)0, (DLR_Number)0, (DLR_Number)0, (DLR_Number)0};
                DLR_Color<DLR_Number> ffinalC = {(DLR_Number)0, (DLR_Number)0, (DLR_Number)0, (DLR_Number)0};
                
                if (state->texture.pixels) {
                    uv = (lambda0 * v0.uv) + (lambda1 * v1.uv) + (lambda2 * v2.uv);
                    uw = (lambda0 * v0.uw) + (lambda1 * v1.uw) + (lambda2 * v2.uw);
                    ftexX = (uv * (DLR_Number)(state->texture.w /* - 1*/));
                    ftexY = (uw * (DLR_Number)(state->texture.h /* - 1*/));
                    ntexX = DLR_Min((int)ftexX, state->texture.w - 1);
                    ntexY = DLR_Min((int)ftexY, state->texture.h - 1);
                    DLR_Assert(ntexX >= 0 && ntexX < state->texture.w);
                    DLR_Assert(ntexY >= 0 && ntexY < state->texture.h);
                    ntexC = DLR_GetPixel32(state->texture.pixels, state->texture.pitch, 4, ntexX, ntexY);
                    ftexC = DLR_ConvertColorFromBytes<DLR_Number>(ntexC);
                    if (state->textureModulate & DLR_TEXTUREMODULATE_COLOR) {
                        ffinalC = ftexC * fincomingC;
                        nfinalC = DLR_ConvertColorToBytes(ffinalC);
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
                        DLR_Color<DLR_Number> fsrc = ffinalC;
                        DLR_Color<Uint8> ndest = (DLR_Color<Uint8>) DLR_GetPixel32(state->dest.pixels, state->dest.pitch, 4, x, y);
                        DLR_Color<DLR_Number> fdest = DLR_ConvertColorFromBytes<DLR_Number>(ndest);

                        // dstRGB = (srcRGB * srcA) + (dstRGB * (1-srcA))
                        fdest.R = (fsrc.R * fsrc.A) + (fdest.R * ((DLR_Number)1 - fsrc.A));
                        fdest.G = (fsrc.G * fsrc.A) + (fdest.G * ((DLR_Number)1 - fsrc.A));
                        fdest.B = (fsrc.B * fsrc.A) + (fdest.B * ((DLR_Number)1 - fsrc.A));

                        // dstA = srcA + (dstA * (1-srcA))
                        fdest.A = fsrc.A + (fdest.A * ((DLR_Number)1 - fsrc.A));

                        ndest = DLR_ConvertColorToBytes(fdest);
                        DLR_AssertValidColor8888(ndest);
                        DLR_SetPixel32(state->dest.pixels, state->dest.pitch, 4, x, y, DLR_Join(ndest));
                    } break;
                }
            } // if (overlaps)
        }
    }
}

DLR_EXTERN_C void DLR_DrawTriangleX(DLR_State * state, DLR_VertexX v0, DLR_VertexX v1, DLR_VertexX v2)
{
    DLR_DrawTriangleT<DLR_Fixed, DLR_VertexX, DLR_FixedBig>(state, v0, v1, v2);
}

DLR_EXTERN_C void DLR_DrawTrianglesX(DLR_State * state, DLR_VertexX * vertices, size_t vertexCount)
{
    for (size_t i = 0; (i + 2) < vertexCount; i += 3) {
        DLR_DrawTriangleX(state, vertices[i], vertices[i+1], vertices[i+2]);
    }
}

DLR_EXTERN_C void DLR_DrawTrianglesD(DLR_State * state, DLR_VertexD * vertices, size_t vertexCount)
{
    DLR_VertexX converted[3];
    for (size_t i = 0; (i + 2) < vertexCount; i += 3) {
#if DLR_USE_DOUBLE
        DLR_DrawTriangleT<double, DLR_VertexD, double>(state, vertices[i], vertices[i+1], vertices[i+2]);
#else
        for (int j = 0; j < 3; ++j) {
            converted[j] = {
                (DLR_Fixed)vertices[i+j].x,
                (DLR_Fixed)vertices[i+j].y,
                (DLR_Fixed)vertices[i+j].a,
                (DLR_Fixed)vertices[i+j].r,
                (DLR_Fixed)vertices[i+j].g,
                (DLR_Fixed)vertices[i+j].b,
                (DLR_Fixed)vertices[i+j].uv,
                (DLR_Fixed)vertices[i+j].uw
            };
        }
        DLR_DrawTriangleX(state, converted[0], converted[1], converted[2]);
#endif
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

