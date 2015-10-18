// ImGui - standalone example application for SDL2 + NanoRT

#include <imgui.h>
#include "imgui_impl_raytrace.h"
#include <vector>
#include <stdio.h>
#include <SDL.h>

inline unsigned char iclamp(int x)
{
    if (x > 255) x = 255;
    if (x < 0) x = 0;
    return (unsigned char)x;
}

inline float fclamp(float x)
{
    int i = x * 255.0f;
    if (i > 255) i = 255;
    if (i < 0) i = 0;
    return (unsigned char)i;
}

float blend(float src, float dst, float alpha)
{
    return alpha * src + (1.0f-alpha) * dst;
}

void Display(SDL_Surface* surface, unsigned char* rgba, int width, int height)
{
    SDL_LockSurface(surface);

    // BGRA?
    unsigned char *data = (unsigned char *)surface->pixels;
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        float alpha = rgba[4*(y*width+x)+3] / 255.0f;
        data[4*(y*width+x)+0] = fclamp(blend(rgba[4*(y*width+x)+2]/255.0f, data[4*(y*width+x)+0]/255.0f, alpha));
        data[4*(y*width+x)+1] = fclamp(blend(rgba[4*(y*width+x)+1]/255.0f, data[4*(y*width+x)+1]/255.0f, alpha));
        data[4*(y*width+x)+2] = fclamp(blend(rgba[4*(y*width+x)+0]/255.0f, data[4*(y*width+x)+2]/255.0f, alpha));
        data[4*(y*width+x)+3] = 255;
      }
    }


    SDL_UnlockSurface(surface);
}


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

int main(int, char**)
{
    // Setup SDL
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
	{
        printf("Error: %s\n", SDL_GetError());
        return -1;
	}

    // Setup window
	SDL_DisplayMode current;
	SDL_GetCurrentDisplayMode(0, &current);
	SDL_Window *window = SDL_CreateWindow("ImGui SDL2+RT example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 768, 768, SDL_WINDOW_RESIZABLE);

    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
      printf("SDL err: %s\n", SDL_GetError());
      exit(1);
    }

    SDL_Surface* surface = SDL_GetWindowSurface(window);
    if (!surface) {
      printf("SDL err: %s\n", SDL_GetError());
      exit(1);
    }



    // Setup ImGui binding
    ImGui_ImplRt_Init(window);
    printf("done init\n");

    // Load Fonts
    // (see extra_fonts/README.txt for more details)
    //ImGuiIO& io = ImGui::GetIO();
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../extra_fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../extra_fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../extra_fonts/ProggyClean.ttf", 13.0f);
    //io.Fonts->AddFontFromFileTTF("../../extra_fonts/ProggyTiny.ttf", 10.0f);
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());

    // Merge glyphs from multiple fonts into one (e.g. combine default font with another with Chinese glyphs, or add icons)
    //ImWchar icons_ranges[] = { 0xf000, 0xf3ff, 0 };
    //ImFontConfig icons_config; icons_config.MergeMode = true; icons_config.PixelSnapH = true;
    //io.Fonts->AddFontFromFileTTF("../../extra_fonts/DroidSans.ttf", 18.0f);
    //io.Fonts->AddFontFromFileTTF("../../extra_fonts/fontawesome-webfont.ttf", 18.0f, &icons_config, icons_ranges);

    std::vector<unsigned char> image(width * height * 4);

    SDL_SetRenderDrawColor(renderer, 114, 144, 154, 255);
    SDL_RenderClear(renderer);
    DrawGUI(window);
    ImGui_ImplRt_GetImage(&image.at(0));

    // Main loop
	bool done = false;
    while (!done)
    {
        SDL_Event event;
        bool updateGUI = false;
        while (SDL_PollEvent(&event))
        {
            if (ImGui_ImplRt_ProcessEvent(&event)) {
                updateGUI = true;
                
                if (event.type == SDL_QUIT) {
                    done = true;
                    break;
                }

            }
        }

        SDL_SetRenderDrawColor(renderer, 114, 144, 154, 255);
        SDL_RenderClear(renderer);

        if (updateGUI) {
            DrawGUI(window);
            ImGui_ImplRt_GetImage(&image.at(0));
        }

        Display(surface, &image.at(0), width, height);
        SDL_RenderPresent(renderer); // bitblit

        SDL_Delay(33);
    }

    // Cleanup
    ImGui_ImplRt_Shutdown();

	SDL_DestroyWindow(window);
	SDL_Quit();

    return 0;
}
