#include <pixel/Window.hpp>
#include <pixel/Canvas.hpp>
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <string>
#include <stdexcept>

struct vertex {
  float x, y, z, w;
};

static const vertex screen_quad[] = {
  { -1, -1, 0.5, 1 },
  { -1,  1, 0.5, 1 },
  {  1, -1, 0.5, 1 },
  {  1, -1, 0.5, 1 },
  { -1,  1, 0.5, 1 },
  {  1,  1, 0.5, 1 },
};

static int compileShader(int prog, const char* source, int type)
{
  const auto shader = glCreateShader(type);
  glAttachShader(prog, shader);
  const auto src = source;
  const GLint len = strlen(source);
  glShaderSource(shader, 1, &src, &len);
  glCompileShader(shader);
  int ok = true;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok)
  {
    int length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    std::string errors;
    errors.reserve(length);
    glGetShaderInfoLog(shader, length, NULL, &errors[0]);
    fprintf(stderr, "%s", errors.c_str());
    throw std::runtime_error(errors);
  }
  return shader;
}

const char vertex_shader[] = 
"#version 330\n"
"\n"
"in vec4 in_pos;\n"
"out vec4 f_pos;\n"
"\n"
"void main() {\n"
"  gl_Position = in_pos;\n"
"  f_pos = in_pos;\n"
"}\n"
"\n"
;

const char fragment_shader[] = 
"#version 330\n"
"\n"
"uniform sampler2D tex;\n"
"in vec4 f_pos;\n"
"\n"
"void main() {\n"
"  gl_FragColor = texture(tex, 0.5 * f_pos.xy + vec2(0.5, 0.5));\n"
"}\n"
"\n"
;

namespace Pixel {
  Window::Window(uint32_t width, uint32_t height, std::string name)
  : width(width)
  , height(height)
  , surface(0)
  {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
      throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    surface = SDL_CreateWindow(name.c_str(),
                               50,
                               50,
                               width,
                               height,
                               SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    if (!surface)
      throw std::runtime_error(std::string("SDL_SetVideoMode failed: ") + SDL_GetError());

    context = SDL_GL_CreateContext((SDL_Window*)surface);
    if (!context)
      throw std::runtime_error(std::string("SDL_GL_CreateContext failed: ") + SDL_GetError());

    if (const auto err = glewInit(); err != GLEW_OK)
      throw std::runtime_error(std::string("glewInit failed: ") + SDL_GetError());

    prog = glCreateProgram();
    fs = compileShader(prog, fragment_shader, GL_FRAGMENT_SHADER);
    vs = compileShader(prog, vertex_shader, GL_VERTEX_SHADER);
    glBindAttribLocation(prog, 0, "in_pos");
    glLinkProgram(prog);
    int ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok)
    {
      int length = 0;
      glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &length);
      std::string errors;
      errors.reserve(length);
      glGetProgramInfoLog(prog, length, NULL, &errors[0]);
      throw std::runtime_error(std::string("Could not compile shader: ") + errors);
    }
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 16, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(screen_quad), &screen_quad, GL_STATIC_DRAW);
    glDisable(GL_DEPTH_TEST);
  }
  Window::~Window() {
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    SDL_Quit();
  }
  bool Window::shouldExit() {
    return shouldExit_;
  }
  void Window::show(Canvas& canvas) {
    int texture_id = canvas.update_texture();

    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "tex"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 16, (void*)0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    SDL_GL_SwapWindow((SDL_Window*)surface);
    SDL_Event event;
    while (SDL_PollEvent(&event))
    { 
      switch (event.type)
      {
        case SDL_QUIT:
          shouldExit_ = true;
          break;
        case SDL_KEYDOWN:
          switch (event.key.keysym.sym)
          {
            case SDLK_ESCAPE:
              shouldExit_ = true;
              break;
          // TODO
          } 
          break;
        case SDL_MOUSEBUTTONDOWN:
          // TODO
          break;
        case SDL_MOUSEMOTION:
          // TODO
          break;
      }
    }
  }
}
