#include <stdio.h>
#include <stdlib.h>

#include <SDL3/SDL.h>
#include "vendor/glad/glad.h"

// ------------------------------------------------------------
// Shader helpers
// ------------------------------------------------------------
static GLuint compile_shader(GLenum type, const char *src)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        fprintf(stderr, "Shader compile error:\n%s\n", log);
        exit(1);
    }

    return shader;
}

static GLuint create_program(const char *vs_src, const char *fs_src)
{
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src);

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    if (!success) {
        char log[512];
        glGetProgramInfoLog(program, sizeof(log), NULL, log);
        fprintf(stderr, "Program link error:\n%s\n", log);
        exit(1);
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

// ------------------------------------------------------------
// Main
// ------------------------------------------------------------
Int ui_test2 (Void) {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window *window = SDL_CreateWindow(
        "Mykron",
        800, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );

    SDL_GLContext gl_ctx = SDL_GL_CreateContext(window);

    gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress);

    // --------------------------------------------------------
    // Triangle data
    // --------------------------------------------------------
    float vertices[] = {
         0.0f,  0.5f,
        -0.5f, -0.5f,
         0.5f, -0.5f
    };

    GLuint vao, vbo;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(
        0,              // location
        2,              // vec2
        GL_FLOAT,
        GL_FALSE,
        2 * sizeof(float),
        (void*)0
    );
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    // --------------------------------------------------------
    // Shaders
    // --------------------------------------------------------
    const char *vs_src =
        "#version 330 core\n"
        "layout (location = 0) in vec2 aPos;\n"
        "void main() {\n"
        "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
        "}\n";

    const char *fs_src =
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "void main() {\n"
        "    FragColor = vec4(1.0, 0.4, 0.2, 1.0);\n"
        "}\n";

    GLuint program = create_program(vs_src, fs_src);

    // --------------------------------------------------------
    // Main loop
    // --------------------------------------------------------
    int running = 1;

    while (running) {

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = 0;
            }
        }

        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        glViewport(0, 0, w, h);

        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(program);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        SDL_GL_SwapWindow(window);
    }

    // --------------------------------------------------------
    // Cleanup
    // --------------------------------------------------------
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(program);

    SDL_GL_DestroyContext(gl_ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
