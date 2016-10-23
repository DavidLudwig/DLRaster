// test.cpp - an interactive test program for the "DLRaster" 2d rasterization library; BUGGY!
// Public domain, Sep 25 2016, David Ludwig, dludwig@pobox.com. See "unlicense" statement at the end of this file.

// Disable deprecated C function warnings from MSVC:
#define _CRT_SECURE_NO_WARNINGS

#include "SDL.h"
// Make sure SDL doesn't redefine main
#undef main
#define GL_GLEXT_PROTOTYPES
#include "SDL_opengl.h"
#include "SDL_opengl_glext.h"
#pragma comment(lib, "opengl32.lib")
//#include <random>
//#include <mutex>
#include <stdio.h>

#define GB_MATH_IMPLEMENTATION
#include "gb_math.h"

#define DL_RASTER_IMPLEMENTATION
#include "DL_Raster.h"

#ifdef _WIN32
#define DLRTEST_D3D10 1
#include <d3d10.h>
#include "SDL_syswm.h"
#pragma comment(lib, "d3d10.lib")
#endif

typedef enum DLR_Test_RendererType {
    DLRTEST_TYPE_SOFTWARE = 0,
    DLRTEST_TYPE_OPENGL1,
    DLRTEST_TYPE_D3D10
} DLR_TestRenderer;

enum DLRTest_Env_Flags {
    DLRTEST_ENV_DEFAULTS    = 0,
    DLRTEST_ENV_COMPARE     = (1 << 0),
};

struct DLRTest_Env {
    DLR_Test_RendererType type;
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
#if DLRTEST_D3D10
        struct {
            IDXGISwapChain * swapChain;
            ID3D10Device * device;
            ID3D10RenderTargetView * renderTargetView;
            ID3D10RasterizerState * rasterState;
			ID3D10Effect * effect;
			ID3D10EffectTechnique * colorTechnique;
			ID3D10EffectTechnique * textureTechnique;
			ID3D10InputLayout * layout;
			ID3D10EffectMatrixVariable * viewMatrix;
			ID3D10EffectShaderResourceVariable * textureForShader;
			ID3D10BlendState * blendModeBlend;
			SDL_Surface * exported;	// Exported pixels
        } d3d10;
#endif
    } inner;
};

static int winW = 460;
static int winH = 400;

static DLRTest_Env envs[] = {
    {
        DLRTEST_TYPE_SOFTWARE,
        0, 60,              // window initial X and Y
        0,
    },

#if 1   // turn on to compare DLRaster with OpenGL or D3D10
#if DLRTEST_D3D10
    {
        DLRTEST_TYPE_D3D10,
        winW + 10, 60,            // window initial X and Y
        0,
    },
#else
	{
		DLRTEST_TYPE_OPENGL1,
		winW + 10, 60,            // window initial X and Y
		0,
	},
#endif
    {
        DLRTEST_TYPE_SOFTWARE,
        (winW + 10) * 2, 60,            // window initial X and Y
        DLRTEST_ENV_COMPARE,
    },
#endif
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

#define DLRTest_CheckGL() { GLenum glerr = glGetError(); (void)glerr; SDL_assert(glerr == GL_NO_ERROR); }

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

#if DLRTEST_D3D10
		case DLRTEST_TYPE_D3D10: {
			HRESULT hr;
			ID3D10Texture2D * backBuffer;
			hr = env->inner.d3d10.swapChain->GetBuffer(0, __uuidof(ID3D10Texture2D), (void **)&backBuffer);
			if (FAILED(hr)) {
				SDL_Log("swap chain GetBuffer() failed, to get back buffer");
				exit(-1);
			}

			ID3D10Texture2D * stagingTex;
			D3D10_TEXTURE2D_DESC texDesc;
			backBuffer->GetDesc(&texDesc);
			texDesc.CPUAccessFlags = D3D10_CPU_ACCESS_READ;
			texDesc.Usage = D3D10_USAGE_STAGING;
			texDesc.BindFlags = 0;
			hr = env->inner.d3d10.device->CreateTexture2D(&texDesc, NULL, &stagingTex);
			if (FAILED(hr)) {
				SDL_Log("d3d10 device CreateTexture2D failed, to create staging texture (for back buffer read)");
				exit(-1);
			}

			env->inner.d3d10.device->CopySubresourceRegion(
				stagingTex,
				0,
				0, 0, 0,
				backBuffer,
				0,
				NULL);

			D3D10_MAPPED_TEXTURE2D texture;
			hr = stagingTex->Map(D3D10CalcSubresource(0,0,0), D3D10_MAP_READ, 0, &texture);
			if (FAILED(hr)) {
				SDL_Log("back buffer Map() failed");
				exit(-1);
			}
			SDL_Surface * tmp = SDL_CreateRGBSurfaceFrom(
				texture.pData,
				winW, winH,
				32,
				texture.RowPitch,
				0x000000ff,
				0x0000ff00,
				0x00ff0000,
				0xff000000
			);
			if ( ! tmp) {
				SDL_Log("couldn't create SDL_Surface with raw back buffer data");
				exit(-1);
			}

			SDL_PixelFormat * targetFormat = SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888);
			SDL_Surface * output = SDL_ConvertSurface(tmp, targetFormat, 0);
			if ( ! output) {
				SDL_Log("couldn't convert d3d10 back buffer data to DLRTest format");
				exit(-1);
			}

			env->inner.d3d10.exported = output;
			SDL_FreeSurface(tmp);
			stagingTex->Release();
			backBuffer->Release();
			return output;
		} break;
#endif
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
        Uint32 c = ( ! (bg && curX >= 0)) ? 0 : DLR_GetPixel32(bg->pixels, bg->pitch, 4, curX, curY);
        Uint8 a, r, g, b;
        DLR_SplitARGB32(c, a, r, g, b);

		const char * typeName = "";
		switch (envs[i].type) {
			case DLRTEST_TYPE_OPENGL1:  typeName = "OGL"; break;
			case DLRTEST_TYPE_D3D10:    typeName = "D3D"; break;
			case DLRTEST_TYPE_SOFTWARE:
				if (envs[i].flags & DLRTEST_ENV_COMPARE) {
					typeName = "CMP";
				} else {
					typeName = "SW";
				}
				break;
		}
        
        const char * lockedText = DLRTEST_IS_LOCKED() ? ",lock" : "";

        if (envs[i].flags & DLRTEST_ENV_COMPARE) {
            SDL_snprintf(window_title, SDL_arraysize(window_title), "DLRTest %s %s +/-:%d p:%d,%d%s a,rgb:%02x,%02x%02x%02x",
                typeName, compare_name, compare_threshold, curX, curY, lockedText,
                a, r, g, b);
        } else {
            SDL_snprintf(window_title, SDL_arraysize(window_title), "DLRTest %s p:(%d,%d)%s a,rgb:0x%02x,0x%02x%02x%02x",
                typeName, curX, curY, lockedText,
                a, r, g, b);
        }
        window_title[SDL_arraysize(window_title)-1] = '\0';
        SDL_SetWindowTitle(envs[i].window, window_title);
    }
}

void DLRTest_DrawTriangles_OpenGL1(
	DLRTest_Env *,
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
        if (state->textureModulate == DLR_TEXTUREMODULATE_COLOR || state->texture == NULL) {
            glColor4d(vertices[i].r, vertices[i].g, vertices[i].b, vertices[i].a);
        } else {
            glColor4d(1., 1., 1., 1.);
        }
        glTexCoord2d(vertices[i].uv, vertices[i].uw);
        glVertex2d(vertices[i].x, vertices[i].y);
    }
    glEnd();
}

#if DLRTEST_D3D10

void DLRTest_DrawTriangles_D3D10(
	DLRTest_Env * env,
    DLR_State * state,
    DLR_Vertex * vertices,
    size_t vertexCount)
{
	struct InnerVertex {
		float x;
		float y;
		float z;
		float r;
		float g;
		float b;
		float a;
		float uv;
		float uw;
	};
	const size_t sizeofInnerBuffer = vertexCount * sizeof(InnerVertex);
	static ID3D10Buffer * vertexBufferGPU = NULL;
	D3D10_BUFFER_DESC bufferDesc;
	HRESULT hr;
	if (vertexBufferGPU) {
		vertexBufferGPU->GetDesc(&bufferDesc);
		if (bufferDesc.ByteWidth < sizeofInnerBuffer) {
			vertexBufferGPU->Release();
			vertexBufferGPU = NULL;
		}
	}
	if ( ! vertexBufferGPU) {
		memset(&bufferDesc, 0, sizeof(bufferDesc));
		bufferDesc.Usage = D3D10_USAGE_DYNAMIC;
		bufferDesc.ByteWidth = sizeofInnerBuffer;
		bufferDesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
		bufferDesc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
		bufferDesc.MiscFlags = 0;
		hr = env->inner.d3d10.device->CreateBuffer(&bufferDesc, NULL, &vertexBufferGPU);
		if (FAILED(hr)) {
			SDL_Log("d3d10 device CreateBuffer(), for vertex buffer, failed");
			exit(-1);
		}
	}
	SDL_assert(vertexBufferGPU != NULL);

	InnerVertex * vertexBufferCPU;
	hr = vertexBufferGPU->Map(D3D10_MAP_WRITE_DISCARD, 0, (void **)&vertexBufferCPU);
	if (FAILED(hr)) {
		SDL_Log("d3d10 vertex buffer Map() failed");
		exit(-1);
	}
	SDL_assert(vertexBufferCPU != NULL);
	for (size_t i = 0; i < vertexCount; ++i) {
		vertexBufferCPU[i].x = (float) vertices[i].x;
		vertexBufferCPU[i].y = (float) vertices[i].y;
		vertexBufferCPU[i].z = 0.f;
		vertexBufferCPU[i].a = (float) vertices[i].a;
		vertexBufferCPU[i].r = (float) vertices[i].r;
		vertexBufferCPU[i].g = (float) vertices[i].g;
		vertexBufferCPU[i].b = (float) vertices[i].b;
		vertexBufferCPU[i].uv = (float) vertices[i].uv;
		vertexBufferCPU[i].uw = (float) vertices[i].uw;
	}
	vertexBufferGPU->Unmap();

	UINT stride = sizeof(InnerVertex);
	UINT offset = 0;
	env->inner.d3d10.device->IASetVertexBuffers(0, 1, &vertexBufferGPU, &stride, &offset);
	env->inner.d3d10.device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	env->inner.d3d10.device->IASetInputLayout(env->inner.d3d10.layout);

	if ( ! state->texture) {
		hr = env->inner.d3d10.colorTechnique->GetPassByIndex(0)->Apply(0);
	} else {
		static ID3D10Texture2D * tex = NULL;
		static ID3D10ShaderResourceView * srView = NULL;
		if ( ! tex) {
			//
			// Super-hack!: For now, use one and only one D3D texture!  This may
			//  break horribly if two or more textures get used by the app.
			//

			D3D10_TEXTURE2D_DESC texDesc;
			memset(&texDesc, 0, sizeof(texDesc));
			texDesc.Width = state->texture->w;
			texDesc.Height = state->texture->h;
			texDesc.MipLevels = 1;
			texDesc.ArraySize = 1;
			texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; //DXGI_FORMAT_B8G8R8A8_UNORM;
			texDesc.SampleDesc.Count = 1;
			texDesc.SampleDesc.Quality = 0;
			texDesc.MiscFlags = 0;
			texDesc.Usage = D3D10_USAGE_DYNAMIC;
			texDesc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
			texDesc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
			hr = env->inner.d3d10.device->CreateTexture2D(&texDesc, NULL, &tex);
			if (FAILED(hr)) {
				SDL_Log("d3d10 device CreateTexture2D() failed for app texture");
				exit(-1);
			}

			D3D10_SHADER_RESOURCE_VIEW_DESC rvDesc;
			memset(&rvDesc, 0, sizeof(rvDesc));
			rvDesc.Format = texDesc.Format;
			rvDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
			rvDesc.Texture2D.MipLevels = texDesc.MipLevels;
			rvDesc.Texture2D.MostDetailedMip = 0;
			hr = env->inner.d3d10.device->CreateShaderResourceView(tex, &rvDesc, &srView);
			if (FAILED(hr)) {
				SDL_Log("d3d10 device CreateShaderResourceView() failed for app texture");
				exit(-1);
			}
		}

		D3D10_MAPPED_TEXTURE2D mapped;
		hr = tex->Map(D3D10CalcSubresource(0,0,0), D3D10_MAP_WRITE_DISCARD, 0, &mapped);
		if (FAILED(hr)) {
			SDL_Log("d3d10 texture Map() failed for app texture populate");
			exit(-1);
		}

		SDL_Surface * dest = SDL_CreateRGBSurfaceFrom(
			mapped.pData,
			state->texture->w,
			state->texture->h,
			32,
			mapped.RowPitch,
			0x000000ff,
			0x0000ff00,
			0x00ff0000,
			0xff000000);
		SDL_BlitSurface(state->texture, NULL, dest, NULL);
		tex->Unmap(D3D10CalcSubresource(0,0,0));
		SDL_FreeSurface(dest);

		hr = env->inner.d3d10.textureForShader->SetResource(srView);
		if (FAILED(hr)) {
			SDL_Log("d3d10 shader resource variable SetResource() failed for app texture");
			exit(-1);
		}

		hr = env->inner.d3d10.textureTechnique->GetPassByIndex(0)->Apply(0);
	}

	ID3D10BlendState * blendState;
	switch (state->blendMode) {
		case DLR_BLENDMODE_BLEND:
			blendState = env->inner.d3d10.blendModeBlend;
			break;
		case DLR_BLENDMODE_NONE:
		default:
			blendState = NULL;
			break;
	}
	env->inner.d3d10.device->OMSetBlendState(blendState, 0, 0xffffffff);

	if (FAILED(hr)) {
		SDL_Log("d3d10 technique pass Apply() failed");
		exit(-1);
	}
	env->inner.d3d10.device->Draw(vertexCount, 0);
	if (FAILED(hr)) {
		SDL_Log("d3d10 device DrawAuto() failed");
		exit(-1);
	}
}
#endif

void DLRTest_Clear(
    DLRTest_Env * env,
    DLR_State * state,
    Uint32 color)
{
    switch (env->type) {
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

#if DLRTEST_D3D10
        case DLRTEST_TYPE_D3D10: {
            Uint8 a, r, g, b;
            DLR_SplitARGB32(color, a, r, g, b);
            float fColor[] = {
                r / 255.f,
                g / 255.f,
                b / 255.f,
                a / 255.f,
            };
            env->inner.d3d10.device->ClearRenderTargetView(env->inner.d3d10.renderTargetView, fColor);
        } break;
#endif
    }
}

void DLRTest_DrawTriangles(
    DLRTest_Env * env,
    DLR_State * state,
    DLR_Vertex * vertices,
    size_t vertexCount)
{
    switch (env->type) {
        case DLRTEST_TYPE_SOFTWARE:
            DLR_DrawTriangles(state, vertices, vertexCount);
            return;
        case DLRTEST_TYPE_OPENGL1:
            DLRTest_DrawTriangles_OpenGL1(env, state, vertices, vertexCount);
            return;
#if DLRTEST_D3D10
        case DLRTEST_TYPE_D3D10:
            DLRTest_DrawTriangles_D3D10(env, state, vertices, vertexCount);
            return;
#endif
    }
}

void DLRTest_DrawScene(DLRTest_Env * env)
{
    SDL_Surface * bg = (env->type == DLRTEST_TYPE_SOFTWARE) ? env->inner.sw.bg : NULL;

    {
        static DLR_State state;
        state.dest = bg;
        DLRTest_Clear(env, &state, 0xff000000);
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

        DLR_Float originX = 20;
        DLR_Float originY = 20;
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
        DLRTest_DrawTriangles(env, &state, vertices, SDL_arraysize(vertices));
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
                    SDL_FillRect(state.texture, NULL, 0xFFFFFFFF);
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

        DLR_Float originX = 200.51;
        DLR_Float originY = 20.;
        struct { DLR_Float a, r, g, b; } c[] = {
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
        DLRTest_DrawTriangles(env, &state, vertices, SDL_arraysize(vertices));
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

int main(int, char **) {
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
            "DLRTest",
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
                SDL_Renderer * r = SDL_CreateRenderer(envs[i].window, -1, 0);
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

#if DLRTEST_D3D10
            case DLRTEST_TYPE_D3D10: {
                HRESULT hr;

                SDL_SysWMinfo wmInfo;
                SDL_VERSION(&wmInfo.version);
                if ( ! SDL_GetWindowWMInfo(envs[i].window, &wmInfo)) {
                    SDL_Log("SDL_GetWindowWMInfo() failed: %s", SDL_GetError());
                    return -1;
                }

                DXGI_SWAP_CHAIN_DESC swapChainDesc;
                memset(&swapChainDesc, 0, sizeof(swapChainDesc));
                swapChainDesc.BufferCount = 1;
                swapChainDesc.BufferDesc.Width = winW;
                swapChainDesc.BufferDesc.Height = winH;
                swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
                swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
                swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
                swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
                swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                swapChainDesc.OutputWindow = wmInfo.info.win.window;
                swapChainDesc.SampleDesc.Count = 1;
                swapChainDesc.SampleDesc.Quality = 0;
                swapChainDesc.SwapEffect= DXGI_SWAP_EFFECT_DISCARD;
                swapChainDesc.Windowed = true;
                hr = D3D10CreateDeviceAndSwapChain(
                    NULL,
                    D3D10_DRIVER_TYPE_REFERENCE, //D3D10_DRIVER_TYPE_HARDWARE,
                    NULL,
					D3D10_CREATE_DEVICE_DEBUG,
                    D3D10_SDK_VERSION,
                    &swapChainDesc,
                    &envs[i].inner.d3d10.swapChain,
                    &envs[i].inner.d3d10.device
                );
                if (FAILED(hr)) {
                    SDL_Log("D3D10CreateDeviceAndSwapChain() failed");
                    return -1;
                }

                ID3D10Texture2D * backBuffer;
                hr = envs[i].inner.d3d10.swapChain->GetBuffer(0, __uuidof(ID3D10Texture2D), (void **)&backBuffer);
                if (FAILED(hr)) {
                    SDL_Log("swap chain GetBuffer() failed");
                    return -1;
                }

                hr = envs[i].inner.d3d10.device->CreateRenderTargetView(backBuffer, NULL, &envs[i].inner.d3d10.renderTargetView);
                if (FAILED(hr)) {
                    SDL_Log("d3d10 device CreateRenderTargetView() failed");
                    return -1;
                }

                backBuffer->Release();
                backBuffer = 0;

                envs[i].inner.d3d10.device->OMSetRenderTargets(1, &envs[i].inner.d3d10.renderTargetView, NULL);

                D3D10_RASTERIZER_DESC rasterDesc;
                memset(&rasterDesc, 0, sizeof(rasterDesc));
                rasterDesc.CullMode = D3D10_CULL_NONE;
                rasterDesc.FillMode = D3D10_FILL_SOLID;
                hr = envs[i].inner.d3d10.device->CreateRasterizerState(&rasterDesc, &envs[i].inner.d3d10.rasterState);
                if (FAILED(hr)) {
                    SDL_Log("d3d10 device CreateRasterizerState() failed");
                    return -1;
                }
                envs[i].inner.d3d10.device->RSSetState(envs[i].inner.d3d10.rasterState);

                D3D10_VIEWPORT viewport;
                memset(&viewport, 0, sizeof(viewport));
                viewport.Width = winW;
                viewport.Height = winH;
                viewport.MinDepth = 0.f;
                viewport.MaxDepth = 1.f;
                viewport.TopLeftX = 0;
                viewport.TopLeftY = 0;
                envs[i].inner.d3d10.device->RSSetViewports(1, &viewport);

				const char shaderSrc[] = R"DLRSTRING(

struct VertexShaderInput {
    float4 position : POSITION;
    float4 color : COLOR;
	float2 tex : TEXCOORD0;
};

struct PixelShaderInput {
	float4 position : SV_POSITION;
	float4 color : COLOR;
	float2 tex : TEXCOORD0;
};

matrix viewMatrix;
Texture2D theTexture;

SamplerState theSampler {
	Filter = MIN_MAG_MIP_POINT;
	AddressU = Clamp;
	AddressV = Clamp;
};

PixelShaderInput TheVertexShader(VertexShaderInput input) {
    PixelShaderInput output;
	input.position.w = 1.0f;
    output.position = mul(input.position, viewMatrix);
    output.color = input.color;
	output.tex = input.tex;
    return output;
}

float4 ColorPixelShader(PixelShaderInput input) : SV_Target {
    return input.color;
}

float4 TexturePixelShader(PixelShaderInput input) : SV_Target {
	return theTexture.Sample(theSampler, input.tex) * input.color;
}

technique10 ColorTechnique {
    pass pass0 {
        SetVertexShader(CompileShader(vs_4_0, TheVertexShader()));
        SetPixelShader(CompileShader(ps_4_0, ColorPixelShader()));
        SetGeometryShader(NULL);
    }
}

technique10 TextureTechnique {
    pass pass0 {
        SetVertexShader(CompileShader(vs_4_0, TheVertexShader()));
        SetPixelShader(CompileShader(ps_4_0, TexturePixelShader()));
        SetGeometryShader(NULL);
    }
}


)DLRSTRING";

				ID3D10Blob * compiledEffect = NULL;
				ID3D10Blob * errors = NULL;
				hr = D3D10CompileEffectFromMemory(
					(void *)shaderSrc,
					sizeof(shaderSrc),
					"D3D10_Shaders.fx",
					NULL,
					NULL,
					D3D10_SHADER_ENABLE_STRICTNESS,
					0,
					&compiledEffect,
					&errors);
				if (FAILED(hr)) {
					SDL_Log("D3D10CompileEffectFromMemory() failed");
					char * errorMessage = (char *) errors->GetBufferPointer();
					size_t errorMessageSize = errors->GetBufferSize();
					char buf[1024];
					snprintf(buf, sizeof(buf), "%.*s", errorMessageSize, errorMessage);
					SDL_Log("%s", buf);
					return -1;
				}

				hr = D3D10CreateEffectFromMemory(
					compiledEffect->GetBufferPointer(),
					compiledEffect->GetBufferSize(),
					0,
					envs[i].inner.d3d10.device,
					NULL,
					&envs[i].inner.d3d10.effect);
				if (FAILED(hr)) {
					SDL_Log("D3D10CreateEffectFromMemory() failed");
					return -1;
				}
				//compiledEffect->Release();

				envs[i].inner.d3d10.colorTechnique = envs[i].inner.d3d10.effect->GetTechniqueByName("ColorTechnique");
				if ( ! envs[i].inner.d3d10.colorTechnique) {
					SDL_Log("d3d 10 effect GetTechniqueByName(\"ColorTechnique\") failed");
					return -1;
				}

				envs[i].inner.d3d10.textureTechnique = envs[i].inner.d3d10.effect->GetTechniqueByName("TextureTechnique");
				if ( ! envs[i].inner.d3d10.textureTechnique) {
					SDL_Log("d3d 10 effect GetTechniqueByName(\"TextureTechnique\") failed");
					return -1;
				}

				envs[i].inner.d3d10.textureForShader = envs[i].inner.d3d10.effect->GetVariableByName("theTexture")->AsShaderResource();
				if ( ! envs[i].inner.d3d10.textureTechnique) {
					SDL_Log("d3d 10 effect GetVariableByName(\"theTexture\")->AsShaderResource() failed");
					return -1;
				}

				D3D10_INPUT_ELEMENT_DESC layout[3];
				layout[0].SemanticName = "POSITION";
				layout[0].SemanticIndex = 0;
				layout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
				layout[0].InputSlot = 0;
				layout[0].AlignedByteOffset = 0;
				layout[0].InputSlotClass = D3D10_INPUT_PER_VERTEX_DATA;
				layout[0].InstanceDataStepRate = 0;
				//
				layout[1].SemanticName = "COLOR";
				layout[1].SemanticIndex = 0;
				layout[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
				layout[1].InputSlot = 0;
				layout[1].AlignedByteOffset = D3D10_APPEND_ALIGNED_ELEMENT;
				layout[1].InputSlotClass = D3D10_INPUT_PER_VERTEX_DATA;
				layout[1].InstanceDataStepRate = 0;
				//
				layout[2].SemanticName = "TEXCOORD";
				layout[2].SemanticIndex = 0;
				layout[2].Format = DXGI_FORMAT_R32G32_FLOAT;
				layout[2].InputSlot = 0;
				layout[2].AlignedByteOffset = D3D10_APPEND_ALIGNED_ELEMENT;
				layout[2].InputSlotClass = D3D10_INPUT_PER_VERTEX_DATA;
				layout[2].InstanceDataStepRate = 0;

				D3D10_PASS_DESC passDesc;
				hr = envs[i].inner.d3d10.colorTechnique->GetPassByIndex(0)->GetDesc(&passDesc);
				if (FAILED(hr)) {
					SDL_Log("d3d 10 technique pass GetDesc() failed");
					return -1;
				}

				hr = envs[i].inner.d3d10.device->CreateInputLayout(
					layout,
					sizeof(layout) / sizeof(layout[0]),
					passDesc.pIAInputSignature,
					passDesc.IAInputSignatureSize,
					&envs[i].inner.d3d10.layout);
				if (FAILED(hr)) {
					SDL_Log("d3d 10 device CreateInputLayout() failed");
					return -1;
				}

				envs[i].inner.d3d10.viewMatrix = envs[i].inner.d3d10.effect->GetVariableByName("viewMatrix")->AsMatrix();
				if ( ! envs[i].inner.d3d10.viewMatrix) {
					SDL_Log("unable to find 'viewMatrix' global in shader");
					return -1;
				}

				D3D10_BLEND_DESC blendDesc;
				memset(&blendDesc, 0, sizeof(blendDesc));
				blendDesc.BlendOp = D3D10_BLEND_OP_ADD;
				blendDesc.BlendOpAlpha = D3D10_BLEND_OP_ADD;
				blendDesc.RenderTargetWriteMask[0] = D3D10_COLOR_WRITE_ENABLE_ALL;
				blendDesc.BlendEnable[0] = true;
				blendDesc.SrcBlend = D3D10_BLEND_SRC_ALPHA;
				blendDesc.DestBlend = D3D10_BLEND_INV_SRC_ALPHA;
				blendDesc.SrcBlendAlpha = D3D10_BLEND_ONE;
				blendDesc.DestBlendAlpha = D3D10_BLEND_INV_SRC_ALPHA;
				hr = envs[i].inner.d3d10.device->CreateBlendState(&blendDesc, &envs[i].inner.d3d10.blendModeBlend);
				if (FAILED(hr)) {
					SDL_Log("d3d10 device CreateBlendState() failed for DLR_BLENDMODE_BLEND");
					return -1;
				}
            } break;
#endif

            default:
                SDL_Log("unknown renderer type");
                return -1;
        }
    } // end of init loop
    DLRTest_UpdateWindowTitles();

    // Performance measurement stuff:
#ifdef DLRTEST_FPSTICKS
    const int numTicksToEachMeasurement = DLRTEST_FPSTICKS;
#else
    const int numTicksToEachMeasurement = 500;
#endif
    int numTicksToMeasurement = numTicksToEachMeasurement;
    Uint64 lastMeasurement = SDL_GetPerformanceCounter();

    // Render loop: process events, then draw.  Repeat until app is done.
    while (1) {
        numTicksToMeasurement--;
        if (numTicksToMeasurement == 0) {
            Uint64 now = SDL_GetPerformanceCounter();
            double dtInMS = (double)((now - lastMeasurement) * 1000) / SDL_GetPerformanceFrequency();
            double fps = (double)numTicksToEachMeasurement / (dtInMS / 1000.0);
            SDL_Log("%f ms for %d ticks ; %f FPS\n", dtInMS, numTicksToEachMeasurement, fps);
            lastMeasurement = now;
            numTicksToMeasurement = numTicksToEachMeasurement;
        }

        // Process events
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            DLR_Test_ProcessEvent(e);
        }

		// Create a view matrix for OpenGL and/or D3D10 use
		gbMat4 rotateX;
		gb_mat4_rotate(&rotateX, {1, 0, 0}, (float)M_PI);
		gbMat4 translateXY;
		gb_mat4_translate(&translateXY, {-1.f, -1.f, 0});
		gbMat4 scaleXY;
		gb_mat4_scale(&scaleXY, {2.f/(float)winW, 2.f/(float)winH, 1});
		gbMat4 finalM;
		gb_mat4_mul(&finalM, &rotateX, &translateXY);
		gb_mat4_mul(&finalM, &finalM, &scaleXY);

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
					glLoadMatrixf(finalM.e);
				} break;

#if DLRTEST_D3D10
				case DLRTEST_TYPE_D3D10: {
					envs[i].inner.d3d10.viewMatrix->SetMatrix(finalM.e);
				} break;
#endif
            }

            // Render Scene
            if ( ! (env->flags & DLRTEST_ENV_COMPARE)) {
                DLRTest_DrawScene(&envs[i]);
            } else {
                // Draw diff between scenes 0 and 1
                SDL_Surface * dest = DLRTest_GetSurfaceForView(&envs[i]);
                SDL_Surface * A = DLRTest_GetSurfaceForView(&envs[0]);
                SDL_Surface * B = DLRTest_GetSurfaceForView(&envs[1]);
                if (dest) {
                    for (int y = 0; y < dest->h; ++y) {
                        for (int x = 0; x < dest->w; ++x) {
							Uint32 dc = 0xffff0000;
							if (A && B) {
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

								dc = beyond ? 0xffffffff : 0x00000000;
							}
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

#if DLRTEST_D3D10
                case DLRTEST_TYPE_D3D10: {
                    HRESULT hr = envs[i].inner.d3d10.swapChain->Present(0, 0);   // present without vsymc
                    if (FAILED(hr)) {
                        SDL_Log("swap chain Present() failed");
                        exit(-1);
                    }
                } break;
#endif
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
