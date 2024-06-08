// OpenGL utility functions are copied from https://github.com/hyprwm/hyprland-plugins
// under BSD 3-Clause License. A copy of the license can be found in the LICENSE file

#include "src/devices/ITouch.hpp"
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Shaders.hpp>
#include <utility>
#include <vector>

GLuint CompileShader(const GLuint& type, std::string src) {
    auto shader = glCreateShader(type);

    auto shaderSource = src.c_str();

    glShaderSource(shader, 1, (const GLchar**)&shaderSource, nullptr);
    glCompileShader(shader);

    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);

    if (ok == GL_FALSE)
        throw std::runtime_error("compileShader() failed!");

    return shader;
}

GLuint CreateProgram(const std::string& vert, const std::string& frag) {
    auto vertCompiled = CompileShader(GL_VERTEX_SHADER, vert);

    if (!vertCompiled)
        throw std::runtime_error("Compiling vshader failed.");

    auto fragCompiled = CompileShader(GL_FRAGMENT_SHADER, frag);

    if (!fragCompiled)
        throw std::runtime_error("Compiling fshader failed.");

    auto prog = glCreateProgram();
    glAttachShader(prog, vertCompiled);
    glAttachShader(prog, fragCompiled);
    glLinkProgram(prog);

    glDetachShader(prog, vertCompiled);
    glDetachShader(prog, fragCompiled);
    glDeleteShader(vertCompiled);
    glDeleteShader(fragCompiled);

    GLint ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);

    if (ok == GL_FALSE)
        throw std::runtime_error("createProgram() failed! GL_LINK_STATUS not OK!");

    return prog;
}

const std::string vertexShaderSource = R"#(

)#";

class Visualizer {
  public:
    void onRender();
    void damageAll();
    void damageFinger(uint64_t id);

    void onTouchDown(ITouch::SDownEvent);
    void onTouchUp(ITouch::SUpEvent);
    void onTouchMotion(ITouch::SMotionEvent);

  private:
    bool tempDamaged             = false;
    const int TOUCH_POINT_RADIUS = 15;
    std::vector<std::pair<uint64_t, Vector2D>> finger_positions;
    std::vector<std::pair<uint64_t, Vector2D>> prev_finger_positions;
};
