# OpenGL Path-Tracing

Realtime Compute Shader Path-Tracing in OpenGL

Recommend not running this at full screen, and messing with shader parameters for more interactive framerates

## Controls

- Click-and-drag to rotate camera about focus
- Hold right-click to rotate camera in first-person (will snap back to focus after releasing right-click)
- Scroll to move towards or away from focus
- Hold left control and scroll to change the field of view

## Shader Recompilation

- Relevant shader code can be found in `ComputeShader.glsl`
  - Modify during execution: the program will recompile shaders on file save
  - Mess with constants (Lines 3-7)
- All shapes and objects are stored in the shader code, modify `boxes` and `spheres` arrays for different material parameters, sizes, and adding/removing objects
  - (will see performance penalty with more objects, no acceleration structure present)
