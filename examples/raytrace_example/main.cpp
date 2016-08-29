// Must be included befor `imgui_impl_raytrace.h`
#include "X11SWWindow.h"

#include "imgui_impl_raytrace.h"
#include <imgui.h>
//#include "../../imgui_demo.h"
#include <stdio.h>
#include <vector>

//#include <atomic>
#include <thread> // C++11

int gWidth = 512;
int gHeight = 512;
int gMousePosX = -1, gMousePosY = -1;
bool gMouseLeftDown = false;
bool gTabPressed = false;
bool gShiftPressed = false;
float gShowPositionScale = 1.0f;
float gShowDepthRange[2] = {10.0f, 20.f};
bool gShowDepthPeseudoColor = true;
float gCurrQuat[4] = {0.0f, 0.0f, 0.0f, 1.0f};
float gPrevQuat[4] = {0.0f, 0.0f, 0.0f, 1.0f};

float gBackgroundColor[3] = {0.0f, 0.0f, 0.25f};

inline unsigned char iclamp(int x) {
  if (x > 255)
    x = 255;
  if (x < 0)
    x = 0;
  return (unsigned char)x;
}

inline float fclamp(float x) {
  int i = x * 255.0f;
  if (i > 255)
    i = 255;
  if (i < 0)
    i = 0;
  return (unsigned char)i;
}

float blend(float src, float dst, float alpha) {
  return alpha * src + (1.0f - alpha) * dst;
}

void Display(unsigned char *dst, unsigned char *rgba, int width, int height) {
  // @todo { gamma correction. }
  // BGRA?
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      float alpha = rgba[4 * (y * width + x) + 3] / 255.0f;
      dst[4 * (y * width + x) + 0] =
          fclamp(blend(rgba[4 * (y * width + x) + 2] / 255.0f,
                       dst[4 * (y * width + x) + 0] / 255.0f, alpha));
      dst[4 * (y * width + x) + 1] =
          fclamp(blend(rgba[4 * (y * width + x) + 1] / 255.0f,
                       dst[4 * (y * width + x) + 1] / 255.0f, alpha));
      dst[4 * (y * width + x) + 2] =
          fclamp(blend(rgba[4 * (y * width + x) + 0] / 255.0f,
                       dst[4 * (y * width + x) + 2] / 255.0f, alpha));
      dst[4 * (y * width + x) + 3] = 255;
    }
  }
}

#if 0
void DrawGUI(SDL_Window* window)
{
    static bool show_test_window = true;
    static bool show_another_window = false;
    static ImVec4 clear_color = ImColor(114, 144, 154);

    ImGui_ImplRt_NewFrame(window);

    // 1. Show a simple window
    // Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appears in a window automatically called "Debug"
    {
        static float f = 0.0f;
        ImGui::Text("Hello, world!");
        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
        ImGui::ColorEdit3("clear color", (float*)&clear_color);
        if (ImGui::Button("Test Window")) show_test_window ^= 1;
        if (ImGui::Button("Another Window")) show_another_window ^= 1;
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    }

    // 2. Show another simple window, this time using an explicit Begin/End pair
    if (show_another_window)
    {
        ImGui::SetNextWindowSize(ImVec2(200,100), ImGuiSetCond_FirstUseEver);
        ImGui::Begin("Another Window", &show_another_window);
        ImGui::Text("Hello");
        ImGui::End();
    }

    // 3. Show the ImGui test window. Most of the sample code is in ImGui::ShowTestWindow()
    if (show_test_window)
    {
        ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiSetCond_FirstUseEver);
        ImGui::ShowTestWindow(&show_test_window);
    }
    ImGui::Render();
}
#endif

b3gDefaultWindow *window;

void keyboardCallback(int keycode, int state) {
  printf("hello key %d, state %d(ctrl %d)\n", keycode, state,
         window->isModifierKeyPressed(B3G_CONTROL));
  if (keycode == 27) {
    if (window)
      window->setRequestExit();
  } else if (keycode == ' ') {
    // trackball(gCurrQuat, 0.0f, 0.0f, 0.0f, 0.0f);
  } else if (keycode == 9) {
    gTabPressed = (state == 1);
  } else if (keycode == B3G_SHIFT) {
    gShiftPressed = (state == 1);
  }

  ImGui_ImplRt_SetKeyState(keycode, (state == 1));

  if (keycode >= 32 && keycode <= 126) {
    if (state == 1) {
      ImGui_ImplRt_SetChar(keycode);
    }
  }
}

void mouseMoveCallback(float x, float y) {

  if (gMouseLeftDown) {
    float w = gWidth;

    float y_offset = gHeight;

    if (gTabPressed) {
      const float dolly_scale = 0.1;
      // gRenderConfig.eye[2] += dolly_scale * (gMousePosY - y);
      // gRenderConfig.look_at[2] += dolly_scale * (gMousePosY - y);
    } else if (gShiftPressed) {
      const float trans_scale = 0.02;
      // gRenderConfig.eye[0] += trans_scale * (gMousePosX - x);
      // gRenderConfig.eye[1] -= trans_scale * (gMousePosY - y);
      // gRenderConfig.look_at[0] += trans_scale * (gMousePosX - x);
      // gRenderConfig.look_at[1] -= trans_scale * (gMousePosY - y);

    } else {
      // Adjust y.
      // trackball(gPrevQuat, (2.f * gMousePosX - w) / (float)w,
      //          (h - 2.f * (gMousePosY - y_offset)) / (float)h, (2.f * x - w)
      //          / (float)w,
      //          (h - 2.f * (y - y_offset)) / (float)h);
      // add_quats(gPrevQuat, gCurrQuat, gCurrQuat);
    }
    // RequestRender();
  }

  gMousePosX = (int)x;
  gMousePosY = (int)y;
}

void mouseButtonCallback(int button, int state, float x, float y) {
  ImGui_ImplRt_SetMouseButtonState(button, (state == 1));

  ImGuiIO &io = ImGui::GetIO();
  if (io.WantCaptureMouse || io.WantCaptureKeyboard) {
    return;
  }

  // left button
  if (button == 0) {
    if (state) {
      gMouseLeftDown = true;
      // trackball(gPrevQuat, 0.0f, 0.0f, 0.0f, 0.0f);
    } else
      gMouseLeftDown = false;
  }
}

void resizeCallback(float width, float height) {
  gWidth = width;
  gHeight = height;
}

static void ClearFramebuffer(unsigned char *dst, int w, int h) {
  float max_intensity = 0.3;
  for (size_t y = 0; y < h; y++) {
    float a = static_cast<float>(y) / static_cast<float>(h);
    float red = a * gBackgroundColor[0];
    float green = a * gBackgroundColor[1];
    float blue = a * gBackgroundColor[2];
    for (size_t x = 0; x < w; x++) {
      dst[4 * (y * w + x) + 0] = fclamp(blue);
      dst[4 * (y * w + x) + 1] = fclamp(green);
      dst[4 * (y * w + x) + 2] = fclamp(red);
      dst[4 * (y * w + x) + 3] = 0;
    }
  }
}

int main(int, char **) {
  window = new b3gDefaultWindow;
  b3gWindowConstructionInfo ci;
  ci.m_width = 800;
  ci.m_height = 600;
  window->createWindow(ci);

  window->setWindowTitle("view");

  window->setMouseButtonCallback(mouseButtonCallback);
  window->setMouseMoveCallback(mouseMoveCallback);
  window->setKeyboardCallback(keyboardCallback);
  window->setResizeCallback(resizeCallback);

  ImGui_ImplRt_Init(window);

  ImGuiIO &io = ImGui::GetIO();
  io.Fonts->AddFontDefault();

  std::vector<unsigned char> framebuffer(ci.m_width * ci.m_height * 4); // RGBA
  ClearFramebuffer(framebuffer.data(), ci.m_width, ci.m_height);

  while (!window->requestedExit()) {
    window->startRendering();

    ImGui_ImplRt_NewFrame(gMousePosX, gMousePosY);
    ImGui::Begin("UI");
    {
      static float f = 0.0f;
      static float b = 0.0f;
      if (ImGui::ColorEdit3("color", gBackgroundColor)) {
        // RequestRender();
      }
      ImGui::InputFloat("intensity", &f);
      ImGui::SliderFloat("bora", &b, 0.0f, 1.0f);
#if 0
      if (ImGui::InputFloat3("eye", gRenderConfig.eye)) {
        RequestRender();
      }
      if (ImGui::InputFloat3("up", gRenderConfig.up)) {
        RequestRender();
      }
      if (ImGui::InputFloat3("look_at", gRenderConfig.look_at)) {
        RequestRender();
      }

      ImGui::RadioButton("color", &gShowBufferMode, SHOW_BUFFER_COLOR);
      ImGui::SameLine();
      ImGui::RadioButton("normal", &gShowBufferMode, SHOW_BUFFER_NORMAL);
      ImGui::SameLine();
      ImGui::RadioButton("position", &gShowBufferMode, SHOW_BUFFER_POSITION);
      ImGui::SameLine();
      ImGui::RadioButton("depth", &gShowBufferMode, SHOW_BUFFER_DEPTH);
      ImGui::SameLine();
      ImGui::RadioButton("texcoord", &gShowBufferMode, SHOW_BUFFER_TEXCOORD);
      ImGui::SameLine();
      ImGui::RadioButton("varycoord", &gShowBufferMode, SHOW_BUFFER_VARYCOORD);

      ImGui::InputFloat("show pos scale", &gShowPositionScale);

      ImGui::InputFloat2("show depth range", gShowDepthRange);
      ImGui::Checkbox("show depth pesudo color", &gShowDepthPeseudoColor);
#endif
    }

    ImGui::End();

    ImGui::Render();

    // Composite
    {
      std::vector<unsigned char> image(window->getWidth() *
                                       window->getHeight() * 4);
      ImGui_ImplRt_GetImage(&image.at(0));

      assert(ci.m_width == window->getWidth());
      assert(ci.m_height == window->getHeight());
      ClearFramebuffer(framebuffer.data(), ci.m_width, ci.m_height);
      Display(framebuffer.data(), image.data(), window->getWidth(),
              window->getHeight());

      window->updateImage(framebuffer.data(), window->getWidth(),
                          window->getHeight());
    }

    window->endRendering();

    // Give some cycles to this thread.
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }

  ImGui_ImplRt_Shutdown();
  window->closeWindow();
  delete window;

  return EXIT_SUCCESS;
}
