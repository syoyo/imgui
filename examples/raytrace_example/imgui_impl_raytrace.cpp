// ImGui SDL2 binding with NanoRTL

#include <SDL.h>
#include <SDL_syswm.h>

#include <imgui.h>
#include "imgui_impl_raytrace.h"

#include <cstdio>

#define NANORT_IMPLEMENTATION
#include "nanort.h"

// Data
static double       g_Time = 0.0f;
static bool         g_MousePressed[3] = { false, false, false };
static float        g_MouseWheel = 0.0f;
static std::vector<unsigned char> g_buffer;

namespace {

static inline unsigned char fclamp(float x) {
  int i = x * 255.5f;
  if (i < 0)
    return 0;
  if (i > 255)
    return 255;
  return (unsigned char)i;
}

union fi {
  float f;
  unsigned int i;
};

inline unsigned int mask(int x)
{
  return (1U << x) - 1;
}

// from fmath.hpp
/*
  for given y > 0
  get f_y(x) := pow(x, y) for x >= 0
*/

class PowGenerator {
  enum {
    N = 11
  };
  float tbl0_[256];
  struct {
    float app;
    float rev;
  } tbl1_[1 << N];
public:
  PowGenerator(float y)
  {
    for (int i = 0; i < 256; i++) {
      tbl0_[i] = ::powf(2, (i - 127) * y);
    }
    const double e = 1 / double(1 << 24);
    const double h = 1 / double(1 << N);
    const size_t n = 1U << N;
    for (size_t i = 0; i < n; i++) {
      double x = 1 + double(i) / n;
      double a = ::pow(x, (double)y);
      tbl1_[i].app = (float)a;
      double b = ::pow(x + h - e, (double)y);
      tbl1_[i].rev = (float)((b - a) / (h - e) / (1 << 23));
    }
  }
  float get(float x) const
  {
    fi fi;
    fi.f = x;
    int a = (fi.i >> 23) & mask(8);
    unsigned int b = fi.i & mask(23);
    unsigned int b1 = b & (mask(N) << (23 - N));
    unsigned int b2 = b & mask(23 - N);
    float f;
    int idx = b1 >> (23 - N);
    f = tbl0_[a] * (tbl1_[idx].app + float(b2) * tbl1_[idx].rev);
    return f;
  }
};


class Texture {

public:
  typedef enum {
    FORMAT_BYTE = 0,
    FORMAT_FLOAT = 1,
  } Format;

  Texture() : m_pow(1.0f) {
    // Make invalid texture
    m_width = -1;
    m_height = -1;
    m_image = NULL;
    m_components = -1;
  }

  Texture(unsigned char *image, int width, int height, int components,
          Format format, float gamma = 1.0f)
      : m_pow(gamma) {
    Set(image, width, height, components, format, gamma);
  }

  ~Texture() {}

  void Set(unsigned char *image, int width, int height, int components,
           Format format, float gamma = 1.0f) {
    m_width = width;
    m_height = height;
    m_image = image;
    m_invWidth = 1.0f / width;
    m_invHeight = 1.0f / height;
    m_components = components;
    m_format = format;
    m_gamma = gamma;
  }

  int width() const { return m_width; }

  int height() const { return m_height; }

  int components() const { return m_components; }

  unsigned char *image() const { return m_image; }

  int format() const { return m_format; }

  float gamma() const { return m_gamma; }

  void fetch(float *rgba, float u, float v) const;

  bool IsValid() const {
    return (m_image != NULL) && (m_width > 0) && (m_height > 0);
  }

private:
  int m_width;
  int m_height;
  float m_invWidth;
  float m_invHeight;
  int m_components;
  unsigned char *m_image;
  int m_format;
  float m_gamma;
  PowGenerator m_pow;
};

int inline fasterfloor(const float x) {
  if (x >= 0) {
    return (int)x;
  }

  int y = (int)x;
  if (std::abs(x - y) <= std::numeric_limits<float>::epsilon()) {
    // Do nothing.
  } else {
    y = y - 1;
  }

  return y;
}

inline float lerp(float x, float y, float t)
{
  return x + t * (y - x);
}

bool myisnan(float a) {
  volatile float d = a;
  return d != d;
}

inline void FilterByteLerp(float *rgba, const unsigned char *image, int i00,
                       int i10, int i01, int i11,
                       float dx, float dy,
                       int stride, const PowGenerator &p) {
  float texel[4][4];

  const float inv = 1.0f / 255.0f;
  if (stride == 4) {

    // Assume color is already degamma'ed.
    for (int i = 0; i < 4; i++) {
      texel[0][i] = (float)image[i00 + i] * inv;
      texel[1][i] = (float)image[i10 + i] * inv;
      texel[2][i] = (float)image[i01 + i] * inv;
      texel[3][i] = (float)image[i11 + i] * inv;
    }

    for (int i = 0; i < 4; i++) {
      rgba[i] = lerp(lerp(texel[0][i], texel[1][i], dx), lerp(texel[2][i], texel[3][i], dx), dy);
    }

  } else {

    for (int i = 0; i < stride; i++) {
      texel[0][i] = (float)image[i00 + i] * inv;
      texel[1][i] = (float)image[i10 + i] * inv;
      texel[2][i] = (float)image[i01 + i] * inv;
      texel[3][i] = (float)image[i11 + i] * inv;
    }

    for (int i = 0; i < stride; i++) {
      rgba[i] = texel[0][i]; // NEAREST
      //lerp(lerp(texel[0][i], texel[1][i], dx), lerp(texel[2][i], texel[3][i], dx), dy);
    }
  }

  if (stride < 4) {
    for (int i = stride; i < 4; i++) {
        rgba[i] = rgba[stride-1];
    }
    //rgba[3] = 1.0;
  }
}

inline void FilterFloatLerp(float *rgba, const float *image, int i00, int i10,
                        int i01, int i11,
                        float dx, float dy,
                        int stride, const PowGenerator &p) {
  float texel[4][4];

  if (stride == 4) {

    for (int i = 0; i < 4; i++) {
      texel[0][i] = image[i00 + i];
      texel[1][i] = image[i10 + i];
      texel[2][i] = image[i01 + i];
      texel[3][i] = image[i11 + i];
    }

    for (int i = 0; i < 4; i++) {
      rgba[i] = lerp(lerp(texel[0][i], texel[1][i], dx), lerp(texel[2][i], texel[3][i], dx), dy);
    }

  } else {

    for (int i = 0; i < stride; i++) {
      texel[0][i] = image[i00 + i];
      texel[1][i] = image[i10 + i];
      texel[2][i] = image[i01 + i];
      texel[3][i] = image[i11 + i]; // alpha is linear
    }

    for (int i = 0; i < stride; i++) {
      rgba[i] = lerp(lerp(texel[0][i], texel[1][i], dx), lerp(texel[2][i], texel[3][i], dx), dy);
    }
  }

  if (stride < 4) {
    rgba[3] = 1.0;
  }
}

void Texture::fetch(float *rgba, float u, float v) const {

  if (!IsValid()) {
    if (rgba) {
      rgba[0] = 0.0f;
      rgba[1] = 0.0f;
      rgba[2] = 0.0f;
      rgba[3] = 1.0f;
    }
    return;
  }

  float sx = fasterfloor(u);
  float sy = fasterfloor(v);

  float uu = u - sx;
  float vv = v - sy;

  // clamp
  uu = std::max(uu, 0.0f);
  uu = std::min(uu, 1.0f);
  vv = std::max(vv, 0.0f);
  vv = std::min(vv, 1.0f);

  float px = m_width * uu;
  float py = m_height * vv;

  int x0 = (int)px;
  int y0 = (int)py;
  int x1 = ((x0 + 1) >= m_width) ? (m_width - 1) : (x0 + 1);
  int y1 = ((y0 + 1) >= m_height) ? (m_height - 1) : (y0 + 1);

  float dx = px - (float)x0;
  float dy = py - (float)y0;

  float w[4];

  w[0] = (1.0f - dx) * (1.0f - dy);
  w[1] = (1.0f - dx) * (dy);
  w[2] = (dx) * (1.0f - dy);
  w[3] = (dx) * (dy);

  int stride = m_components;

  int i00 = stride * (y0 * m_width + x0);
  int i01 = stride * (y0 * m_width + x1);
  int i10 = stride * (y1 * m_width + x0);
  int i11 = stride * (y1 * m_width + x1);

  if (m_format == FORMAT_BYTE) {
    FilterByteLerp(rgba, m_image, i00, i01, i10, i11, dx, dy,
                stride, m_pow);
  } else if (m_format == FORMAT_FLOAT) {
    FilterFloatLerp(rgba, reinterpret_cast<float *>(m_image), i00, i01, i10, i11, dx, dy,
                stride, m_pow);
  } else { // unknown
  }
}

bool TraceAndShade(
    float RGBA[4],
    int px, int py,
    int width, int height,
    nanort::Ray& ray,
    nanort::BVHAccel& accel,
    const Texture& texture,
    const std::vector<float>& vertices,
    const std::vector<unsigned int>& faces,
    const std::vector<float>& colors,
    const std::vector<float>& texcoords,
    int depth)
{

    if (depth > 8) {
        printf("???\n");
        return false;
    }

    nanort::Intersection isect;
    isect.t = 1.0e+30f;

    bool hit = accel.Traverse(isect, &vertices.at(0), &faces.at(0), ray);
    if (hit) {
        float texcoord[3][2];
        float vtxcol[3][4];
        for (int fi = 0; fi < 3; fi++) {
            texcoord[fi][0] = texcoords[2 * (3 * isect.faceID + fi) + 0];
            texcoord[fi][1] = texcoords[2 * (3 * isect.faceID + fi) + 1];
            vtxcol[fi][0] = colors[4 * (3 * isect.faceID + fi) + 0];
            vtxcol[fi][1] = colors[4 * (3 * isect.faceID + fi) + 1];
            vtxcol[fi][2] = colors[4 * (3 * isect.faceID + fi) + 2];
            vtxcol[fi][3] = colors[4 * (3 * isect.faceID + fi) + 3];
        }

        float ts = (1.0f - isect.u - isect.v) * texcoord[0][0] + isect.u * texcoord[1][0] + isect.v * texcoord[2][0];
        float tt = (1.0f - isect.u - isect.v) * texcoord[0][1] + isect.u * texcoord[1][1] + isect.v * texcoord[2][1];
        float vr = (1.0f - isect.u - isect.v) * vtxcol[0][0] + isect.u * vtxcol[1][0] + isect.v * vtxcol[2][0];
        float vg = (1.0f - isect.u - isect.v) * vtxcol[0][1] + isect.u * vtxcol[1][1] + isect.v * vtxcol[2][1];
        float vb = (1.0f - isect.u - isect.v) * vtxcol[0][2] + isect.u * vtxcol[1][2] + isect.v * vtxcol[2][2];
        float va = (1.0f - isect.u - isect.v) * vtxcol[0][3] + isect.u * vtxcol[1][3] + isect.v * vtxcol[2][3];

        float texRGBA[4];
        texture.fetch(texRGBA, ts,  tt);

        va *= texRGBA[3];

        if (texRGBA[3] < (1.0f-1.0e-5f)) {
            // Shoot transparent rays.
            nanort::Ray transRay;
            transRay.org[0] = ray.org[0] + isect.t * ray.dir[0];
            transRay.org[1] = ray.org[1] + isect.t * ray.dir[1];
            transRay.org[2] = ray.org[2] + isect.t * ray.dir[2];

            transRay.dir[0] = ray.dir[0];
            transRay.dir[1] = ray.dir[1];
            transRay.dir[2] = ray.dir[2];

            transRay.org[0] += 0.01 * transRay.dir[0];
            transRay.org[1] += 0.01 * transRay.dir[1];
            transRay.org[2] += 0.01 * transRay.dir[2];

            float transRGBA[4];
            bool transHit = TraceAndShade(
                transRGBA,
                px, py, width, height,
                transRay,
                accel,
                texture, vertices, faces, colors, texcoords, depth+1);
            if (transHit) {
                // @fixme {}
                RGBA[0] = transRGBA[0];
                RGBA[1] = transRGBA[1];
                RGBA[2] = transRGBA[2];
                RGBA[3] = transRGBA[3];
            } else {
                RGBA[0] = texRGBA[0] * vr;
                RGBA[1] = texRGBA[1] * vg;
                RGBA[2] = texRGBA[2] * vb;
                RGBA[3] = va;
            }
        } else {
            RGBA[0] = texRGBA[0] * vr;
            RGBA[1] = texRGBA[1] * vg;
            RGBA[2] = texRGBA[2] * vb;
            RGBA[3] = va;
        }
    }

    return hit;
}

} // namespace

static Texture g_FontTexture;
std::vector<unsigned char> g_TextureMem;


void ImGui_ImplRt_RenderDrawLists(ImDrawData* draw_data)
{
    const float width = ImGui::GetIO().DisplaySize.x;
    const float height = ImGui::GetIO().DisplaySize.y;

    g_buffer.resize(width * height * 4);
    for (int i = 0; i < width * height; i++) {
        g_buffer[4 * i + 0] = 0.0f;
        g_buffer[4 * i + 1] = 0.0f;
        g_buffer[4 * i + 2] = 0.0f;
        g_buffer[4 * i + 3] = 1.0f;
    }

    int maxLayers = 0;

    // Render command lists
    #define OFFSETOF(TYPE, ELEMENT) ((size_t)&(((TYPE *)0)->ELEMENT))
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {

        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        const unsigned char* vtx_buffer = (const unsigned char*)&cmd_list->VtxBuffer.front();
        const ImDrawIdx* idx_buffer = &cmd_list->IdxBuffer.front();
        
        int numVtx = cmd_list->VtxBuffer.size();
        int numIdx = cmd_list->IdxBuffer.size();

        int index_offset = 0;
        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.size(); cmd_i++)
        {
            std::vector<float>          vertices;  // XYZ
            std::vector<unsigned int>   faces;     // 3 * numFaces(triangle)
            std::vector<float>          texcoords; // facevarying(2 * 3 * numFaces)
            std::vector<float>          colors;    // facevarying(4 * 3 * numFaces)

            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];

            int numElems = (int)pcmd->ElemCount;

            if (pcmd->UserCallback)
            {
                pcmd->UserCallback(cmd_list, pcmd);
            }
            else
            {

                int faceCount = 0;
                float maxDepth = numElems / 3.0;
                for (int i = 0; i < numElems; i += 3) {
                    unsigned int idx0 = cmd_list->IdxBuffer[index_offset+i+0];
                    unsigned int idx1 = cmd_list->IdxBuffer[index_offset+i+1];
                    unsigned int idx2 = cmd_list->IdxBuffer[index_offset+i+2];

                    // Create facevarying attributes
                    float x0 = cmd_list->VtxBuffer[idx0].pos.x;
                    float y0 = cmd_list->VtxBuffer[idx0].pos.y;
                    float x1 = cmd_list->VtxBuffer[idx1].pos.x;
                    float y1 = cmd_list->VtxBuffer[idx1].pos.y;
                    float x2 = cmd_list->VtxBuffer[idx2].pos.x;
                    float y2 = cmd_list->VtxBuffer[idx2].pos.y;

                    const ImDrawVert& v0 = cmd_list->VtxBuffer[idx0];
                    const ImDrawVert& v1 = cmd_list->VtxBuffer[idx1];
                    const ImDrawVert& v2 = cmd_list->VtxBuffer[idx2];

                    // Adoid co-planar triangle by adding z-value.
                    // Assume primitives are sorted in back-to-front
                    vertices.push_back(x0);
                    vertices.push_back(y0);
                    vertices.push_back(i/3.0);
                    vertices.push_back(x1);
                    vertices.push_back(y1);
                    vertices.push_back(i/3.0);
                    vertices.push_back(x2);
                    vertices.push_back(y2);
                    vertices.push_back(i/3.0);

                    texcoords.push_back(v0.uv.x);
                    texcoords.push_back(v0.uv.y);
                    texcoords.push_back(v1.uv.x);
                    texcoords.push_back(v1.uv.y);
                    texcoords.push_back(v2.uv.x);
                    texcoords.push_back(v2.uv.y);

                    ImVec4 rgba0 = ImGui::ColorConvertU32ToFloat4(v0.col);
                    ImVec4 rgba1 = ImGui::ColorConvertU32ToFloat4(v1.col);
                    ImVec4 rgba2 = ImGui::ColorConvertU32ToFloat4(v2.col);

                    colors.push_back(rgba0.x);
                    colors.push_back(rgba0.y);
                    colors.push_back(rgba0.z);
                    colors.push_back(rgba0.w);
                    colors.push_back(rgba1.x);
                    colors.push_back(rgba1.y);
                    colors.push_back(rgba1.z);
                    colors.push_back(rgba1.w);
                    colors.push_back(rgba2.x);
                    colors.push_back(rgba2.y);
                    colors.push_back(rgba2.z);
                    colors.push_back(rgba2.w);

                    faces.push_back(3 * faceCount + 0);
                    faces.push_back(3 * faceCount + 1);
                    faces.push_back(3 * faceCount + 2);
                    faceCount++;
                }

                nanort::BVHBuildOptions options;
                nanort::BVHAccel accel;
                bool ret = accel.Build(&vertices.at(0), &faces.at(0), faces.size() / 3, options);
                assert(ret);

                nanort::BVHBuildStatistics stats = accel.GetStatistics();

                //printf("  BVH statistics:\n");
                //printf("    # of leaf   nodes: %d\n", stats.numLeafNodes);
                //printf("    # of branch nodes: %d\n", stats.numBranchNodes);
                //printf("  Max tree depth     : %d\n", stats.maxTreeDepth);
                //printf("  Scene eps          : %f\n", stats.epsScale);
                //float bmin[3], bmax[3];
                //accel.BoundingBox(bmin, bmax);
                //printf("  Bmin               : %f, %f, %f\n", bmin[0], bmin[1], bmin[2]);
                //printf("  Bmax               : %f, %f, %f\n", bmax[0], bmax[1], bmax[2]);

                float tFar = 1.0e+30f;

                // Shoot rays.
                #ifdef _OPENMP
                #pragma omp parallel for
                #endif
                for (int y = 0; y < height; y++) {
                  for (int x = 0; x < width; x++) {
                    nanort::Intersection isect;
                    isect.t = tFar;

                    nanort::Ray ray;
                    ray.org[0] = x+0.5f;
                    ray.org[1] = y+0.5f;
                    ray.org[2] = maxDepth + 1.0f;

                    nanort::float3 dir;
                    dir[0] = 0.0f;
                    dir[1] = 0.0f;
                    dir[2] = -1.0f;

                    ray.dir[0] = dir[0];
                    ray.dir[1] = dir[1];
                    ray.dir[2] = dir[2];

                    float RGBA[4];
                    bool hit = TraceAndShade(RGBA, x, y, width, height, ray, accel,
                                g_FontTexture, vertices, faces, colors, texcoords, 0);

                    if (hit) {
                        // Mimics glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                        float sfactor = RGBA[3];
                        float dfactor = 1.0f - RGBA[3];
                        g_buffer[4*(y*width+x)+0] = fclamp(RGBA[0] * sfactor + dfactor*g_buffer[4*(y*width+x)+0]/255.5f);
                        g_buffer[4*(y*width+x)+1] = fclamp(RGBA[1] * sfactor + dfactor*g_buffer[4*(y*width+x)+1]/255.5f);
                        g_buffer[4*(y*width+x)+2] = fclamp(RGBA[2] * sfactor + dfactor*g_buffer[4*(y*width+x)+2]/255.5f);
                        g_buffer[4*(y*width+x)+3] = fclamp(RGBA[3] * sfactor + dfactor*g_buffer[4*(y*width+x)+3]/255.5f);
                    }
                  }
                }

            }

            idx_buffer += pcmd->ElemCount;
            index_offset += pcmd->ElemCount;
        }


    }
    #undef OFFSETOF

}

static const char* ImGui_ImplRt_GetClipboardText()
{
	return SDL_GetClipboardText();
}

static void ImGui_ImplRt_SetClipboardText(const char* text)
{
    SDL_SetClipboardText(text);
}

bool ImGui_ImplRt_ProcessEvent(SDL_Event* event)
{
    ImGuiIO& io = ImGui::GetIO();
    switch (event->type)
    {
    case SDL_MOUSEMOTION:
        {
            return true;
        }
    case SDL_MOUSEWHEEL:
        {
            if (event->wheel.y > 0)
                g_MouseWheel = 1;
            if (event->wheel.y < 0)
                g_MouseWheel = -1;
            return true;
        }
    case SDL_MOUSEBUTTONDOWN:
        {
            if (event->button.button == SDL_BUTTON_LEFT) g_MousePressed[0] = true;
            if (event->button.button == SDL_BUTTON_RIGHT) g_MousePressed[1] = true;
            if (event->button.button == SDL_BUTTON_MIDDLE) g_MousePressed[2] = true;
            return true;
        }
    case SDL_TEXTINPUT:
        {
            ImGuiIO& io = ImGui::GetIO();
            io.AddInputCharactersUTF8(event->text.text);
            return true;
        }
    case SDL_KEYDOWN:
        {
            if (event->key.keysym.sym == SDLK_ESCAPE ||
                event->key.keysym.sym == 'q') {
                event->type = SDL_QUIT;
            }
            return true;
        }
    case SDL_KEYUP:
        {
            int key = event->key.keysym.sym & ~SDLK_SCANCODE_MASK;
            io.KeysDown[key] = (event->type == SDL_KEYDOWN);
            io.KeyShift = ((SDL_GetModState() & KMOD_SHIFT) != 0);
            io.KeyCtrl = ((SDL_GetModState() & KMOD_CTRL) != 0);
            io.KeyAlt = ((SDL_GetModState() & KMOD_ALT) != 0);
            return true;
        }
    }
    return false;
}

bool ImGui_ImplRt_CreateDeviceObjects()
{
    ImGuiIO& io = ImGui::GetIO();

    // Build texture
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);

    g_TextureMem.resize(width * height);
    std::copy(pixels, pixels+width*height, g_TextureMem.begin());
    g_FontTexture.Set(&g_TextureMem.at(0), width, height, 1, Texture::FORMAT_BYTE, 1.0f);

    // Cleanup (don't clear the input data if you want to append new fonts later)
	io.Fonts->ClearInputData();
	io.Fonts->ClearTexData();

    return true;
}

void    ImGui_ImplRt_InvalidateDeviceObjects()
{
    //if (g_FontTexture)
    //{
    //    //glDeleteTextures(1, &g_FontTexture);
    //    ImGui::GetIO().Fonts->TexID = 0;
    //    g_FontTexture = 0;
    //}
}

bool    ImGui_ImplRt_Init(SDL_Window* window)
{
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.AntiAliasedLines = false;
    style.AntiAliasedShapes = false;
    io.KeyMap[ImGuiKey_Tab] = SDLK_TAB;                     // Keyboard mapping. ImGui will use those indices to peek into the io.KeyDown[] array.
    io.KeyMap[ImGuiKey_LeftArrow] = SDL_SCANCODE_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = SDL_SCANCODE_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow] = SDL_SCANCODE_UP;
    io.KeyMap[ImGuiKey_DownArrow] = SDL_SCANCODE_DOWN;
    io.KeyMap[ImGuiKey_PageUp] = SDL_SCANCODE_PAGEUP;
    io.KeyMap[ImGuiKey_PageDown] = SDL_SCANCODE_PAGEDOWN;
    io.KeyMap[ImGuiKey_Home] = SDL_SCANCODE_HOME;
    io.KeyMap[ImGuiKey_End] = SDL_SCANCODE_END;
    io.KeyMap[ImGuiKey_Delete] = SDLK_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = SDLK_BACKSPACE;
    io.KeyMap[ImGuiKey_Enter] = SDLK_RETURN;
    io.KeyMap[ImGuiKey_Escape] = SDLK_ESCAPE;
    io.KeyMap[ImGuiKey_A] = SDLK_a;
    io.KeyMap[ImGuiKey_C] = SDLK_c;
    io.KeyMap[ImGuiKey_V] = SDLK_v;
    io.KeyMap[ImGuiKey_X] = SDLK_x;
    io.KeyMap[ImGuiKey_Y] = SDLK_y;
    io.KeyMap[ImGuiKey_Z] = SDLK_z;
	
    io.SetClipboardTextFn = ImGui_ImplRt_SetClipboardText;
    io.GetClipboardTextFn = ImGui_ImplRt_GetClipboardText;
	
#ifdef _WIN32
	SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo(window, &wmInfo);
    io.ImeWindowHandle = wmInfo.info.win.window;
#endif
    io.RenderDrawListsFn = ImGui_ImplRt_RenderDrawLists;   // Alternatively you can set this to NULL and call ImGui::GetDrawData() after ImGui::Render() to get the same ImDrawData pointer.

    return true;
}

void ImGui_ImplRt_Shutdown()
{
    ImGui_ImplRt_InvalidateDeviceObjects();
    ImGui::Shutdown();
}

void ImGui_ImplRt_NewFrame(SDL_Window* window)
{
    if (!g_FontTexture.IsValid())
        ImGui_ImplRt_CreateDeviceObjects();

    ImGuiIO& io = ImGui::GetIO();

    // Setup display size (every frame to accommodate for window resizing)
    int w, h;
	SDL_GetWindowSize(window, &w, &h);
    g_buffer.resize(w * h * 3);
    io.DisplaySize = ImVec2((float)w, (float)h);

    // Setup time step
	Uint32	time = SDL_GetTicks();
	double current_time = time / 1000.0;
    io.DeltaTime = g_Time > 0.0 ? (float)(current_time - g_Time) : (float)(1.0f/60.0f);
    g_Time = current_time;

    // Setup inputs
    // (we already got mouse wheel, keyboard keys & characters from glfw callbacks polled in glfwPollEvents())
    int mx, my;
    Uint32 mouseMask = SDL_GetMouseState(&mx, &my);
    if (SDL_GetWindowFlags(window) & SDL_WINDOW_MOUSE_FOCUS)
    	io.MousePos = ImVec2((float)mx, (float)my);   // Mouse position, in pixels (set to -1,-1 if no mouse / on another screen, etc.)
    else
    	io.MousePos = ImVec2(-1,-1);
   
	io.MouseDown[0] = g_MousePressed[0] || (mouseMask & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;		// If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are shorter than 1 frame.
	io.MouseDown[1] = g_MousePressed[1] || (mouseMask & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
	io.MouseDown[2] = g_MousePressed[2] || (mouseMask & SDL_BUTTON(SDL_BUTTON_MIDDLE)) != 0;
    g_MousePressed[0] = g_MousePressed[1] = g_MousePressed[2] = false;

    io.MouseWheel = g_MouseWheel;
    g_MouseWheel = 0.0f;

    // Hide OS mouse cursor if ImGui is drawing it
    SDL_ShowCursor(io.MouseDrawCursor ? 0 : 1);

    // Start the frame
    ImGui::NewFrame();
}

void ImGui_ImplRt_GetImage(unsigned char* dst_image)
{
    memcpy(dst_image, &g_buffer.at(0), g_buffer.size());
}
