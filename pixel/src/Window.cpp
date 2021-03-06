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
R"(
#version 330

uniform vec4 sc_pos;

in vec4 in_pos;
out vec4 f_pos;

void main() {
//  mat2 r = mat2(-sin(sc_pos.w), cos(sc_pos.w), cos(sc_pos.w), sin(sc_pos.w));
  mat2 r = mat2(cos(sc_pos.w), sin(sc_pos.w), -sin(sc_pos.w), cos(sc_pos.w));
  gl_Position = vec4(((r * in_pos.xy + sc_pos.xy) * sc_pos.z), in_pos.zw);
  f_pos = in_pos;
}
)";

const char fragment_shader[] = 
R"(
#version 330

uniform vec4 sc_attr;
uniform sampler2D tex;
uniform vec4 key;
uniform int keyed;

in vec4 f_pos;

void main() {
  vec4 color = texture(tex, (0.5 * f_pos.xy + vec2(0.5, 0.5) + sc_attr.xy) * sc_attr.zw);
  if (keyed == 1 && key == color) discard;
  gl_FragColor = color;
}
)";

namespace Pixel {
  Window::Window(uint32_t width, uint32_t height, std::string name)
  : width(width)
  , height(height)
  , surface(0)
  , mouseX_(width/2)
  , mouseY_(width/2)
  , mouseZ_(0)
  , mouseButtons(0)
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
    glUseProgram(prog);
    glActiveTexture(GL_TEXTURE0);
    memset(keys, 0, sizeof(keys));
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

  void Window::set_as_target() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
  }

  void Window::clear(Color col) {
    glClearColor(col.r, col.g, col.b, col.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }

  void Window::text(std::string_view str, float x, float y, float scale, float rotation) {
    // TODO
  }

  Canvas Window::read() {
    set_as_target();
    Canvas c(width, height);
    glReadPixels(0, 0, width, height, GL_BGRA, GL_UNSIGNED_BYTE, c.canvas_.data());
    return c;
  }

  void Window::draw(const Subcanvas& canvas, float x, float y, float scale, float rotation) {
    int texture_id = canvas.canvas_->update_texture();
    if (canvas.canvas_->opacity == Canvas::translucent) {
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glEnable(GL_BLEND);
    } else {
      glDisable(GL_BLEND);
    }

    glUniform4f(glGetUniformLocation(prog, "sc_attr"), canvas.x, canvas.y, float(canvas.width) / canvas.canvas_->width(), float(canvas.height) / canvas.canvas_->height());
    glUniform4f(glGetUniformLocation(prog, "sc_pos"), x, y, scale / baseScale, rotation);
    glUniform4f(glGetUniformLocation(prog, "key"), canvas.canvas_->transparency_key.r, canvas.canvas_->transparency_key.g, canvas.canvas_->transparency_key.b, canvas.canvas_->transparency_key.a);
    glUniform1i(glGetUniformLocation(prog, "keyed"), canvas.canvas_->opacity == Canvas::keyed);
    glUniform1i(glGetUniformLocation(prog, "tex"), 0);
    static int lastTexture = texture_id;
    if (texture_id != lastTexture) {
        glBindTexture(GL_TEXTURE_2D, texture_id);
        lastTexture = texture_id;
    }
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 16, (void*)0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
  }

  void Window::show(const Subcanvas& subcanvas) {
    draw(subcanvas, 0, 0, baseScale, 0);

    show();
  }

  void Window::show() {
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
          }
          if (event.key.keysym.sym <= 0x7F) {
            keys[event.key.keysym.sym] = true;
          }
          break;
        case SDL_KEYUP:
          if (event.key.keysym.sym <= 0x7F) {
            keys[event.key.keysym.sym] = false;
          }
          break;
        case SDL_MOUSEBUTTONDOWN:
          break;
        case SDL_MOUSEBUTTONUP:
          break;
        case SDL_MOUSEMOTION:
          SDL_MouseMotionEvent* ev = (SDL_MouseMotionEvent*)&event;
          mouseX_ = ev->x;
          mouseY_ = height - ev->y;
          mouseButtons = ev->state;
          break;
      }
    }
  }

  int Window::mouseX()
  {
    return this->mouseX_;
  }

  int Window::mouseY()
  {
    return this->mouseY_;
  }

  int Window::mouseScroll()
  {
    int delta = mouseZ_;
    mouseZ_ = 0;
    return delta;
  }

  bool Window::mouseLeft()
  {
    return mouseButtons & 1;
  }

  bool Window::mouseRight()
  {
    return mouseButtons & 2;
  }

  bool Window::isPressed(int key)
  {
    return keys[key];
  }
}

