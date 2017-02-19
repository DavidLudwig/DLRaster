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

#include <stddef.h>
#include <stdint.h>

//#define DLR_USE_DOUBLE 1

#define DLR_FIXED_PRECISION 14

template <typename DLR_Integer, int DLR_Precision, typename DLR_IntegerBig>
struct DLR_FixedT {
    DLR_Integer data;

#ifdef __cplusplus
    static const int Precision = DLR_Precision;

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
            data = ((DLR_Integer)src.data) << (DLR_Precision - DLR_PrecisionSrc);
        } else if (DLR_PrecisionSrc > DLR_Precision) {
            data = (DLR_Integer)(src.data >> (DLR_PrecisionSrc - DLR_Precision));
        } else {
            data = (DLR_Integer)src.data;
        }
    }

    inline DLR_FixedT operator+(DLR_FixedT b) const { return FromRaw(data + b.data); }
    inline DLR_FixedT operator-(DLR_FixedT b) const { return FromRaw(data - b.data); }
    inline DLR_FixedT operator/(DLR_FixedT b) const { return FromRaw(((DLR_IntegerBig)data << DLR_Precision) / (DLR_IntegerBig)b.data); }
    inline DLR_FixedT operator*(DLR_FixedT b) const { return FromRaw(((DLR_IntegerBig)data * (DLR_IntegerBig)b.data) >> DLR_Precision); }
    inline DLR_FixedT operator>>(unsigned int shift) const { return FromRaw(data >> shift); }
    inline DLR_FixedT operator<<(unsigned int shift) const { return FromRaw(data << shift); }

    inline DLR_FixedT & operator+=(DLR_FixedT b) { *this = *this + b; return *this; }

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

template <typename DLR_Number>
struct DLR_Point {
    DLR_Number x;
    DLR_Number y;
};

typedef struct DLR_VertexX {
    DLR_Fixed x;
    DLR_Fixed y;
    DLR_Fixed b;
    DLR_Fixed g;
    DLR_Fixed r;
    DLR_Fixed a;
    DLR_Fixed uv;
    DLR_Fixed uw;
} DLR_VertexX;

typedef struct DLR_VertexD {
    double x;
    double y;
    double b;
    double g;
    double r;
    double a;
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
    void * userData;
} DLR_State;

#define DLR_GetPixel32(PIXELS, PITCH, BYTESPP, X, Y) \
    (*(uint32_t *)((uint8_t *)(PIXELS) + (((Y) * (PITCH)) + ((X) * (BYTESPP)))))

#define DLR_SetPixel32(PIXELS, PITCH, BYTESPP, X, Y, COLOR) \
    (*(uint32_t *)((uint8_t *)(PIXELS) + (((Y) * (PITCH)) + ((X) * (BYTESPP))))) = (COLOR)

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
    uint32_t color);

DLR_EXTERN_C void DLR_DrawTriangleX(
    DLR_State * state,
    DLR_VertexX v0,
    DLR_VertexX v1,
    DLR_VertexX v2);

DLR_EXTERN_C void DLR_DrawTrianglesX(
    DLR_State * state,
    const DLR_VertexX * vertices,
    size_t vertexCount);

DLR_EXTERN_C void DLR_DrawTrianglesD(
    DLR_State * state,
    const DLR_VertexD * vertices,
    size_t vertexCount);

#endif // ifndef DL_RASTER_H

#ifdef DL_RASTER_IMPLEMENTATION

DLR_EXTERN_C void DLR_Clear(
    DLR_State * state,
    uint32_t color)
{
    const int ymax = state->dest.h;
    const int xmax = (state->dest.pitch - 4);
    const uint8_t * pixels = (uint8_t *) state->dest.pixels;
    const int pitch = state->dest.pitch;
    int y, x;
    for (y = 0; y < ymax; ++y) {
        for (x = 0; x <= xmax; x += 4) {
            *((uint32_t *)(pixels + (y * pitch) + x)) = color;
        }
    }
}

#define DLR_MAX_COLOR_COMPONENT 256

template <typename DLR_Number>
inline uint8_t DLR_ConvertColorComponentToByte(DLR_Number in) {
    return (uint8_t)((in * (DLR_Number)DLR_MAX_COLOR_COMPONENT) + (DLR_Number)0.5f);
}

template <>
inline uint8_t DLR_ConvertColorComponentToByte(DLR_Fixed in) {
    return (uint8_t)((in.data >> (DLR_Fixed::Precision - 8)) + ((DLR_Fixed)0.5f).data);
}

template <typename DLR_Number>
inline DLR_Number DLR_ConvertColorComponentFromByte(uint8_t in) {
    return (DLR_Number)in / (DLR_Number)DLR_MAX_COLOR_COMPONENT;
}

template <>
inline DLR_Fixed DLR_ConvertColorComponentFromByte(uint8_t in) {
    return DLR_Fixed::FromRaw(((int32_t)in) << (DLR_Fixed::Precision - 8));
}

template <typename DLR_ColorComponent>
struct DLR_Color {
    DLR_ColorComponent b;
    DLR_ColorComponent g;
    DLR_ColorComponent r;
    DLR_ColorComponent a;

    DLR_Color() = default;

    DLR_Color(DLR_ColorComponent b_, DLR_ColorComponent g_, DLR_ColorComponent r_, DLR_ColorComponent a_) {
        b = b_;
        g = g_;
        r = r_;
        a = a_;
    }

    DLR_Color(uint32_t argb) {
        a = (argb >> 24) & 0xff;
        r = (argb >> 16) & 0xff;
        g = (argb >>  8) & 0xff;
        b = (argb      ) & 0xff;
    }

#if 0	// the following does not work with MSVC 2015, so we'll have and use DLR_ConvertColor instead
    explicit DLR_Color(DLR_Color<uint8_t> other) {
        b = DLR_ConvertColorComponentFromByte<DLR_ColorComponent>(other.b);
        g = DLR_ConvertColorComponentFromByte<DLR_ColorComponent>(other.g);
        r = DLR_ConvertColorComponentFromByte<DLR_ColorComponent>(other.r);
        a = DLR_ConvertColorComponentFromByte<DLR_ColorComponent>(other.a);
    }

    explicit operator DLR_Color<uint8_t>() const {
        return {
            DLR_ConvertColorComponentToByte(b),
            DLR_ConvertColorComponentToByte(g),
            DLR_ConvertColorComponentToByte(r),
            DLR_ConvertColorComponentToByte(a),
        };
    }
#endif

    uint32_t ToARGB32() const { return DLR_JoinARGB32(a, r, g, b); }

    DLR_Color operator + (DLR_Color other) const { return {b + other.b, g + other.g, r + other.r, a + other.a}; }
    DLR_Color operator * (DLR_Color other) const { return {b * other.b, g * other.g, r * other.r, a * other.a}; }

    template <typename DLR_Coefficient>
    DLR_Color operator * (DLR_Coefficient coeff) const { return {b * coeff, g * coeff, r * coeff, a * coeff}; }

    template <typename DLR_Coefficient>
    DLR_Color operator / (DLR_Coefficient coeff) const { return {b / coeff, g / coeff, r / coeff, a / coeff}; }

    DLR_Color operator >> (unsigned int shift) const { return {b >> shift, g >> shift, r >> shift, a >> shift}; }
    DLR_Color operator << (unsigned int shift) const { return {b << shift, g << shift, r << shift, a << shift}; }
};

template <typename DLR_DestColorComponent, typename DLR_SrcColorComponent>
static inline DLR_Color<DLR_DestColorComponent> DLR_ConvertColor(DLR_Color<DLR_SrcColorComponent> src);

template <>
inline DLR_Color<DLR_Fixed> DLR_ConvertColor(DLR_Color<uint8_t> src) {
	return {
		DLR_ConvertColorComponentFromByte<DLR_Fixed>(src.b),
		DLR_ConvertColorComponentFromByte<DLR_Fixed>(src.g),
		DLR_ConvertColorComponentFromByte<DLR_Fixed>(src.r),
		DLR_ConvertColorComponentFromByte<DLR_Fixed>(src.a),
	};
}

template <>
inline DLR_Color<uint8_t> DLR_ConvertColor(DLR_Color<DLR_Fixed> src) {
	return {
		DLR_ConvertColorComponentToByte(src.b),
		DLR_ConvertColorComponentToByte(src.g),
		DLR_ConvertColorComponentToByte(src.r),
		DLR_ConvertColorComponentToByte(src.a),
	};
}

template <>
inline DLR_Color<double> DLR_ConvertColor(DLR_Color<uint8_t> src) {
    return {
        DLR_ConvertColorComponentFromByte<double>(src.b),
        DLR_ConvertColorComponentFromByte<double>(src.g),
        DLR_ConvertColorComponentFromByte<double>(src.r),
        DLR_ConvertColorComponentFromByte<double>(src.a),
    };
}

template <>
inline DLR_Color<uint8_t> DLR_ConvertColor(DLR_Color<double> src) {
    return {
        DLR_ConvertColorComponentToByte(src.b),
        DLR_ConvertColorComponentToByte(src.g),
        DLR_ConvertColorComponentToByte(src.r),
        DLR_ConvertColorComponentToByte(src.a),
    };
}

#define DLR_AssertValidColor8888(C) \
    DLR_Assert(C.b >= 0 && C.b <= 255); \
    DLR_Assert(C.g >= 0 && C.g <= 255); \
    DLR_Assert(C.r >= 0 && C.r <= 255); \
    DLR_Assert(C.a >= 0 && C.a <= 255);

template <typename DLR_Number, typename DLR_Vertex>
const DLR_Color<DLR_Number> & DLR_VertexColor(const DLR_Vertex & v) {
    return * (const DLR_Color<DLR_Number> * const) &(v.b);
}

template <typename DLR_Number>
static inline bool DLR_WithinEdgeAreaClockwise(DLR_Number barycentric, DLR_Number edgeX, DLR_Number edgeY)
{
    if (barycentric > (DLR_Number)0) { return true; }
    if (barycentric < (DLR_Number)0) { return false; }
    if (edgeY == (DLR_Number)0 && edgeX > (DLR_Number)0) { return true; }
    if (edgeY < (DLR_Number)0) { return true; }
    return false;
}

template <typename DLR_Number, typename DLR_Vertex>
static inline void DLR_PixelShade_Generic(
    const DLR_Vertex & v0,
    const DLR_Vertex & v1,
    const DLR_Vertex & v2,
    const DLR_Number lambda0,
    const DLR_Number lambda1,
    const DLR_Number lambda2,
    const DLR_State * const state,
    const int x,
    const int y
)
{
    DLR_Color<DLR_Number> fincomingC = \
        (DLR_VertexColor<DLR_Number, DLR_Vertex>(v0) * lambda0) +
        (DLR_VertexColor<DLR_Number, DLR_Vertex>(v1) * lambda1) +
        (DLR_VertexColor<DLR_Number, DLR_Vertex>(v2) * lambda2);

    if (state->texture.pixels) {
        const DLR_Number uv = (lambda0 * v0.uv) + (lambda1 * v1.uv) + (lambda2 * v2.uv);
        const DLR_Number uw = (lambda0 * v0.uw) + (lambda1 * v1.uw) + (lambda2 * v2.uw);
        const DLR_Number ftexX = (uv * (DLR_Number)(state->texture.w /* - 1*/));
        const DLR_Number ftexY = (uw * (DLR_Number)(state->texture.h /* - 1*/));
        const int ntexX = DLR_Min((int)ftexX, state->texture.w - 1);
        const int ntexY = DLR_Min((int)ftexY, state->texture.h - 1);
        DLR_Assert(ntexX >= 0 && ntexX < state->texture.w);
        DLR_Assert(ntexY >= 0 && ntexY < state->texture.h);
        const DLR_Color<uint8_t> ntexC = DLR_GetPixel32(state->texture.pixels, state->texture.pitch, 4, ntexX, ntexY);
        DLR_Color<DLR_Number> ftexC = DLR_ConvertColor<DLR_Number, uint8_t>(ntexC);
        if (state->textureModulate & DLR_TEXTUREMODULATE_COLOR) {
            fincomingC = ftexC * fincomingC;
        } else {
            fincomingC = ftexC;
        }
    } // if tex ...

    // color is ARGB
    switch (state->blendMode) {
        case DLR_BLENDMODE_NONE: {
            const DLR_Color<uint8_t> nfinalC = DLR_ConvertColor<uint8_t, DLR_Number>(fincomingC);
            DLR_AssertValidColor8888(nfinalC);
            DLR_SetPixel32(state->dest.pixels, state->dest.pitch, 4, x, y, nfinalC.ToARGB32());
        } break;

        case DLR_BLENDMODE_BLEND: {
            DLR_Color<uint8_t> ndest = (DLR_Color<uint8_t>) DLR_GetPixel32(state->dest.pixels, state->dest.pitch, 4, x, y);
            DLR_Color<DLR_Number> fdest = DLR_ConvertColor<DLR_Number, uint8_t>(ndest);

            // dstRGB = (srcRGB * srcA) + (dstRGB * (1-srcA))
            fdest.r = (fincomingC.r * fincomingC.a) + (fdest.r * ((DLR_Number)1 - fincomingC.a));
            fdest.g = (fincomingC.g * fincomingC.a) + (fdest.g * ((DLR_Number)1 - fincomingC.a));
            fdest.b = (fincomingC.b * fincomingC.a) + (fdest.b * ((DLR_Number)1 - fincomingC.a));

            // dstA = srcA + (dstA * (1-srcA))
            fdest.a = fincomingC.a + (fdest.a * ((DLR_Number)1 - fincomingC.a));

            ndest = DLR_ConvertColor<uint8_t, DLR_Number>(fdest);
            DLR_AssertValidColor8888(ndest);
            DLR_SetPixel32(state->dest.pixels, state->dest.pitch, 4, x, y, ndest.ToARGB32());
        } break;
    } // switch (blendMode)
}

template <typename DLR_Number, typename DLR_Vertex>
static inline void DLR_PixelShade_White(
    const DLR_Vertex & v0,
    const DLR_Vertex & v1,
    const DLR_Vertex & v2,
    const DLR_Number lambda0,
    const DLR_Number lambda1,
    const DLR_Number lambda2,
    const DLR_State * state,
    const int x,
    const int y
)
{
    DLR_SetPixel32(state->dest.pixels, state->dest.pitch, 4, x, y, 0xffffffff);
}

template <typename DLR_Number, typename DLR_Vertex>
void DLR_DrawTriangleT_Loop(
    const int xmin,
    const int xmax,
    const int ymin,
    const int ymax,
    const DLR_Vertex & v0,
    const DLR_Vertex & v1,
    const DLR_Vertex & v2,
    DLR_Number lambda0_row,
    DLR_Number lambda1_row,
    DLR_Number lambda2_row,
    const DLR_Number lambda0_xstep,
    const DLR_Number lambda1_xstep,
    const DLR_Number lambda2_xstep,
    const DLR_Number lambda0_ystep,
    const DLR_Number lambda1_ystep,
    const DLR_Number lambda2_ystep,
    const DLR_State * const state
)
{
    int y, x;
    DLR_Number lambda0, lambda1, lambda2;

    const DLR_Number edge0X = v2.x - v1.x;
    const DLR_Number edge0Y = v2.y - v1.y;
    const DLR_Number edge1X = v0.x - v2.x;
    const DLR_Number edge1Y = v0.y - v2.y;
    const DLR_Number edge2X = v1.x - v0.x;
    const DLR_Number edge2Y = v1.y - v0.y;

    for (y = ymin; y <= ymax; ++y) {
        lambda0 = lambda0_row;
        lambda1 = lambda1_row;
        lambda2 = lambda2_row;

        for (x = xmin; x <= xmax; ++x) {
            if (DLR_WithinEdgeAreaClockwise(lambda0, edge0X, edge0Y) &&
                DLR_WithinEdgeAreaClockwise(lambda1, edge1X, edge1Y) &&
                DLR_WithinEdgeAreaClockwise(lambda2, edge2X, edge2Y))
            {
                DLR_PixelShade_Generic
                // DLR_PixelShade_White
                (
                    v0,
                    v1,
                    v2,
                    lambda0,
                    lambda1,
                    lambda2,
                    state,
                    x,
                    y
                );
            }

            lambda0 += lambda0_xstep;
            lambda1 += lambda1_xstep;
            lambda2 += lambda2_xstep;
        } // for X

        lambda0_row += lambda0_ystep;
        lambda1_row += lambda1_ystep;
        lambda2_row += lambda2_ystep;
    } // for Y
}

template <typename DLR_Number, typename DLR_Vertex, typename DLR_NumberBig>
void DLR_DrawTriangleT(const DLR_State * const state, DLR_Vertex v0, DLR_Vertex v1, DLR_Vertex v2)
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

    // Use the center of the pixel, to determine whether to rasterize
    const DLR_Number xy_offset = (DLR_Number)0.5f;
    const DLR_Point<DLR_Number> p = {(DLR_Number)xmin + xy_offset, (DLR_Number)ymin + xy_offset};

    // Precompute a common part of the triangle's barycentric coordinates,
    // in a form that won't require division later-on.
    const DLR_NumberBig barycentric_conversion_factor = ( (DLR_NumberBig)1 / (DLR_NumberBig)((v1.y - v2.y) * (v0.x - v2.x) + (v2.x - v1.x) * (v0.y - v2.y)));

    DLR_Number lambda0_row = (DLR_Number)(((DLR_NumberBig) (((v1.y - v2.y) * ( p.x - v2.x) + (v2.x - v1.x) * ( p.y - v2.y)))) * barycentric_conversion_factor);
    DLR_Number lambda1_row = (DLR_Number)(((DLR_NumberBig) (((v2.y - v0.y) * ( p.x - v2.x) + (v0.x - v2.x) * ( p.y - v2.y)))) * barycentric_conversion_factor);
    DLR_Number lambda2_row = (DLR_Number)(((DLR_NumberBig) (((v0.y - v1.y) * ( p.x - v0.x) + (v1.x - v0.x) * ( p.y - v0.y)))) * barycentric_conversion_factor);
    const DLR_Number lambda0_xstep = (DLR_Number)((DLR_NumberBig)(v1.y - v2.y) * barycentric_conversion_factor);
    const DLR_Number lambda0_ystep = (DLR_Number)((DLR_NumberBig)(v2.x - v1.x) * barycentric_conversion_factor);
    const DLR_Number lambda1_xstep = (DLR_Number)((DLR_NumberBig)(v2.y - v0.y) * barycentric_conversion_factor);
    const DLR_Number lambda1_ystep = (DLR_Number)((DLR_NumberBig)(v0.x - v2.x) * barycentric_conversion_factor);
    const DLR_Number lambda2_xstep = (DLR_Number)((DLR_NumberBig)(v0.y - v1.y) * barycentric_conversion_factor);
    const DLR_Number lambda2_ystep = (DLR_Number)((DLR_NumberBig)(v1.x - v0.x) * barycentric_conversion_factor);

    DLR_DrawTriangleT_Loop(
        xmin,
        xmax,
        ymin,
        ymax,
        v0,
        v1,
        v2,
        lambda0_row,
        lambda1_row,
        lambda2_row,
        lambda0_xstep,
        lambda1_xstep,
        lambda2_xstep,
        lambda0_ystep,
        lambda1_ystep,
        lambda2_ystep,
        state
    );
}

DLR_EXTERN_C void DLR_DrawTriangleX(DLR_State * state, DLR_VertexX v0, DLR_VertexX v1, DLR_VertexX v2)
{
    DLR_DrawTriangleT<DLR_Fixed, DLR_VertexX, DLR_FixedBig>(state, v0, v1, v2);
}

DLR_EXTERN_C void DLR_DrawTrianglesX(DLR_State * state, const DLR_VertexX * vertices, size_t vertexCount)
{
    for (size_t i = 0; (i + 2) < vertexCount; i += 3) {
        DLR_DrawTriangleX(state, vertices[i], vertices[i+1], vertices[i+2]);
    }
}

DLR_EXTERN_C void DLR_DrawTrianglesD(DLR_State * state, const DLR_VertexD * vertices, size_t vertexCount)
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
                (DLR_Fixed)vertices[i+j].b,
                (DLR_Fixed)vertices[i+j].g,
                (DLR_Fixed)vertices[i+j].r,
                (DLR_Fixed)vertices[i+j].a,
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

