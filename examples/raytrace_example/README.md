# NanoRT + ImGui

Use NanoRT ( https://github.com/lighttransport/nanort ) to render ImGui. No GPU(and OpenGL) required.
It works on CPU only machine.

This example use SDL2 for window and mouse/keyboard event.

## Compile

    $ g++ -O2 -g -fopenmp `sdl2-config --cflags` -I../../ main.cpp imgui_impl_raytrace.cpp ../../imgui.cpp ../../imgui_draw.cpp ../../imgui_demo.cpp `sdl2-config --libs`

## TODO

* [ ] Performance optimization.
  * [ ] Shoot rays only in scissor region.
  * [ ] Hierarchical ray traversal.
* [ ] anti-aliasing.
* [ ] Bilinear texture filtering.
* [ ] reflection, refraction, glossy effect.

