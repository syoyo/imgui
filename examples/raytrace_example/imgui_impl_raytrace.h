class b3gDefaultWindow;

IMGUI_API bool ImGui_ImplRt_Init(b3gDefaultWindow *window);
IMGUI_API void ImGui_ImplRt_Shutdown();
IMGUI_API void ImGui_ImplRt_NewFrame(int mouse_x, int mouse_y);
// IMGUI_API bool        ImGui_ImplRt_ProcessEvent(SDL_Event* event);

IMGUI_API void ImGui_ImplRt_GetImage(unsigned char *dst_image);

// Use if you want to reset your rendering device without losing ImGui state.
IMGUI_API void ImGui_ImplRt_InvalidateDeviceObjects();
IMGUI_API bool ImGui_ImplRt_CreateDeviceObjects();
