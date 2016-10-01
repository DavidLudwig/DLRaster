// DL_Raster.h - SUPER-EARLY WORK-IN-PROGRESS on a new, 2d rasterization library; BUGGY!
// Public domain, Sep 25 2016, David Ludwig, dludwig@pobox.com. See "unlicense" statement at the end of this file.

#ifndef DLRASTER_H
#define DLRASTER_H

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

typedef struct DLR_Point {
    double x;
    double y;
} DLR_Point;

typedef struct DLR_Vertex {
    double x;
    double y;
    double a;
    double r;
    double g;
    double b;
    double uv;
    double uw;
} DLR_Vertex;

typedef struct DLR_State {
    SDL_Surface * dest;     // a NON-owning pointer!  TODO: remove LibSDL dependency 
    SDL_Surface * texture;  // a NON-owning pointer!  TODO: remove LibSDL dependency
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

void DLR_Clear(
    DLR_State * state,
    Uint32 color);

void DLR_CalculateBarycentricCoordinates(
    DLR_Vertex p,
    DLR_Vertex a,
    DLR_Vertex b,
    DLR_Vertex c,
    double *lambdaA,
    double *lambdaB,
    double *lambdaC);

void DLR_DrawTriangle(
    DLR_State * state,
    DLR_Vertex v0,
    DLR_Vertex v1,
    DLR_Vertex v2);

void DLR_DrawTriangles(
    DLR_State * state,
    DLR_Vertex * vertices,
    size_t vertexCount);

#endif // ifndef DLRASTER_H

#ifdef DL_RASTER_IMPLEMENTATION

void DLR_Clear(
    DLR_State * state,
    Uint32 color)
{
    SDL_FillRect(state->dest, NULL, color);
}

//double DLR_CalculateOrientation(DLR_Vertex a, DLR_Vertex b, DLR_Vertex c) {
//    //return (b.x-a.x) * (c.y-a.y) - (b.y-a.y) * (c.x-a.x));
//    return (b.x-a.x)*(c.y-a.y) - (b.y-a.y)*(c.x-a.x);
//}

void DLR_CalculateBarycentricCoordinates(DLR_Vertex p, DLR_Vertex a, DLR_Vertex b, DLR_Vertex c, double *lambdaA, double *lambdaB, double *lambdaC) {
    *lambdaA =
        ((b.y - c.y) * (p.x - c.x) + (c.x - b.x) * (p.y - c.y)) /
        ((b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y));

    *lambdaB =
        ((c.y - a.y) * (p.x - c.x) + (a.x - c.x) * (p.y - c.y)) /
        ((b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y));

    *lambdaC =
        ((a.y - b.y) * (p.x - a.x) + (b.x - a.x) * (p.y - a.y)) /
        ((b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y));

    //double lambdaC2 = 
    //    1.f - *lambdaA - *lambdaB;
    //if (abs(*lambdaC - lambdaC2) >= 0.000000000000001) { // add an extra 0 to the right of the . to make a failed assertion
    //    SDL_Log("inequal: %f  %f", *lambdaC, lambdaC2);
    //    SDL_assert(false);
    //}
}

static inline bool DLR_WithinEdgeAreaClockwise(double barycentric, double edgeX, double edgeY)
{
    if (barycentric == 0) {
        if (edgeY == 0 && edgeX > 0) {
            return true;
        } else if (edgeY < 0) {
            return true;
        } else {
            return false;
        }
    } else {
        if (barycentric > 0) {
            return true;
        } else {
            return false;
        }
    }
}

void DLR_DrawTriangle(DLR_State * state, DLR_Vertex v0, DLR_Vertex v1, DLR_Vertex v2)
{
    // TODO: consider adding +1 to *max vars, to prevent clipping.  This'll depend on how we determine if a pixel is lit.
    int ymin = (int) SDL_min(v0.y, SDL_min(v1.y, v2.y));
    int ymax = (int) SDL_max(v0.y, SDL_max(v1.y, v2.y));
    int xmin = (int) SDL_min(v0.x, SDL_min(v1.x, v2.x));
    int xmax = (int) SDL_max(v0.x, SDL_max(v1.x, v2.x));

    //xmax++;
    //ymax++;

    ymin = SDL_max(ymin, 0);
    ymax = SDL_min(ymax, (state->dest->h - 1));
    xmin = SDL_max(xmin, 0);
    xmax = SDL_min(xmax, (state->dest->w - 1));

    double lambda0;
    double lambda1;
    double lambda2;

    for (int y = ymin; y <= ymax; ++y) {
        for (int x = 0; x <= xmax; ++x) {
            DLR_Vertex p = {(double)x, (double)y};
            p.x += 0.5; // Use the center of the pixel, to determine whether to rasterize
            p.y += 0.5;
            DLR_CalculateBarycentricCoordinates(p, v0, v1, v2, &lambda0, &lambda1, &lambda2);

//            if (lambda0 >= 0.f && lambda1 >= 0.f && lambda2 >= 0.f) {

            if (x == 23 && y == 26) {
                int foo = 1;
            }

            DLR_Vertex edge0 = {v2.x-v1.x/*-v2.x*/, v2.y-v1.y/*-v2.y*/}; // v2 - v1
            DLR_Vertex edge1 = {v0.x-v2.x/*-v0.x*/, v0.y-v2.y/*-v0.y*/}; // v0 - v2
            DLR_Vertex edge2 = {v1.x-v0.x/*-v1.x*/, v1.y-v0.y/*-v1.y*/}; // v1 - v0
            bool overlaps = true;
            overlaps &= DLR_WithinEdgeAreaClockwise(lambda0, edge0.x, edge0.y);
            overlaps &= DLR_WithinEdgeAreaClockwise(lambda1, edge1.x, edge1.y);
            overlaps &= DLR_WithinEdgeAreaClockwise(lambda2, edge2.x, edge2.y);
            if (overlaps) {
                double fincomingA = (lambda0 * v0.a) + (lambda1 * v1.a) + (lambda2 * v2.a);
                double fincomingR = (lambda0 * v0.r) + (lambda1 * v1.r) + (lambda2 * v2.r);
                double fincomingG = (lambda0 * v0.g) + (lambda1 * v1.g) + (lambda2 * v2.g);
                double fincomingB = (lambda0 * v0.b) + (lambda1 * v1.b) + (lambda2 * v2.b);
                int nincomingA = (int)round(fincomingA * 255.0);
                int nincomingR = (int)round(fincomingR * 255.0);
                int nincomingG = (int)round(fincomingG * 255.0);
                int nincomingB = (int)round(fincomingB * 255.0);
                SDL_assert(nincomingA >= 0 && nincomingA <= 255);
                SDL_assert(nincomingR >= 0 && nincomingR <= 255);
                SDL_assert(nincomingG >= 0 && nincomingG <= 255);
                SDL_assert(nincomingB >= 0 && nincomingB <= 255);

                Uint32 nfinalC = 0xffff00ff;  // default to ugly color

                // TODO: see if &'s are necessary, in practice
                //Uint32 incomingC = 0xff000000 | (nincomingR << 16) | (nincomingG << 8) | nincomingB;
                
                Uint32 incomingC = 0xff000000 | ((nincomingR << 16) & 0x00ff0000) | ((nincomingG << 8) & 0x0000ff00) | (nincomingB & 0x000000ff);
                //SDL_assert(incomingC == incoming2C);

                double uv = 0.;
                double uw = 0.;
                double ftexX = 0.;
                double ftexY = 0.;
                int ntexX = 0;
                int ntexY = 0;
                Uint32 ntexC = 0;
                int ntexA = 0;
                int ntexR = 0;
                int ntexG = 0;
                int ntexB = 0;
                int nfinalA = 0;
                int nfinalR = 0;
                int nfinalG = 0;
                int nfinalB = 0;

                if (state->texture) {
                    uv = (lambda0 * v0.uv) + (lambda1 * v1.uv) + (lambda2 * v2.uv);
                    uw = (lambda0 * v0.uw) + (lambda1 * v1.uw) + (lambda2 * v2.uw);
                    ftexX = (uv * (double)(state->texture->w /* - 1*/));
                    ftexY = (uw * (double)(state->texture->h /* - 1*/));
                    ntexX = SDL_min((int)/*round*/(ftexX), state->texture->w - 1);
                    ntexY = SDL_min((int)/*round*/(ftexY), state->texture->h - 1);
                    SDL_assert(ntexX >= 0 && ntexX < state->texture->w);
                    SDL_assert(ntexY >= 0 && ntexY < state->texture->h);

                    Uint32 ntexC = DLR_GetPixel32(state->texture->pixels, state->texture->pitch, 4, ntexX, ntexY);
                    if (state->textureModulate & DLR_TEXTUREMODULATE_COLOR) {
                        ntexA = (ntexC >> 24) & 0xff;
                        ntexR = (ntexC >> 16) & 0xff;
                        ntexG = (ntexC >>  8) & 0xff;
                        ntexB = (ntexC      ) & 0xff;
                        SDL_assert(ntexA >= 0 && ntexA <= 255);
                        SDL_assert(ntexR >= 0 && ntexR <= 255);
                        SDL_assert(ntexG >= 0 && ntexG <= 255);
                        SDL_assert(ntexB >= 0 && ntexB <= 255);

                        nfinalA = (ntexA * nincomingA) / 255;
                        nfinalR = (ntexR * nincomingR) / 255;
                        nfinalG = (ntexG * nincomingG) / 255;
                        nfinalB = (ntexB * nincomingB) / 255;
                        SDL_assert(nfinalA >= 0 && nfinalA <= 255);
                        SDL_assert(nfinalR >= 0 && nfinalR <= 255);
                        SDL_assert(nfinalG >= 0 && nfinalG <= 255);
                        SDL_assert(nfinalB >= 0 && nfinalB <= 255);
                        nfinalC = \
                            (nfinalA << 24) |
                            (nfinalR << 16) |
                            (nfinalG <<  8) |
                            (nfinalB      );
                    } else {
                        nfinalC = ntexC;
                    }
                } else {
                    // texture is NULL
                    SDL_assert(state->texture == NULL);
                    nfinalC = incomingC;
                } // if tex ... ; else ...

                // color is ARGB
                switch (state->blendMode) {
                    case DLR_BLENDMODE_NONE: {
                        DLR_SetPixel32(state->dest->pixels, state->dest->pitch, 4, x, y, nfinalC);
                    } break;

                    case DLR_BLENDMODE_BLEND: {
                        int nsrcA, nsrcR, nsrcG, nsrcB;

                        DLR_SplitARGB32(nfinalC, nsrcA, nsrcR, nsrcG, nsrcB);
                        float fsrcA = (float)nsrcA / 255.f;
                        float fsrcR = (float)nsrcR / 255.f;
                        float fsrcG = (float)nsrcG / 255.f;
                        float fsrcB = (float)nsrcB / 255.f;

                        Uint32 ndestC = DLR_GetPixel32(state->dest->pixels, state->dest->pitch, 4, x, y);
                        int ndestA, ndestR, ndestG, ndestB;
                        DLR_SplitARGB32(ndestC, ndestA, ndestR, ndestG, ndestB);
                        ndestA = 255;
                        float fdestA = (float)ndestA / 255.f;
                        float fdestR = (float)ndestR / 255.f;
                        float fdestG = (float)ndestG / 255.f;
                        float fdestB = (float)ndestB / 255.f;

                        // dstRGB = (srcRGB * srcA) + (dstRGB * (1-srcA))
                        fdestR = (fsrcR * fsrcA) + (fdestR * (1.f - fsrcA));
                        fdestG = (fsrcG * fsrcA) + (fdestG * (1.f - fsrcA));
                        fdestB = (fsrcB * fsrcA) + (fdestB * (1.f - fsrcA));

                        // dstA = srcA + (dstA * (1-srcA))
                        fdestA = fsrcA + (fdestA * (1.f - fsrcA));

                        ndestA = (int)round(fdestA * 255.f);
                        ndestR = (int)round(fdestR * 255.f);
                        ndestG = (int)round(fdestG * 255.f);
                        ndestB = (int)round(fdestB * 255.f);
                        SDL_assert(ndestA >= 0 && ndestA <= 255);
                        SDL_assert(ndestR >= 0 && ndestR <= 255);
                        SDL_assert(ndestG >= 0 && ndestG <= 255);
                        SDL_assert(ndestB >= 0 && ndestB <= 255);

                        nfinalC = DLR_JoinARGB32(ndestA, ndestR, ndestG, ndestB);

                        DLR_SetPixel32(state->dest->pixels, state->dest->pitch, 4, x, y, nfinalC);
                    } break;
                }
            }
        }
    }
}

void DLR_DrawTriangles(DLR_State * state, DLR_Vertex * vertices, size_t vertexCount)
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

