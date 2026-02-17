#include "vendor/glad/glad.h"
#include <SDL3/SDL.h>
#include "vendor/stb/stb_image.h"
#include "ui/font.h"
#include "base/log.h"
#include "base/math.h"
#include "base/string.h"
#include "os/time.h"
#include "base/map.h"
#include "buffer/buffer.h"
#include "os/fs.h"

static Void app_build ();
static Void app_init ();
static Void ui_init ();
static Void ui_frame (Void(*)(), F64 dt);
static Bool ui_is_animating ();

// =============================================================================
// Glfw and opengl layer:
// =============================================================================
#define VERTEX_MAX_BATCH_SIZE 2400

ienum (EventTag, U8) {
    EVENT_DUMMY,
    EVENT_EATEN,
    EVENT_WINDOW_SIZE,
    EVENT_MOUSE_MOVE,
    EVENT_SCROLL,
    EVENT_KEY_PRESS,
    EVENT_KEY_RELEASE,
    EVENT_TEXT_INPUT,
};

istruct (Event) {
    EventTag tag;
    F64 x;
    F64 y;
    Int key;
    Int mods;
    Int scancode;
    String text;
};

istruct (RectAttributes) {
    Vec4 color;
    Vec4 color2; // If x = -1, no gradient.
    Vec2 top_left;
    Vec2 bottom_right;
    Vec4 radius;
    F32  edge_softness;
    Vec4 border_color;
    Vec4 border_widths;
    Vec4 inset_shadow_color;
    Vec4 outset_shadow_color;
    F32  outset_shadow_width;
    F32  inset_shadow_width;
    Vec2 shadow_offsets;
    Vec4 texture_rect;
    Vec4 text_color;
    F32 text_is_grayscale;
};

istruct (Vertex) {
    Vec2 position;
    Vec4 color;
    Vec2 top_left;
    Vec2 bottom_right;
    Vec4 radius;
    F32 edge_softness;
    Vec4 border_color;
    Vec4 border_widths;
    Vec4 inset_shadow_color;
    Vec4 outset_shadow_color;
    F32 outset_shadow_width;
    F32 inset_shadow_width;
    Vec2 shadow_offsets;
    Vec2 uv;
    Vec4 text_color;
    F32 text_is_grayscale;
};

array_typedef(Vertex, Vertex);
array_typedef(Event, Event);

Int win_width  = 800;
Int win_height = 600;

#define BLUR_SHRINK 4
U32 blur_shader;
U32 blur_VBO, blur_VAO;
U32 blur_buffer1;
U32 blur_buffer2;
U32 blur_tex1;
U32 blur_tex2;
Array(struct { Vec2 pos; }) blur_vertices;

ArrayVertex vertices;
ArrayEvent events;

U32 rect_shader;
U32 VBO, VAO;
Mat4 projection;
U32 framebuffer;
U32 framebuffer_tex;

U32 screen_shader;
U32 screen_VBO, screen_VAO;
Array(struct { Vec2 pos; Vec2 tex; }) screen_vertices;

F32 dt;
F32 prev_frame;
F32 current_frame;
U64 frame_count;
F32 first_counted_frame;

static U32 framebuffer_new (U32 *out_texture, Bool only_color_attach, U32 w, U32 h);

static Void set_bool  (U32 p, CString name, Bool v) { glUniform1i(glGetUniformLocation(p, name), cast(Int, v)); }
static Void set_int   (U32 p, CString name, Int v)  { glUniform1i(glGetUniformLocation(p, name), v); }
static Void set_float (U32 p, CString name, F32 v)  { glUniform1f(glGetUniformLocation(p, name), v); }
static Void set_vec2  (U32 p, CString name, Vec2 v) { glUniform2f(glGetUniformLocation(p, name), v.x, v.y); }
static Void set_vec4  (U32 p, CString name, Vec4 v) { glUniform4f(glGetUniformLocation(p, name), v.x, v.y, v.z, v.w); }
static Void set_mat4  (U32 p, CString name, Mat4 m) { glUniformMatrix4fv(glGetUniformLocation(p, name), 1, GL_FALSE, cast(F32*, &m)); }

#define ATTR(T, OFFSET, LEN, NAME) ({\
    glVertexAttribPointer(OFFSET, LEN, GL_FLOAT, GL_FALSE, sizeof(T), cast(Void*, offsetof(T, NAME)));\
    glEnableVertexAttribArray(OFFSET);\
})

Noreturn static Void error () {
    log_scope_end_all();
    panic();
}

Noreturn Fmt(1, 2) static Void error_fmt (CString fmt, ...) {
    log_msg(m, LOG_ERROR, "UI", 1);
    astr_push_fmt_vam(m, fmt);
    astr_push_byte(m, '\n');
    error();
}

static Void update_projection () {
    F32 h = cast(F32, win_height);
    F32 w = cast(F32, win_width);
    projection = mat_ortho(0, w, 0, h, -1.f, 1.f);
}

istruct (Image) {
    U32 texture;
    F32 width;
    F32 height;
};

Image load_image (CString filepath, Bool flip) {
    U32 id; glGenTextures(1, &id);

    glBindTexture(GL_TEXTURE_2D, id);

    glTextureParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTextureParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_set_flip_vertically_on_load(flip);

    Int w, h, n; U8 *data = stbi_load(filepath, &w, &h, &n, 0);
    if (! data) error_fmt("Couldn't load image from file: %s\n", filepath);

    Int fmt = (n == 3) ? GL_RGB : GL_RGBA;
    glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);

    return (Image){
        .texture = id,
        .width   = w,
        .height  = h,
    };
}

static U32 framebuffer_new (U32 *out_texture, Bool only_color_attach, U32 w, U32 h) {
    U32 r;
    glGenFramebuffers(1, &r);
    glBindFramebuffer(GL_FRAMEBUFFER, r);

    U32 texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    if (out_texture) *out_texture = texture;

    if (! only_color_attach) {
        U32 rbo;
        glGenRenderbuffers(1, &rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
    }

    assert_always(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return r;
}

static U32 shader_compile (GLenum type, String filepath) {
    tmem_new(tm);

    String source = fs_read_entire_file(tm, filepath, 0);
    if (! source.data) error_fmt("Unable to read file: %.*s\n", STR(filepath));

    U32 shader = glCreateShader(type);

    glShaderSource(shader, 1, cast(const GLchar**, &source.data), 0);
    glCompileShader(shader);

    Int success; glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (! success) {
        log_msg(msg, LOG_ERROR, "", 1);
        Int count; glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &count);
        astr_push_fmt(msg, "Shader compilation error: %.*s\n  ", STR(filepath));
        U32 offset = msg->count;
        array_increase_count(msg, cast(U32, count), false);
        glGetShaderInfoLog(shader, count, 0, msg->data + offset);
        msg->count--; // Get rid of the NUL byte...
        error();
    }

    return shader;
}

static U32 shader_new (CString vshader_path, CString fshader_path) {
    U32 id      = glCreateProgram();
    U32 vshader = shader_compile(GL_VERTEX_SHADER, str(vshader_path));
    U32 fshader = shader_compile(GL_FRAGMENT_SHADER, str(fshader_path));

    glAttachShader(id, vshader);
    glAttachShader(id, fshader);
    glLinkProgram(id);

    Int success; glGetProgramiv(id, GL_LINK_STATUS, &success);
    if (! success) {
        log_msg(msg, LOG_ERROR, "", 1);
        Int count; glGetProgramiv(id, GL_INFO_LOG_LENGTH, &count);
        astr_push_cstr(msg, "Shader prog link error.\n  ");
        U32 offset = msg->count;
        array_increase_count(msg, cast(U32, count), false);
        glGetProgramInfoLog(id, count, 0, msg->data + offset);
        msg->count--; // Get rid of the NUL byte...
        error();
    }

    glDeleteShader(vshader);
    glDeleteShader(fshader);

    return id;
}

static Void flush_vertices () {
    glBindVertexArray(VAO);
    glUseProgram(rect_shader);
    set_mat4(rect_shader, "projection", projection);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    ATTR(Vertex, 0, 2, position);
    ATTR(Vertex, 1, 4, color);
    ATTR(Vertex, 2, 2, top_left);
    ATTR(Vertex, 3, 2, bottom_right);
    ATTR(Vertex, 4, 4, radius);
    ATTR(Vertex, 5, 1, edge_softness);
    ATTR(Vertex, 6, 4, border_color);
    ATTR(Vertex, 7, 4, border_widths);
    ATTR(Vertex, 8, 4, inset_shadow_color);
    ATTR(Vertex, 9, 4, outset_shadow_color);
    ATTR(Vertex, 10, 1, outset_shadow_width);
    ATTR(Vertex, 11, 1, inset_shadow_width);
    ATTR(Vertex, 12, 2, shadow_offsets);
    ATTR(Vertex, 13, 2, uv);
    ATTR(Vertex, 14, 4, text_color);
    ATTR(Vertex, 15, 1, text_is_grayscale);

    glBufferData(GL_ARRAY_BUFFER, array_size(&vertices), vertices.data, GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, vertices.count);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    vertices.count = 0;
}

static Vertex *reserve_vertices (U32 n) {
    if (vertices.count + n >= VERTEX_MAX_BATCH_SIZE) flush_vertices();
    SliceVertex slice;
    array_increase_count_o(&vertices, n, false, &slice);
    return slice.data;
}

static Void draw_rect_vertex (Vertex *v, Vec2 pos, Vec2 uv, Vec4 color, RectAttributes *a) {
    v->position            = pos;
    v->color               = color;
    v->top_left            = a->top_left;
    v->bottom_right        = a->bottom_right;
    v->radius              = a->radius;
    v->edge_softness       = a->edge_softness;
    v->border_color        = a->border_color;
    v->border_widths       = a->border_widths;
    v->inset_shadow_color  = a->inset_shadow_color;
    v->outset_shadow_color = a->outset_shadow_color;
    v->outset_shadow_width = a->outset_shadow_width;
    v->inset_shadow_width  = a->inset_shadow_width;
    v->shadow_offsets      = a->shadow_offsets;
    v->uv                  = uv;
    v->text_color          = a->text_color;
    v->text_is_grayscale   = a->text_is_grayscale;
}

#define draw_rect(...) draw_rect_fn(&(RectAttributes){__VA_ARGS__})

static SliceVertex draw_rect_fn (RectAttributes *a) {
    Vertex *p = reserve_vertices(6);

    if (a->color2.x == -1.0) a->color2 = a->color;

    // We make the rect slightly bigger because the fragment
    // shader will shrink it to make room for the drop shadow.
    a->top_left.x     -= 2*a->outset_shadow_width + 2*a->edge_softness;
    a->top_left.y     -= 2*a->outset_shadow_width + 2*a->edge_softness;
    a->bottom_right.x += 2*a->outset_shadow_width + 2*a->edge_softness;
    a->bottom_right.y += 2*a->outset_shadow_width + 2*a->edge_softness;

    a->top_left.y = win_height - a->top_left.y;
    a->bottom_right.y = win_height - a->bottom_right.y;

    Vec2 bottom_left = vec2(a->top_left.x, a->bottom_right.y);
    Vec2 top_right   = vec2(a->bottom_right.x, a->top_left.y);

    Vec4 tr = a->texture_rect;

    draw_rect_vertex(&p[0], a->top_left, vec2(tr.x, tr.y), a->color, a);
    draw_rect_vertex(&p[1], bottom_left, vec2(tr.x, tr.y+tr.w), a->color2, a);
    draw_rect_vertex(&p[2], a->bottom_right, vec2(tr.x+tr.z, tr.y+tr.w), a->color2, a);
    draw_rect_vertex(&p[3], a->bottom_right, vec2(tr.x+tr.z, tr.y+tr.w), a->color2, a);
    draw_rect_vertex(&p[4], top_right, vec2(tr.x+tr.z, tr.y), a->color, a);
    draw_rect_vertex(&p[5], a->top_left, vec2(tr.x, tr.y), a->color, a);

    return (SliceVertex){p,6};
}

static Void set_clipboard_text (String str) {
    tmem_new(tm);
    SDL_SetClipboardText(cstr(tm, str));
}

static String get_clipboard_text (Mem *mem) {
    CString txt = SDL_GetClipboardText();
    String result = str_copy(mem, str(txt));
    SDL_free(txt);
    return result;
}

F64 get_time_sec () {
    U64 counter = SDL_GetPerformanceCounter();
    U64 freq    = SDL_GetPerformanceFrequency();
    return cast(F64, counter) / cast(F64, freq);
}

static Void process_event (SDL_Event *event, Bool *running) {
    switch (event->type) {
    case SDL_EVENT_QUIT: {
        *running = false;
    } break;

    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
        Int width  = event->window.data1;
        Int height = event->window.data2;

        win_width  = width;
        win_height = height;

        update_projection();
        glViewport(0, 0, width, height);

        framebuffer = framebuffer_new(&framebuffer_tex, 1, win_width, win_height);
        blur_buffer1 = framebuffer_new(&blur_tex1, 1, floor(win_width  / BLUR_SHRINK), floor(win_height / BLUR_SHRINK));
        blur_buffer2 = framebuffer_new(&blur_tex2, 1, floor(win_width  / BLUR_SHRINK), floor(win_height / BLUR_SHRINK));

        glScissor(0, 0, width, height);

        Auto e = array_push_slot(&events);
        e->tag = EVENT_WINDOW_SIZE;
    } break;

    case SDL_EVENT_MOUSE_WHEEL: {
        Auto e = array_push_slot(&events);
        e->tag = EVENT_SCROLL;
        e->x = event->wheel.x;
        e->y = event->wheel.y;
    } break;

    case SDL_EVENT_MOUSE_MOTION: {
        Auto e = array_push_slot(&events);
        e->tag = EVENT_MOUSE_MOVE;
        e->x = event->motion.x;
        e->y = event->motion.y;
    } break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        Auto e  = array_push_slot(&events);
        e->tag  = (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) ? EVENT_KEY_PRESS : EVENT_KEY_RELEASE;
        e->key  = event->button.button;
        e->mods = SDL_GetModState();
    } break;

    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
        Auto e = array_push_slot(&events);
        e->tag = (event->type == SDL_EVENT_KEY_UP) ? EVENT_KEY_RELEASE : EVENT_KEY_PRESS;
        e->key = event->key.key;
        e->scancode = event->key.scancode;
        e->mods = event->key.mod;
    } break;

    case SDL_EVENT_TEXT_INPUT: {
        Auto e = array_push_slot(&events);
        e->tag = EVENT_TEXT_INPUT;
        e->text = str(cast(Char*, event->text.text));
    } break;
}
}

Void ui_test () {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window = SDL_CreateWindow("Mimui", 800, 600, SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    SDL_GLContext gl_ctx = SDL_GL_CreateContext(window);
    gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress);
    SDL_StartTextInput(window);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glEnable(GL_SCISSOR_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    framebuffer   = framebuffer_new(&framebuffer_tex, 1, win_width, win_height);
    blur_buffer1  = framebuffer_new(&blur_tex1, 1, floor(win_width/BLUR_SHRINK), floor(win_height/BLUR_SHRINK));
    blur_buffer2  = framebuffer_new(&blur_tex2, 1, floor(win_width/BLUR_SHRINK), floor(win_height/BLUR_SHRINK));
    rect_shader   = shader_new("src/ui/shaders/rect_vs.glsl", "src/ui/shaders/rect_fs.glsl");
    screen_shader = shader_new("src/ui/shaders/screen_vs.glsl", "src/ui/shaders/screen_fs.glsl");
    blur_shader   = shader_new("src/ui/shaders/blur_vs.glsl", "src/ui/shaders/blur_fs.glsl");

    { // Screen quad init:
        array_init(&screen_vertices, mem_root);
        array_push_lit(&screen_vertices, .pos={-1.0f,  1.0f},  .tex={0.0f, 1.0f});
        array_push_lit(&screen_vertices, .pos={-1.0f, -1.0f},  .tex={0.0f, 0.0f});
        array_push_lit(&screen_vertices, .pos={ 1.0f, -1.0f},  .tex={1.0f, 0.0f});
        array_push_lit(&screen_vertices, .pos={-1.0f,  1.0f},  .tex={0.0f, 1.0f});
        array_push_lit(&screen_vertices, .pos={ 1.0f, -1.0f},  .tex={1.0f, 0.0f});
        array_push_lit(&screen_vertices, .pos={ 1.0f,  1.0f},  .tex={1.0f, 1.0f});

        glGenVertexArrays(1, &screen_VAO);
        glGenBuffers(1, &screen_VBO);
        glBindVertexArray(screen_VAO);
        glBindBuffer(GL_ARRAY_BUFFER, screen_VBO);
        glBufferData(GL_ARRAY_BUFFER, array_size(&screen_vertices), screen_vertices.data, GL_STATIC_DRAW);
        ATTR(AElem(&screen_vertices), 0, 2, pos);
        ATTR(AElem(&screen_vertices), 1, 2, tex);

        glUseProgram(screen_shader);
        set_int(screen_shader, "tex", 0);
    }

    array_init(&blur_vertices, mem_root);
    glGenVertexArrays(1, &blur_VAO);
    glBindVertexArray(blur_VAO);
    glGenBuffers(1, &blur_VBO);
    ATTR(AElem(&blur_vertices), 0, 2, pos);
    glBindVertexArray(0);

    array_init(&vertices, mem_root);
    array_init(&events, mem_root);
    update_projection();

    ui_init();
    app_init();

    F64 dt   = 0;
    U64 now  = SDL_GetPerformanceCounter();
    U64 last = 0;

    Bool running = true;
    Bool poll_events = true;

    while (running) {
        last = now;
        now  =  SDL_GetPerformanceCounter();
        dt   = (now - last) / cast(F64, SDL_GetPerformanceFrequency());

        log_scope(ls, 1);

        #if 0
            static U64 fps_last_counter = 0;
            static U64 fps_frame_count  = 0;

            fps_frame_count++;
            if (fps_last_counter == 0) fps_last_counter = now;
            F64 elapsed = (now - fps_last_counter) / cast(F64, SDL_GetPerformanceFrequency());

            if (elapsed >= 0.5) {
                tmem_new(tm);
                F64 fps = cast(F64, fps_frame_count) / elapsed;
                SDL_SetWindowTitle(window, astr_fmt(tm, "fps: %.1f%c", fps, 0).data);
                fps_frame_count  = 0;
                fps_last_counter = now;
            }
        #endif

        SDL_Event event;
        if (poll_events || ui_is_animating()) {
            while (SDL_PollEvent(&event)) process_event(&event, &running);
        } else {
            SDL_WaitEvent(&event);
            do process_event(&event, &running); while (SDL_PollEvent(&event));
            poll_events = false;
            now = SDL_GetPerformanceCounter();
        }
        poll_events = !poll_events;

        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        if (events.count == 0) array_push_lit(&events, .tag=EVENT_DUMMY);
        ui_frame(app_build, dt);
        events.count = 0;
        if (vertices.count) flush_vertices();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(screen_shader);
        glBindVertexArray(screen_VAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, framebuffer_tex);
        glDrawArrays(GL_TRIANGLES, 0, screen_vertices.count);

        SDL_GL_SwapWindow(window);
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(rect_shader);
    glDeleteProgram(screen_shader);
    SDL_GL_DestroyContext(gl_ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

// =============================================================================
// UI layer:
// =============================================================================
typedef U64 UiKey;

ienum (UiSizeTag, U8) {
    UI_SIZE_CUSTOM,
    UI_SIZE_PIXELS,
    UI_SIZE_PCT_PARENT,
    UI_SIZE_CHILDREN_SUM,
};

#define UI_CONFIG_FONT_NORMAL        str("ui_config_font_normal")
#define UI_CONFIG_FONT_BOLD          str("ui_config_font_bold")
#define UI_CONFIG_FONT_MONO          str("ui_config_font_mono")
#define UI_CONFIG_FONT_ICONS         str("ui_config_font_icons")
#define UI_CONFIG_ANIMATION_TIME_1   str("ui_config_animation_time_1")
#define UI_CONFIG_ANIMATION_TIME_2   str("ui_config_animation_time_2")
#define UI_CONFIG_ANIMATION_TIME_3   str("ui_config_animation_time_3")
#define UI_CONFIG_LINE_SPACING       str("ui_config_line_spacing")
#define UI_CONFIG_SCROLLBAR_WIDTH    str("ui_config_scrollbar_width")
#define UI_CONFIG_PADDING_1          str("ui_config_padding_1")
#define UI_CONFIG_RADIUS_1           str("ui_config_radius_1")
#define UI_CONFIG_RADIUS_2           str("ui_config_radius_2")
#define UI_CONFIG_BORDER_1_COLOR     str("ui_config_border_1_color")
#define UI_CONFIG_BORDER_2_COLOR     str("ui_config_border_2_color")
#define UI_CONFIG_BORDER_1_WIDTH     str("ui_config_border_1_width")
#define UI_CONFIG_IN_SHADOW_1_WIDTH  str("ui_config_in_shadow_1_width")
#define UI_CONFIG_IN_SHADOW_1_COLOR  str("ui_config_in_shadow_1_color")
#define UI_CONFIG_SHADOW_1_COLOR     str("ui_config_shadow_1_color")
#define UI_CONFIG_SHADOW_1_WIDTH     str("ui_config_shadow_1_width")
#define UI_CONFIG_BORDER_FOCUS_WIDTH str("ui_config_border_focus_width")
#define UI_CONFIG_BORDER_FOCUS_COLOR str("ui_config_border_focus_color")
#define UI_CONFIG_BLUE_TEXT          str("ui_config_blue_text")
#define UI_CONFIG_RED_1              str("ui_config_red_1")
#define UI_CONFIG_RED_TEXT           str("ui_config_red_text")
#define UI_CONFIG_MAGENTA_1          str("ui_config_magenta_1")
#define UI_CONFIG_BG_1               str("ui_config_bg_1")
#define UI_CONFIG_BG_2               str("ui_config_bg_2")
#define UI_CONFIG_BG_3               str("ui_config_bg_3")
#define UI_CONFIG_BG_4               str("ui_config_bg_4")
#define UI_CONFIG_BG_SELECTION       str("ui_config_bg_selection")
#define UI_CONFIG_FG_1               str("ui_config_fg_1")
#define UI_CONFIG_FG_2               str("ui_config_fg_2")
#define UI_CONFIG_FG_3               str("ui_config_fg_3")
#define UI_CONFIG_FG_4               str("ui_config_fg_4")
#define UI_CONFIG_TEXT_SELECTION     str("ui_config_text_selection")
#define UI_CONFIG_TEXT_COLOR_1       str("ui_config_text_color_1")
#define UI_CONFIG_TEXT_COLOR_2       str("ui_config_text_color_2")
#define UI_CONFIG_BLUR               str("ui_config_blur")
#define UI_CONFIG_HIGHLIGHT          str("ui_config_highlight")
#define UI_CONFIG_SLIDER_KNOB        str("ui_config_slider_knob")

ienum (Icon, U32) {
    ICON_CHECK = 0xe900,
    ICON_WRENCH,
    ICON_UNDERSCORE,
    ICON_TRASH,
    ICON_TRANSLATE,
    ICON_TODO,
    ICON_TODO_LOADING,
    ICON_TIME_TRACKER,
    ICON_TIMER,
    ICON_STRIKETHROUGH,
    ICON_STOPWATCH,
    ICON_START,
    ICON_SORT_DESC,
    ICON_SORT_ASC,
    ICON_SEARCH,
    ICON_QUESTION,
    ICON_POMODORO,
    ICON_PLUS,
    ICON_PIN,
    ICON_PAUSE,
    ICON_PAN_UP,
    ICON_PAN_RIGHT,
    ICON_PAND_DOWN,
    ICON_MINUS,
    ICON_MARK,
    ICON_LINK,
    ICON_KANBAN,
    ICON_ITALIC,
    ICON_ISSUE,
    ICON_IMPORT_EXPORT,
    ICON_HOME,
    ICON_HIDDEN,
    ICON_HEATMAP,
    ICON_HEADER,
    ICON_HAMBURGER,
    ICON_GRAPH,
    ICON_GRAPH_INTERVAL,
    ICON_FULLSCREEN,
    ICON_FOLDER,
    ICON_FLASH,
    ICON_FIRE,
    ICON_FILTER,
    ICON_FILE,
    ICON_EYE,
    ICON_EYE_CLOSED,
    ICON_EXAM,
    ICON_EDIT,
    ICON_CODE,
    ICON_CLOSE,
    ICON_BOLD,
    ICON_ALARM,
};

#define get_icon(X) X

istruct (UiSize) {
    UiSizeTag tag;
    F32 value;
    F32 strictness;
};

ienum (UiAlign, U8) {
    UI_ALIGN_START,
    UI_ALIGN_MIDDLE,
    UI_ALIGN_END,
};

iunion (UiBoxSize) {
    struct { UiSize width, height; };
    UiSize v[2];
};

ienum (UiAxis, U8) {
    UI_AXIS_HORIZONTAL,
    UI_AXIS_VERTICAL,
};

assert_static(UI_AXIS_HORIZONTAL == 0);
assert_static(UI_AXIS_VERTICAL == 1);

ienum (UiStyleAttribute, U32) {
    UI_WIDTH,
    UI_HEIGHT,
    UI_AXIS,
    UI_BG_COLOR,
    UI_BG_COLOR2,
    UI_TEXT_COLOR,
    UI_RADIUS,
    UI_PADDING,
    UI_SPACING,
    UI_ALIGN_X,
    UI_ALIGN_Y,
    UI_FLOAT_X,
    UI_FLOAT_Y,
    UI_OVERFLOW_X,
    UI_OVERFLOW_Y,
    UI_EDGE_SOFTNESS,
    UI_BORDER_COLOR,
    UI_BORDER_WIDTHS,
    UI_INSET_SHADOW_COLOR,
    UI_OUTSET_SHADOW_COLOR,
    UI_INSET_SHADOW_WIDTH,
    UI_OUTSET_SHADOW_WIDTH,
    UI_SHADOW_OFFSETS,
    UI_BLUR_RADIUS,
    UI_FONT,
    UI_FONT_SIZE,
    UI_ANIMATION,
    UI_ANIMATION_TIME,
    UI_ATTRIBUTE_COUNT,
};

fenum (UiStyleMask, U32) {
    UI_MASK_WIDTH               = 1 << UI_WIDTH,
    UI_MASK_HEIGHT              = 1 << UI_HEIGHT,
    UI_MASK_AXIS                = 1 << UI_AXIS,
    UI_MASK_BG_COLOR            = 1 << UI_BG_COLOR,
    UI_MASK_BG_COLOR2           = 1 << UI_BG_COLOR2,
    UI_MASK_TEXT_COLOR          = 1 << UI_TEXT_COLOR,
    UI_MASK_RADIUS              = 1 << UI_RADIUS,
    UI_MASK_PADDING             = 1 << UI_PADDING,
    UI_MASK_SPACING             = 1 << UI_SPACING,
    UI_MASK_ALIGN_X             = 1 << UI_ALIGN_X,
    UI_MASK_ALIGN_Y             = 1 << UI_ALIGN_Y,
    UI_MASK_FLOAT_X             = 1 << UI_FLOAT_X,
    UI_MASK_FLOAT_Y             = 1 << UI_FLOAT_Y,
    UI_MASK_OVERFLOW_X          = 1 << UI_OVERFLOW_X,
    UI_MASK_OVERFLOW_Y          = 1 << UI_OVERFLOW_Y,
    UI_MASK_EDGE_SOFTNESS       = 1 << UI_EDGE_SOFTNESS,
    UI_MASK_BORDER_COLOR        = 1 << UI_BORDER_COLOR,
    UI_MASK_BORDER_WIDTHS       = 1 << UI_BORDER_WIDTHS,
    UI_MASK_INSET_SHADOW_COLOR  = 1 << UI_INSET_SHADOW_COLOR,
    UI_MASK_OUTSET_SHADOW_COLOR = 1 << UI_OUTSET_SHADOW_COLOR,
    UI_MASK_INSET_SHADOW_WIDTH  = 1 << UI_INSET_SHADOW_WIDTH,
    UI_MASK_OUTSET_SHADOW_WIDTH = 1 << UI_OUTSET_SHADOW_WIDTH,
    UI_MASK_SHADOW_OFFSETS      = 1 << UI_SHADOW_OFFSETS,
    UI_MASK_BLUR_RADIUS         = 1 << UI_BLUR_RADIUS,
    UI_MASK_FONT                = 1 << UI_FONT,
    UI_MASK_FONT_SIZE           = 1 << UI_FONT_SIZE,
    UI_MASK_ANIMATION           = 1 << UI_ANIMATION,
    UI_MASK_ANIMATION_TIME      = 1 << UI_ANIMATION_TIME,
};

istruct (UiStyle) {
    UiBoxSize size;
    UiAxis axis;
    Vec4 bg_color;
    Vec4 bg_color2; // If x = -1, no gradient.
    Vec4 text_color;
    Vec4 radius;
    Vec2 padding;
    F32  spacing;
    UiAlign align[2];
    F32  edge_softness;
    F32 floating[2]; // If NAN no floating.
    U32 overflow[2];
    Vec4 border_color;
    Vec4 border_widths;
    Vec4 inset_shadow_color;
    Vec4 outset_shadow_color;
    F32  inset_shadow_width;
    F32  outset_shadow_width;
    Vec2 shadow_offsets;
    F32  blur_radius; // 0 means no background blur.
    Font *font;
    U32 font_size;
    UiStyleMask animation_mask;
    F32 animation_time;
};

ienum (UiPatternTag, U8) {
    UI_PATTERN_PATH,
    UI_PATTERN_AND,
    UI_PATTERN_ANY,
    UI_PATTERN_ID,
    UI_PATTERN_TAG,
    UI_PATTERN_IS_ODD,
    UI_PATTERN_IS_EVEN,
    UI_PATTERN_IS_FIRST,
    UI_PATTERN_IS_LAST,
};

istruct (UiSpecificity) {
    U32 id;
    U32 tag;
};

istruct (UiPattern) {
    UiPatternTag tag;
    String string;
    UiSpecificity specificity;
    Array(UiPattern*) patterns;
};

istruct (UiStyleRule) {
    UiStyle *style;
    UiPattern *pattern;
    UiStyleMask mask;
};

ienum (UiConfigTag, U8) {
    UI_CONFIG_U32,
    UI_CONFIG_F32,
    UI_CONFIG_VEC2,
    UI_CONFIG_VEC4,
    UI_CONFIG_FONT,
    UI_CONFIG_SIZE,
};

istruct (UiConfig) {
    UiConfigTag tag;
    String name;
    union {
        U32 u32;
        F32 f32;
        Vec2 vec2;
        Vec4 vec4;
        Font *font;
        UiSize size;
    };
};

istruct (UiBox);
array_typedef(UiBox*, UiBox);
array_typedef(UiPattern*, UiPattern);
array_typedef(UiStyleRule, UiStyleRule);
array_typedef(UiSpecificity, UiSpecificity);

istruct (UiSignals) {
    Bool hovered;
    Bool pressed;
    Bool clicked;
    Bool focused;
};

istruct (UiRect) {
    union { struct { F32 x, y; }; Vec2 top_left; };
    union { struct { F32 w, h; }; F32 size[2]; };
};

fenum (UiBoxFlags, U8) {
    UI_BOX_REACTIVE      = flag(0),
    UI_BOX_CAN_FOCUS     = flag(1),
    UI_BOX_INVISIBLE     = flag(2),
    UI_BOX_CLIPPING      = flag(3),
    UI_BOX_CLICK_THROUGH = flag(4),
};

typedef Void (*UiBoxDrawFn)(UiBox*);
typedef Void (*UiBoxSizeFn)(UiBox*, U64 axis);

istruct (UiBox) {
    UiBox *parent;
    ArrayUiBox children;
    UiStyle style;
    UiStyle next_style;
    ArrayUiStyleRule style_rules;
    Array(UiConfig) configs;
    ArrayString tags;
    UiSignals signals;
    String label;
    UiKey key;
    U64 start_frame;
    UiBoxFlags flags;
    U8 gc_flag;
    U64 scratch;
    UiRect rect;
    UiBoxDrawFn draw_fn;
    UiBoxSizeFn size_fn;

    // The x/y components of this field are set independently
    // by the user build code for the purpose of scrolling the
    // content. The w/h components are set by the layout code.
    UiRect content;
};

istruct (UiBoxCallback) {
    Void (*fn)(UiBox*);
    UiBox *box;
};

istruct (Ui) {
    Mem *perm_mem;
    Mem *frame_mem;
    U8 gc_flag;
    Event *event;
    Vec2 mouse_dt;
    Vec2 mouse;
    Bool animation_running;
    Map(U32, U8) pressed_keys;
    U64 frame;
    F64 dt;
    UiBox *root;
    UiBox *hovered;
    UiBox *focused;
    U64 focus_idx;
    ArrayUiBox depth_first;
    ArrayUiBox free_boxes;
    ArrayUiBox box_stack;
    Map(UiKey, UiBox*) box_cache;
    Map(UiKey, Void*) box_data;
    Array(UiRect) clip_stack;
    Array(UiBoxCallback) deferred_layout_fns;
    UiStyleRule *current_style_rule;
    FontCache *font_cache;
    Font *font;
};

Ui *ui;

UiStyle default_box_style = {
    .size.width     = {UI_SIZE_CHILDREN_SUM, 0, 0},
    .size.height    = {UI_SIZE_CHILDREN_SUM, 0, 0},
    .bg_color2      = {-1},
    .text_color     = {1, 1, 1, .8},
    .edge_softness  = .75,
    .floating[0]    = NAN,
    .floating[1]    = NAN,
    .animation_time = .3,
};

istruct (UiTextBox);
static Void ui_eat_event ();
static Void ui_tag (CString tag);
static Void grab_focus (UiBox *box);
static Vec2 text_box_cursor_to_coord (UiTextBox *info, UiBox *box, BufCursor *);
static BufCursor text_box_coord_to_cursor (UiTextBox *info, UiBox *box, Vec2 coord);

static Void free_box_data (UiBox *box) {
    Void *data = map_get_ptr(&ui->box_data, box->key);

    if (data) {
        Mem **mem = data;
        arena_destroy(cast(Arena*, *mem));
        map_remove(&ui->box_data, box->key);
    }
}

static Void *get_box_data (UiBox *box, U64 size, U64 arena_block_size) {
    Void *data = map_get_ptr(&ui->box_data, box->key);

    if (! data) {
        Arena *arena = arena_new(mem_root, arena_block_size);
        Mem **header = mem_alloc(arena, Mem*, .size=size, .zeroed=true);
        *header = cast(Mem*, arena);
        data = header;
        map_add(&ui->box_data, box->key, data);
    }

    return data;
}

static Bool is_key_pressed (Int key) {
    U8 val; Bool pressed = map_get(&ui->pressed_keys, key, &val);
    return pressed;
}

static Bool within_box (UiRect r, Vec2 p) {
    return (p.x > r.x) && (p.x < (r.x + r.w)) && (p.y > r.y) && (p.y < (r.y + r.h));
}

static UiRect compute_rect_intersect (UiRect r0, UiRect r1) {
    F32 x0 = max(r0.x, r1.x);
    F32 y0 = max(r0.y, r1.y);
    F32 x1 = min(r0.x + r0.w, r1.x + r1.w);
    F32 y1 = min(r0.y + r0.h, r1.y + r1.h);
    return (UiRect){ x0, y0, max(0, x1 - x0), max(0, y1 - y0) };
}

static Void compute_signals (UiBox *box) {
    UiSignals *sig = &box->signals;
    Bool pressed = sig->pressed;
    *sig = (UiSignals){};

    if (! (box->flags & UI_BOX_REACTIVE)) return;

    sig->focused = (box == ui->focused);
    sig->clicked = sig->focused && (ui->event->tag == EVENT_KEY_PRESS) && (ui->event->key == SDLK_RETURN);

    sig->hovered = false;
    for (UiBox *b = ui->hovered; b; b = b->parent) {
        if (b == box) {
            UiRect intersection = compute_rect_intersect(box->rect, array_get_last(&ui->clip_stack));
            sig->hovered = within_box(intersection, ui->mouse);
            break;
        }
    }

    if (! pressed) {
        Int k = ui->event->key;
        sig->pressed = (ui->hovered == box) && (ui->event->tag == EVENT_KEY_PRESS) && (k == SDL_BUTTON_LEFT || k == SDL_BUTTON_MIDDLE || k == SDL_BUTTON_RIGHT);
    } else if ((ui->event->tag == EVENT_KEY_RELEASE) && (ui->event->key == SDL_BUTTON_LEFT)) {
        sig->pressed = false;
        if (sig->hovered) sig->clicked = true;
    } else {
        sig->pressed = true;
    }
}

static UiKey ui_build_key (String string) {
    UiBox *parent = array_try_get_last(&ui->box_stack);
    U64 seed = parent ? parent->key : 0;
    return str_hash_seed(string, seed);
}

static Void ui_push_parent  (UiBox *box) { array_push(&ui->box_stack, box); }
static UiBox *ui_pop_parent ()           { return array_pop(&ui->box_stack); }
static Void ui_pop_parent_  (Void *)     { array_pop(&ui->box_stack); }

#define ui_parent(...)\
    ui_push_parent(__VA_ARGS__);\
    if (cleanup(ui_pop_parent_) U8 _; 1)

static UiBox *ui_box_push_str (UiBoxFlags flags, String label) {
    UiKey key  = ui_build_key(label);
    UiBox *box = map_get_ptr(&ui->box_cache, key);

    if (box) {
        if (box->gc_flag == ui->gc_flag) error_fmt("UiBox label hash collision: [%.*s] vs [%.*s].", STR(box->label), STR(label));
        box->parent = 0;
        box->draw_fn = 0;
        box->size_fn = 0;
        box->tags.count = 0;
        box->children.count = 0;
        box->style_rules.count = 0;
        box->configs.count = 0;
    } else if (ui->free_boxes.count) {
        box = array_pop(&ui->free_boxes);
        box->parent = 0;
        box->tags.count = 0;
        box->children.count = 0;
        box->style_rules.count = 0;
        box->configs.count = 0;
        box->style = default_box_style;
        box->rect = (UiRect){};
        box->content = (UiRect){};
        box->start_frame = ui->frame;
        box->scratch = 0;
        box->draw_fn = 0;
        box->size_fn = 0;
        map_add(&ui->box_cache, key, box);
    } else {
        box = mem_new(ui->perm_mem, UiBox);
        array_init(&box->children, ui->perm_mem);
        array_init(&box->style_rules, ui->perm_mem);
        array_init(&box->configs, ui->perm_mem);
        array_init(&box->tags, ui->perm_mem);
        box->style = default_box_style;
        box->start_frame = ui->frame;
        map_add(&ui->box_cache, key, box);
    }

    array_push(&ui->depth_first, box);
    box->next_style = default_box_style;
    box->label = str_copy(ui->frame_mem, label);
    box->key = key;
    box->gc_flag = ui->gc_flag;
    box->flags = flags;
    Auto parent = array_try_get_last(&ui->box_stack);
    if (parent) array_push(&parent->children, box);
    box->parent = parent;
    ui_push_parent(box);
    compute_signals(box);
    if (box->signals.focused) ui_tag("focus");
    if (box->signals.hovered) ui_tag("hover");
    if (box->signals.pressed) ui_tag("press");
    return box;
}

static UiBox *ui_box_push_fmt (UiBoxFlags flags, CString fmt, ...) {
    tmem_new(tm);
    AString a = astr_new(tm);
    astr_push_fmt_vam(&a, fmt);
    return ui_box_push_str(flags, astr_to_str(&a));
}

static UiBox *ui_box_push (UiBoxFlags flags, CString label) {
    return ui_box_push_str(flags, str(label));
}

#define ui_box(...)     ui_box_push(__VA_ARGS__);     if (cleanup(ui_pop_parent_) U8 _; 1)
#define ui_box_str(...) ui_box_push_str(__VA_ARGS__); if (cleanup(ui_pop_parent_) U8 _; 1)
#define ui_box_fmt(...) ui_box_push_fmt(__VA_ARGS__); if (cleanup(ui_pop_parent_) U8 _; 1)

static UiRect ui_push_clip_rect (UiRect rect) {
    UiRect intersection = compute_rect_intersect(rect, array_get_last(&ui->clip_stack));
    array_push(&ui->clip_stack, intersection);
    return intersection;
}

static UiRect ui_push_clip (UiBox *box, Bool is_sub_clip) {
    box->flags |= UI_BOX_CLIPPING;
    UiRect rect = box->rect;
    rect.x += box->style.border_widths.z;
    rect.y += box->style.border_widths.y;
    rect.w -= box->style.border_widths.x + box->style.border_widths.z;
    rect.h -= box->style.border_widths.w + box->style.border_widths.y;
    if (is_sub_clip) return ui_push_clip_rect(rect);
    array_push(&ui->clip_stack, rect);
    return rect;
}

static UiRect ui_pop_clip () {
    array_pop(&ui->clip_stack);
    return array_get_last(&ui->clip_stack);
}

static Void animate_f32 (F32 *current, F32 final, F32 duration) {
    if (isnan(*current)) *current = 0;
    const F32 epsilon = 0.001f;
    if (duration <= 0.0f) { *current = final; return; }
    if (fabsf(*current - final) <= epsilon) { *current = final; return; }
    ui->animation_running = true;
    *current = lerp_f32(*current, final, 1.0f - powf(epsilon, ui->dt / duration));
}

static Void animate_vec2 (Vec2 *current, Vec2 final, F32 duration) {
    animate_f32(&current->x, final.x, duration);
    animate_f32(&current->y, final.y, duration);
}

static Void animate_vec4 (Vec4 *current, Vec4 final, F32 duration) {
    animate_f32(&current->x, final.x, duration);
    animate_f32(&current->y, final.y, duration);
    animate_f32(&current->z, final.z, duration);
    animate_f32(&current->w, final.w, duration);
}

static Void animate_size (UiSize *current, UiSize final, F32 duration) {
    current->tag = final.tag;
    current->strictness = final.strictness;
    animate_f32(&current->value, final.value, duration);
}

static Void animate_style (UiBox *box) {
    UiStyle *a = &box->style;
    UiStyle *b = &box->next_style;
    F32 duration = box->next_style.animation_time;
    UiStyleMask mask = box->next_style.animation_mask;

    #define X(T, M, F) if (mask & M) animate_##T(&a->F, b->F, duration); else a->F = b->F;

    X(size, UI_MASK_WIDTH, size.width);
    X(size, UI_MASK_HEIGHT, size.height);
    X(vec4, UI_MASK_BG_COLOR, bg_color);
    X(vec4, UI_MASK_BG_COLOR2, bg_color2);
    X(vec4, UI_MASK_TEXT_COLOR, text_color);
    X(vec4, UI_MASK_RADIUS, radius);
    X(vec2, UI_MASK_PADDING, padding);
    X(f32, UI_MASK_SPACING, spacing);
    X(vec4, UI_MASK_BORDER_COLOR, border_color);
    X(vec4, UI_MASK_BORDER_WIDTHS, border_widths);
    X(vec4, UI_MASK_INSET_SHADOW_COLOR, inset_shadow_color);
    X(vec4, UI_MASK_OUTSET_SHADOW_COLOR, outset_shadow_color);
    X(f32, UI_MASK_INSET_SHADOW_WIDTH, inset_shadow_width);
    X(f32, UI_MASK_OUTSET_SHADOW_WIDTH, outset_shadow_width);
    X(vec2, UI_MASK_SHADOW_OFFSETS, shadow_offsets);
    X(f32, UI_MASK_BLUR_RADIUS, blur_radius);
    X(f32, UI_MASK_FLOAT_X, floating[0]);
    X(f32, UI_MASK_FLOAT_Y, floating[1]);

    #undef X

    a->axis = b->axis;
    a->align[0] = b->align[0];
    a->align[1] = b->align[1];
    a->edge_softness = b->edge_softness;
    a->overflow[0] = b->overflow[0];
    a->overflow[1] = b->overflow[1];
    a->font = b->font;
    a->font_size = b->font_size;
}

// =============================================================================
// Style rules:
// =============================================================================
static UiPattern *pattern_alloc (Mem *mem, UiPatternTag tag) {
    Auto p = mem_new(mem, UiPattern);
    p->tag = tag;
    array_init(&p->patterns, mem);
    return p;
}

#define pattern_advance(S, N) (S)->data += N; (S)->count -= N;

static String parse_pattern_name (String *chunk) {
    U64 n = chunk->count;
    array_iter (c, chunk) {
        if (c == '#' || c == '.' || c == ':' || c == ' ') {
            n = ARRAY_IDX;
            break;
        }
    }

    if (n == 0) error_fmt("Expected selector name: [%.*s]", STR(*chunk));

    String slice = str_slice(*chunk, 0, n);
    pattern_advance(chunk, n);
    return slice;
}

static UiPattern *parse_pattern_and (String chunk, Mem *mem) {
    Auto result = pattern_alloc(mem, UI_PATTERN_AND);

    while (chunk.count) {
        Auto selector = pattern_alloc(mem, 0);
        array_push(&result->patterns, selector);

        Auto c = array_get(&chunk, 0);
        pattern_advance(&chunk, 1);

        switch (c) {
        case '*': selector->tag = UI_PATTERN_ANY; break;
        case '#': result->specificity.id++; selector->tag = UI_PATTERN_ID;  selector->string = parse_pattern_name(&chunk); break;
        case '.': result->specificity.tag++; selector->tag = UI_PATTERN_TAG; selector->string = parse_pattern_name(&chunk); break;
        case ':': {
            result->specificity.tag++;
            if      (str_starts_with(chunk, str("first"))) { pattern_advance(&chunk, 5); selector->tag = UI_PATTERN_IS_FIRST; }
            else if (str_starts_with(chunk, str("last")))  { pattern_advance(&chunk, 4); selector->tag = UI_PATTERN_IS_LAST; }
            else if (str_starts_with(chunk, str("odd")))   { pattern_advance(&chunk, 3); selector->tag = UI_PATTERN_IS_ODD; }
            else if (str_starts_with(chunk, str("even")))  { pattern_advance(&chunk, 4); selector->tag = UI_PATTERN_IS_EVEN; }
            else                                           error_fmt("Invalid pseudo tag: [%.*s]", STR(chunk));
        } break;

        default: error_fmt("Invalid selector: [%.*s]", STR(chunk));
        }
    }

    return result;
}

// Root node is UI_PATTERN_PATH and it's children UI_PATTERN_AND.
static UiPattern *parse_pattern (String pattern, Mem *mem) {
    tmem_new(tm);

    ArrayString chunks;
    array_init(&chunks, tm);
    str_split(pattern, str(" "), false, false, &chunks);

    Auto p = pattern_alloc(mem, UI_PATTERN_PATH);

    array_iter (chunk, &chunks) {
        UiPattern *child = parse_pattern_and(chunk, mem);
        array_push(&p->patterns, child);
        p->specificity.id += child->specificity.id;
        p->specificity.tag += child->specificity.tag;
    }

    return p;
}

static Void print_pattern (String text, UiPattern *pattern) {
    printf("%.*s\nspecificity=[%i, %i]\n\n", STR(text), pattern->specificity.id, pattern->specificity.tag);

    array_iter (chunk, &pattern->patterns) {
        printf("  %*s", cast(int, ARRAY_IDX), "");

        array_iter (selector, &chunk->patterns) {
            printf("[");
            switch (selector->tag) {
            case UI_PATTERN_ANY:      printf("*"); break;
            case UI_PATTERN_ID:       printf("#%.*s", STR(selector->string)); break;
            case UI_PATTERN_TAG:      printf(".%.*s", STR(selector->string)); break;
            case UI_PATTERN_IS_ODD:   printf(":odd"); break;
            case UI_PATTERN_IS_EVEN:  printf(":even"); break;
            case UI_PATTERN_IS_FIRST: printf(":first"); break;
            case UI_PATTERN_IS_LAST:  printf(":last"); break;
            case UI_PATTERN_PATH:     badpath;
            case UI_PATTERN_AND:      badpath;
            }
            printf("] ");
        }

        printf("\n");
    }

    printf("\n");
}

static UiStyleMask style_attr_to_mask (UiStyleAttribute attr) {
    return 1 << attr;
}

static Void ui_style_box_u32 (UiBox *box, UiStyleAttribute attr, U32 val) {
    Auto s = ui->current_style_rule ? ui->current_style_rule->style : &box->next_style;
    switch (attr) {
    case UI_ANIMATION:  s->animation_mask = val; break;
    case UI_ALIGN_X:    s->align[0] = val; break;
    case UI_ALIGN_Y:    s->align[1] = val; break;
    case UI_OVERFLOW_X: s->overflow[0] = val; break;
    case UI_OVERFLOW_Y: s->overflow[1] = val; break;
    case UI_AXIS:       s->axis = val; break;
    default:            error_fmt("Given attribute is not of type U32.");
    }
    if (ui->current_style_rule) ui->current_style_rule->mask |= style_attr_to_mask(attr);
}

static Void ui_style_box_font (UiBox *box, UiStyleAttribute attr, Font *val) {
    Auto s = ui->current_style_rule ? ui->current_style_rule->style : &box->next_style;
    switch (attr) {
    case UI_FONT: s->font = val; break;
    default:      error_fmt("Given attribute is not of type Font*.");
    }
    if (ui->current_style_rule) ui->current_style_rule->mask |= style_attr_to_mask(attr);
}

static Void ui_style_box_f32 (UiBox *box, UiStyleAttribute attr, F32 val) {
    Auto s = ui->current_style_rule ? ui->current_style_rule->style : &box->next_style;
    switch (attr) {
    case UI_ANIMATION_TIME:      s->animation_time = val; break;
    case UI_BLUR_RADIUS:         s->blur_radius = val; break;
    case UI_FLOAT_X:             s->floating[0] = val; break;
    case UI_FLOAT_Y:             s->floating[1] = val; break;
    case UI_SPACING:             s->spacing = val; break;
    case UI_EDGE_SOFTNESS:       s->edge_softness = val; break;
    case UI_INSET_SHADOW_WIDTH:  s->inset_shadow_width = val; break;
    case UI_OUTSET_SHADOW_WIDTH: s->outset_shadow_width = val; break;
    case UI_FONT_SIZE:           s->font_size = val; break;
    default:                     error_fmt("Given attribute is not of type F32.");
    }
    if (ui->current_style_rule) ui->current_style_rule->mask |= style_attr_to_mask(attr);
}

static Void ui_style_box_vec2 (UiBox *box, UiStyleAttribute attr, Vec2 val) {
    Auto s = ui->current_style_rule ? ui->current_style_rule->style : &box->next_style;
    switch (attr) {
    case UI_PADDING:        s->padding = val; break;
    case UI_SHADOW_OFFSETS: s->shadow_offsets = val; break;
    default:                error_fmt("Given attribute is not of type Vec2.");
    }
    if (ui->current_style_rule) ui->current_style_rule->mask |= style_attr_to_mask(attr);
}

static Void ui_style_box_vec4 (UiBox *box, UiStyleAttribute attr, Vec4 val) {
    Auto s = ui->current_style_rule ? ui->current_style_rule->style : &box->next_style;
    switch (attr) {
    case UI_BG_COLOR:            s->bg_color = val; break;
    case UI_BG_COLOR2:           s->bg_color2 = val; break;
    case UI_TEXT_COLOR:          s->text_color = val; break;
    case UI_RADIUS:              s->radius = val; break;
    case UI_BORDER_COLOR:        s->border_color = val; break;
    case UI_BORDER_WIDTHS:       s->border_widths = val; break;
    case UI_INSET_SHADOW_COLOR:  s->inset_shadow_color = val; break;
    case UI_OUTSET_SHADOW_COLOR: s->outset_shadow_color = val; break;
    default:                     error_fmt("Given attribute is not of type Vec4.");
    }
    if (ui->current_style_rule) ui->current_style_rule->mask |= style_attr_to_mask(attr);
}

static Void ui_style_box_size (UiBox *box, UiStyleAttribute attr, UiSize val) {
    Auto s = ui->current_style_rule ? ui->current_style_rule->style : &box->next_style;
    switch (attr) {
    case UI_WIDTH:  s->size.width = val; break;
    case UI_HEIGHT: s->size.height = val; break;
    default:        error_fmt("Given attribute is not of type UiSize.");
    }
    if (ui->current_style_rule) ui->current_style_rule->mask |= style_attr_to_mask(attr);
}

static Void ui_style_u32  (UiStyleAttribute attr, U32 val)    { ui_style_box_u32(array_get_last(&ui->box_stack), attr, val); }
static Void ui_style_f32  (UiStyleAttribute attr, F32 val)    { ui_style_box_f32(array_get_last(&ui->box_stack), attr, val); }
static Void ui_style_vec2 (UiStyleAttribute attr, Vec2 val)   { ui_style_box_vec2(array_get_last(&ui->box_stack), attr, val); }
static Void ui_style_vec4 (UiStyleAttribute attr, Vec4 val)   { ui_style_box_vec4(array_get_last(&ui->box_stack), attr, val); }
static Void ui_style_size (UiStyleAttribute attr, UiSize val) { ui_style_box_size(array_get_last(&ui->box_stack), attr, val); }
static Void ui_style_font (UiStyleAttribute attr, Font *val)  { ui_style_box_font(array_get_last(&ui->box_stack), attr, val); }

static Void ui_style_rule_push (UiBox *box, String pattern) {
    if (ui->current_style_rule) error_fmt("Style rule declarations cannot be nested.");
    UiStyleRule rule = {};
    rule.pattern = parse_pattern(pattern, ui->frame_mem);
    rule.style = mem_new(ui->frame_mem, UiStyle);
    *rule.style = default_box_style;
    array_push(&box->style_rules, rule);
    ui->current_style_rule = array_ref_last(&box->style_rules);
}

static Void ui_style_rule_pop (Void *) {
    ui->current_style_rule = 0;
}

#define ui_style_rule_box(BOX, ...)\
    ui_style_rule_push(BOX, str(__VA_ARGS__));\
    if (cleanup(ui_style_rule_pop) U8 _; 1)

#define ui_style_rule(...)\
    ui_style_rule_box(array_get_last(&ui->box_stack), __VA_ARGS__)

static Bool rule_applies (UiStyleRule *rule, UiSpecificity a, UiSpecificity *specs, UiStyleAttribute attr) {
    if (! (rule->mask & style_attr_to_mask(attr))) return false;
    UiSpecificity b = specs[attr];
    return (a.id > b.id) || ((a.id == b.id) && (a.tag >= b.tag));
}

static Void apply_style_rule (UiBox *box, UiStyleRule *rule, UiSpecificity *specs) {
    Auto s = rule->pattern->specificity;
    if (rule_applies(rule, s, specs, UI_WIDTH))               { box->next_style.size.width = rule->style->size.width; specs[UI_WIDTH] = s; }
    if (rule_applies(rule, s, specs, UI_HEIGHT))              { box->next_style.size.height = rule->style->size.height; specs[UI_HEIGHT] = s; }
    if (rule_applies(rule, s, specs, UI_AXIS))                { box->next_style.axis = rule->style->axis; specs[UI_AXIS] = s; }
    if (rule_applies(rule, s, specs, UI_BG_COLOR))            { box->next_style.bg_color = rule->style->bg_color; specs[UI_BG_COLOR] = s; }
    if (rule_applies(rule, s, specs, UI_BG_COLOR2))           { box->next_style.bg_color2 = rule->style->bg_color2; specs[UI_BG_COLOR2] = s; }
    if (rule_applies(rule, s, specs, UI_TEXT_COLOR))          { box->next_style.text_color = rule->style->text_color; specs[UI_TEXT_COLOR] = s; }
    if (rule_applies(rule, s, specs, UI_RADIUS))              { box->next_style.radius = rule->style->radius; specs[UI_RADIUS] = s; }
    if (rule_applies(rule, s, specs, UI_PADDING))             { box->next_style.padding = rule->style->padding; specs[UI_PADDING] = s; }
    if (rule_applies(rule, s, specs, UI_SPACING))             { box->next_style.spacing = rule->style->spacing; specs[UI_SPACING] = s; }
    if (rule_applies(rule, s, specs, UI_ALIGN_X))             { box->next_style.align[0] = rule->style->align[0]; specs[UI_ALIGN_X] = s; }
    if (rule_applies(rule, s, specs, UI_ALIGN_Y))             { box->next_style.align[1] = rule->style->align[1]; specs[UI_ALIGN_Y] = s; }
    if (rule_applies(rule, s, specs, UI_FLOAT_X))             { box->next_style.floating[0] = rule->style->floating[0]; specs[UI_FLOAT_X] = s; }
    if (rule_applies(rule, s, specs, UI_FLOAT_Y))             { box->next_style.floating[1] = rule->style->floating[1]; specs[UI_FLOAT_Y] = s; }
    if (rule_applies(rule, s, specs, UI_OVERFLOW_X))          { box->next_style.overflow[0] = rule->style->overflow[0]; specs[UI_OVERFLOW_X] = s; }
    if (rule_applies(rule, s, specs, UI_OVERFLOW_Y))          { box->next_style.overflow[1] = rule->style->overflow[1]; specs[UI_OVERFLOW_Y] = s; }
    if (rule_applies(rule, s, specs, UI_EDGE_SOFTNESS))       { box->next_style.edge_softness = rule->style->edge_softness; specs[UI_EDGE_SOFTNESS] = s; }
    if (rule_applies(rule, s, specs, UI_BORDER_COLOR))        { box->next_style.border_color = rule->style->border_color; specs[UI_BORDER_COLOR] = s; }
    if (rule_applies(rule, s, specs, UI_BORDER_WIDTHS))       { box->next_style.border_widths = rule->style->border_widths; specs[UI_BORDER_WIDTHS] = s; }
    if (rule_applies(rule, s, specs, UI_INSET_SHADOW_COLOR))  { box->next_style.inset_shadow_color = rule->style->inset_shadow_color; specs[UI_INSET_SHADOW_COLOR] = s; }
    if (rule_applies(rule, s, specs, UI_OUTSET_SHADOW_COLOR)) { box->next_style.outset_shadow_color = rule->style->outset_shadow_color; specs[UI_OUTSET_SHADOW_COLOR] = s; }
    if (rule_applies(rule, s, specs, UI_INSET_SHADOW_WIDTH))  { box->next_style.inset_shadow_width = rule->style->inset_shadow_width; specs[UI_INSET_SHADOW_WIDTH] = s; }
    if (rule_applies(rule, s, specs, UI_OUTSET_SHADOW_WIDTH)) { box->next_style.outset_shadow_width = rule->style->outset_shadow_width; specs[UI_OUTSET_SHADOW_WIDTH] = s; }
    if (rule_applies(rule, s, specs, UI_SHADOW_OFFSETS))      { box->next_style.shadow_offsets = rule->style->shadow_offsets; specs[UI_SHADOW_OFFSETS] = s; }
    if (rule_applies(rule, s, specs, UI_BLUR_RADIUS))         { box->next_style.blur_radius = rule->style->blur_radius; specs[UI_BLUR_RADIUS] = s; }
    if (rule_applies(rule, s, specs, UI_FONT))                { box->next_style.font = rule->style->font; specs[UI_FONT] = s; }
    if (rule_applies(rule, s, specs, UI_FONT_SIZE))           { box->next_style.font_size = rule->style->font_size; specs[UI_FONT_SIZE] = s; }
    if (rule_applies(rule, s, specs, UI_ANIMATION))           { box->next_style.animation_mask = rule->style->animation_mask; specs[UI_ANIMATION] = s; }
    if (rule_applies(rule, s, specs, UI_ANIMATION_TIME))      { box->next_style.animation_time = rule->style->animation_time; specs[UI_ANIMATION_TIME] = s; }
}

static Bool match_pattern (UiBox *box, UiPattern *pattern) {
    assert_dbg(pattern->tag == UI_PATTERN_AND);

    U64 box_idx = box->parent ? array_find(&box->parent->children, IT == box) : 0;
    assert_dbg(box_idx != ARRAY_NIL_IDX);

    Bool result = true;
    array_iter (selector, &pattern->patterns) {
        switch (selector->tag) {
        case UI_PATTERN_ID:       result = str_match(box->label, selector->string); break;
        case UI_PATTERN_IS_ODD:   result = (box_idx % 2); break;
        case UI_PATTERN_IS_EVEN:  result = !(box_idx % 2); break;
        case UI_PATTERN_IS_FIRST: result = (box_idx == 0); break;
        case UI_PATTERN_IS_LAST:  result = (box_idx == box->parent->children.count - 1); break;
        case UI_PATTERN_TAG:      result = array_find_ref(&box->tags, str_match(*IT, selector->string)); break;
        case UI_PATTERN_ANY:      break;
        case UI_PATTERN_PATH:     badpath;
        case UI_PATTERN_AND:      badpath;
        }

        if (! result) break;
    }

    return result;
}

static UiStyleRule derive_new_rule (UiStyleRule *old_rule, Mem *mem) {
    UiStyleRule new_rule = {};
    new_rule.style = old_rule->style;
    new_rule.mask = old_rule->mask;
    new_rule.pattern = pattern_alloc(mem, UI_PATTERN_PATH);
    *new_rule.pattern = *old_rule->pattern;
    new_rule.pattern->patterns.data++;
    new_rule.pattern->patterns.count--;
    return new_rule;
}

static Void apply_style_rules_box (UiBox *box, ArrayUiStyleRule *active_rules, Mem *mem) {
    U64 restore_point = active_rules->count;
    array_push_many(active_rules, &box->style_rules);

    UiSpecificity specs[UI_ATTRIBUTE_COUNT] = {};

    Auto stop_at = active_rules->count - 1; // Don't loop over newly added derived rules.
    array_iter (rule, active_rules) {
        UiPattern *head_of_rule = array_get(&rule.pattern->patterns, 0);
        Bool match = match_pattern(box, head_of_rule);

        if (match) {
            if (rule.pattern->patterns.count == 1) {
                apply_style_rule(box, &rule, specs);
            } else {
                array_push(active_rules, derive_new_rule(&rule, mem));
            }
        }

        if (ARRAY_IDX == stop_at) break;
    }

    array_iter (child, &box->children) apply_style_rules_box(child, active_rules, mem);
    active_rules->count = restore_point;
    animate_style(box);
}

static Void apply_style_rules (UiBox *box) {
    tmem_new(tm);
    ArrayUiStyleRule active_rules;
    array_init(&active_rules, tm);
    apply_style_rules_box(box, &active_rules, tm);
}

static Void ui_tag_box_str (UiBox *box, String tag)  { array_push(&box->tags, tag); }
static Void ui_tag_str     (String tag)              { return ui_tag_box_str(array_get_last(&ui->box_stack), tag); }
static Void ui_tag_box     (UiBox *box, CString tag) { return ui_tag_box_str(box, str(tag)); }
static Void ui_tag         (CString tag)             { return ui_tag_box_str(array_get_last(&ui->box_stack), str(tag)); }

Bool set_font (UiBox *box) {
    Font *font = box->style.font;
    U32 size = box->style.font_size;
    if (!font || !size) return false;
    if (ui->font != font || size != ui->font->size) {
        flush_vertices();
        ui->font = font_get(ui->font_cache, font->filepath, size, font->is_mono);
    }
    return true;
}

// =============================================================================
// Config:
// =============================================================================
static UiConfig *ui_config_get (String name) {
    array_iter_back (box, &ui->box_stack) {
        array_iter_back (config, &box->configs, *) {
            if (str_match(name, config->name)) {
                return config;
            }
        }
    }

    return 0;
}

static U32    ui_config_get_u32  (String name) { UiConfig *c = ui_config_get(name); assert_dbg(c->tag == UI_CONFIG_U32); return c->u32; }
static F32    ui_config_get_f32  (String name) { UiConfig *c = ui_config_get(name); assert_dbg(c->tag == UI_CONFIG_F32); return c->f32; }
static Vec2   ui_config_get_vec2 (String name) { UiConfig *c = ui_config_get(name); assert_dbg(c->tag == UI_CONFIG_VEC2); return c->vec2; }
static Vec4   ui_config_get_vec4 (String name) { UiConfig *c = ui_config_get(name); assert_dbg(c->tag == UI_CONFIG_VEC4); return c->vec4; }
static UiSize ui_config_get_size (String name) { UiConfig *c = ui_config_get(name); assert_dbg(c->tag == UI_CONFIG_SIZE); return c->size; }
static Font  *ui_config_get_font (String name) { UiConfig *c = ui_config_get(name); assert_dbg(c->tag == UI_CONFIG_FONT); return c->font; }

static Void ui_config_def (UiConfig config) {
    UiBox *box = array_get_last(&ui->box_stack);
    array_push(&box->configs, config);
}

static Void ui_config_def_u32  (String name, U32 val)    { ui_config_def((UiConfig){ UI_CONFIG_U32, name, .u32=val}); }
static Void ui_config_def_f32  (String name, F32 val)    { ui_config_def((UiConfig){ UI_CONFIG_F32, name, .f32=val}); }
static Void ui_config_def_vec2 (String name, Vec2 val)   { ui_config_def((UiConfig){ UI_CONFIG_VEC2, name, .vec2=val}); }
static Void ui_config_def_vec4 (String name, Vec4 val)   { ui_config_def((UiConfig){ UI_CONFIG_VEC4, name, .vec4=val}); }
static Void ui_config_def_size (String name, UiSize val) { ui_config_def((UiConfig){ UI_CONFIG_SIZE, name, .size=val}); }
static Void ui_config_def_font (String name, Font *val)  { ui_config_def((UiConfig){ UI_CONFIG_FONT, name, .font=val}); }

static Void ui_style_box_from_config (UiBox *box, UiStyleAttribute attr, String name) {
    UiConfig *config = ui_config_get(name);
    switch (config->tag) {
    case UI_CONFIG_U32:  ui_style_box_u32(box, attr, config->u32); break;
    case UI_CONFIG_F32:  ui_style_box_f32(box, attr, config->f32); break;
    case UI_CONFIG_VEC2: ui_style_box_vec2(box, attr, config->vec2); break;
    case UI_CONFIG_VEC4: ui_style_box_vec4(box, attr, config->vec4); break;
    case UI_CONFIG_FONT: ui_style_box_font(box, attr, config->font); break;
    case UI_CONFIG_SIZE: ui_style_box_size(box, attr, config->size); break;
    }
}

static Void ui_style_from_config (UiStyleAttribute attr, String name) {
    UiConfig *config = ui_config_get(name);
    switch (config->tag) {
    case UI_CONFIG_U32:  ui_style_u32(attr, config->u32); break;
    case UI_CONFIG_F32:  ui_style_f32(attr, config->f32); break;
    case UI_CONFIG_VEC2: ui_style_vec2(attr, config->vec2); break;
    case UI_CONFIG_VEC4: ui_style_vec4(attr, config->vec4); break;
    case UI_CONFIG_FONT: ui_style_font(attr, config->font); break;
    case UI_CONFIG_SIZE: ui_style_size(attr, config->size); break;
    }
}

// =============================================================================
// Layout:
// =============================================================================
static Void compute_standalone_sizes (ArrayUiBox *boxes, U64 axis) {
    array_iter (box, boxes) {
        Auto size = &box->style.size.v[axis];
        if (size->tag == UI_SIZE_PIXELS) box->rect.size[axis] = size->value;
    }
}

static Void compute_downward_dependent_sizes (ArrayUiBox *boxes, U64 axis) {
    array_iter_back (box, boxes) {
        Auto size = &box->style.size.v[axis];
        if (size->tag != UI_SIZE_CHILDREN_SUM && size->tag != UI_SIZE_CUSTOM) continue;

        array_iter (child, &box->children) {
            if (child->style.size.v[axis].tag == UI_SIZE_PCT_PARENT && size->tag == UI_SIZE_CHILDREN_SUM) {
                // Cycle: parent defined by child and child defined by parent.
                size->tag = UI_SIZE_PCT_PARENT;
                size->value = 1;
                break;
            }
        }

        if (size->tag == UI_SIZE_PCT_PARENT) {
            continue;
        } else if (size->tag == UI_SIZE_CUSTOM) {
            box->size_fn(box, axis);
        } else {
            F32 final_size = 2*box->style.padding.v[axis];
            if (box->style.axis == axis) {
                array_iter (child, &box->children) {
                    if (! isnan(child->style.floating[axis])) continue;
                    final_size += child->rect.size[axis];
                    if (! ARRAY_ITER_DONE) final_size += box->style.spacing;
                }
            } else {
                array_iter (child, &box->children) {
                    if (! isnan(child->style.floating[axis])) continue;
                    final_size = max(final_size, child->rect.size[axis] + 2*box->style.padding.v[axis]);
                }
            }

            box->rect.size[axis] = final_size;
        }
    }
}

static Void compute_upward_dependent_sizes (ArrayUiBox *boxes, U64 axis) {
    array_iter (box, boxes) {
        Auto size = &box->style.size.v[axis];
        if (size->tag == UI_SIZE_PCT_PARENT) box->rect.size[axis] = size->value * (box->parent->rect.size[axis] - 2*box->parent->style.padding.v[axis]);
    }
}

static Void fix_overflow (ArrayUiBox *boxes, U64 axis) {
    array_iter (box, boxes) {
        F32 box_size = box->rect.size[axis] - 2*box->style.padding.v[axis];

        if (box->style.axis == axis) {
            F32 children_size = 0;
            array_iter (child, &box->children) {
                if (! isnan(child->style.floating[axis])) continue;
                children_size += child->rect.size[axis];
                if (! ARRAY_ITER_DONE) children_size += box->style.spacing;
            }

            if (box_size < children_size && !box->style.overflow[axis]) {
                F32 overflow = children_size - box_size;

                F32 total_slack = 0;
                array_iter (child, &box->children) {
                    if (! isnan(child->style.floating[axis])) continue;
                    total_slack += child->rect.size[axis] * (1 - child->style.size.v[axis].strictness);
                }

                if (total_slack >= overflow) {
                    F32 slack_fraction = overflow / total_slack;
                    array_iter (child, &box->children) {
                        if (! isnan(child->style.floating[axis])) continue;
                        child->rect.size[axis] -= child->rect.size[axis] * (1 - child->style.size.v[axis].strictness) * slack_fraction;
                    }
                }
            }
        } else {
            array_iter (child, &box->children) {
                if (! isnan(child->style.floating[axis])) continue;
                if (box_size >= child->rect.size[axis]) continue;
                if (box->style.overflow[axis]) continue;
                F32 overflow = child->rect.size[axis] - box_size;
                F32 slack = child->rect.size[axis] * (1 - child->style.size.v[axis].strictness);
                if (slack >= overflow) child->rect.size[axis] -= overflow;
            }
        }
    }
}

static Void compute_positions (ArrayUiBox *boxes, U64 axis) {
    array_iter (box, boxes) {
        if (box->style.axis == axis) {
            F32 content_size = 2*box->style.padding.v[axis];
            array_iter (child, &box->children) {
                if (! isnan(child->style.floating[axis])) continue;
                content_size += child->rect.size[axis];
                if (! ARRAY_ITER_DONE) content_size += box->style.spacing;
            }

            box->content.size[axis] = floor(content_size);

            F32 align_offset = 0;
            switch (box->style.align[axis]) {
            case UI_ALIGN_START:  break;
            case UI_ALIGN_MIDDLE: align_offset = floor(box->rect.size[axis]/2 - content_size/2); break;
            case UI_ALIGN_END:    align_offset = box->rect.size[axis] - content_size; break;
            }

            F32 pos = box->rect.top_left.v[axis] + box->style.padding.v[axis] + align_offset + box->content.top_left.v[axis];
            array_iter (child, &box->children) {
                if (isnan(child->style.floating[axis])) {
                    child->rect.top_left.v[axis] = pos;
                    pos += child->rect.size[axis] + box->style.spacing;
                } else {
                    child->rect.top_left.v[axis] = box->rect.top_left.v[axis] + child->style.floating[axis];
                }
            }
        } else {
            box->content.size[axis] = 0;

            array_iter (child, &box->children) {
                if (isnan(child->style.floating[axis])) {
                    F32 content_size = child->rect.size[axis] + 2*box->style.padding.v[axis];

                    box->content.size[axis] = floor(max(box->content.size[axis], content_size));

                    F32 align_offset = 0;
                    switch (box->style.align[axis]) {
                    case UI_ALIGN_START:  break;
                    case UI_ALIGN_MIDDLE: align_offset = floor(box->rect.size[axis]/2 - content_size/2); break;
                    case UI_ALIGN_END:    align_offset = box->rect.size[axis] - content_size; break;
                    }

                    child->rect.top_left.v[axis] = box->rect.top_left.v[axis] + box->style.padding.v[axis] + align_offset + box->content.top_left.v[axis];
                } else {
                    child->rect.top_left.v[axis] = box->rect.top_left.v[axis] + child->style.floating[axis];
                }
            }
        }

        array_iter (child, &box->children) {
            child->rect.x = round(child->rect.x);
            child->rect.y = round(child->rect.y);
            child->rect.w = round(child->rect.w);
            child->rect.h = round(child->rect.h);
        }
    }
}

static Void collect_nodes_dfs (UiBox *box, ArrayUiBox *out) {
    array_push(out, box);
    array_iter (child, &box->children) collect_nodes_dfs(child, out);
}

static Void compute_layout (UiBox *box) {
    ArrayUiBox nodes;

    if (box == ui->root) {
        nodes = ui->depth_first;
    } else {
        array_init(&nodes, ui->frame_mem);
        collect_nodes_dfs(box, &nodes);
    }

    for (U64 axis = 0; axis < 2; ++axis) {
        compute_standalone_sizes(&nodes, axis);
        compute_downward_dependent_sizes(&nodes, axis);
        compute_upward_dependent_sizes(&nodes, axis);
        fix_overflow(&nodes, axis);
        compute_positions(&nodes, axis);
    }
}

static Void find_topmost_hovered_box (UiBox *box) {
    if (! (box->flags & UI_BOX_CLICK_THROUGH)) {
        UiRect r = compute_rect_intersect(box->rect, array_get_last(&ui->clip_stack));
        if (within_box(r, ui->mouse)) ui->hovered = box;
    }

    if (box->flags & UI_BOX_CLIPPING) ui_push_clip(box, true);
    array_iter (child, &box->children) find_topmost_hovered_box(child);
    if (box->flags & UI_BOX_CLIPPING) ui_pop_clip();
}

static Void draw_box (UiBox *box) {
    if (!(box->flags & UI_BOX_INVISIBLE) && box->style.blur_radius) {
        flush_vertices();
        glScissor(0, 0, win_width, win_height);

        F32 blur_radius = max(1, cast(Int, box->style.blur_radius));

        glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, blur_buffer1);
        glBlitFramebuffer(0, 0, win_width, win_height, 0, 0, win_width/BLUR_SHRINK, win_height/BLUR_SHRINK, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glViewport(0, 0, win_width/BLUR_SHRINK, win_height/BLUR_SHRINK);

        blur_vertices.count = 0;
        array_push_lit(&blur_vertices, -1.0f,  1.0f);
        array_push_lit(&blur_vertices, -1.0f, -1.0f);
        array_push_lit(&blur_vertices,  1.0f, -1.0f);
        array_push_lit(&blur_vertices, -1.0f,  1.0f);
        array_push_lit(&blur_vertices,  1.0f, -1.0f);
        array_push_lit(&blur_vertices,  1.0f,  1.0f);

        glBindVertexArray(blur_VAO);
        glBindBuffer(GL_ARRAY_BUFFER, blur_VBO);
        ATTR(AElem(&blur_vertices), 0, 2, pos);
        glBufferData(GL_ARRAY_BUFFER, array_size(&blur_vertices), blur_vertices.data, GL_STREAM_DRAW);

        glUseProgram(blur_shader);
        set_int(blur_shader, "blur_radius", blur_radius);
        set_bool(blur_shader, "do_blurring", true);
        set_mat4(blur_shader, "projection", mat4(1));

        for (U64 i = 0; i < 3; ++i) {
            glBindFramebuffer(GL_FRAMEBUFFER, blur_buffer2);
            glBindTexture(GL_TEXTURE_2D, blur_tex1);
            set_bool(blur_shader, "horizontal", true);
            glDrawArrays(GL_TRIANGLES, 0, blur_vertices.count);

            glBindFramebuffer(GL_FRAMEBUFFER, blur_buffer1);
            glBindTexture(GL_TEXTURE_2D, blur_tex2);
            set_bool(blur_shader, "horizontal", false);
            glDrawArrays(GL_TRIANGLES, 0, blur_vertices.count);
        }

        glViewport(0, 0, win_width, win_height);
        glBindTexture(GL_TEXTURE_2D, blur_tex1);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

        UiRect r = box->rect;
        r.y = win_height - r.y;
        blur_vertices.count = 0;
        array_push_lit(&blur_vertices, r.x, r.y);
        array_push_lit(&blur_vertices, r.x+r.w, r.y);
        array_push_lit(&blur_vertices, r.x, r.y-r.h);
        array_push_lit(&blur_vertices, r.x, r.y-r.h);
        array_push_lit(&blur_vertices, r.x+r.w, r.y);
        array_push_lit(&blur_vertices, r.x+r.w, r.y-r.h);

        set_mat4(blur_shader, "projection", projection);
        set_bool(blur_shader, "do_blurring", false);
        set_vec2(blur_shader, "half_size", vec2(r.w/2, r.h/2));
        set_vec2(blur_shader, "center", vec2(r.x+r.w/2, r.y-r.h/2));
        set_vec4(blur_shader, "radius", box->style.radius);
        set_float(blur_shader, "blur_shrink", BLUR_SHRINK);

        glBufferData(GL_ARRAY_BUFFER, array_size(&blur_vertices), blur_vertices.data, GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, blur_vertices.count);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        r = array_get_last(&ui->clip_stack);
        glScissor(r.x, win_height - r.y - r.h, r.w, r.h);
    }

    if (! (box->flags & UI_BOX_INVISIBLE)) draw_rect(
        .top_left            = box->rect.top_left,
        .bottom_right        = vec2(box->rect.x + box->rect.w, box->rect.y + box->rect.h),
        .color               = box->style.bg_color,
        .color2              = box->style.bg_color2,
        .radius              = box->style.radius,
        .edge_softness       = box->style.edge_softness,
        .border_color        = box->style.border_color,
        .border_widths       = box->style.border_widths,
        .inset_shadow_color  = box->style.inset_shadow_color,
        .outset_shadow_color = box->style.outset_shadow_color,
        .inset_shadow_width  = box->style.inset_shadow_width,
        .outset_shadow_width = box->style.outset_shadow_width,
        .shadow_offsets      = box->style.shadow_offsets,
    );

    if (box->flags & UI_BOX_CLIPPING) {
        flush_vertices();
        UiRect r = ui_push_clip(box, true);
        glScissor(r.x, win_height - r.y - r.h, r.w, r.h);
    }

    array_push(&ui->box_stack, box); // For use_style_var_get().
    if (box->draw_fn) box->draw_fn(box);
    array_iter (c, &box->children) draw_box(c);
    array_pop(&ui->box_stack);

    if (box->flags & UI_BOX_CLIPPING) {
        flush_vertices();
        UiRect r = ui_pop_clip();
        glScissor(r.x, win_height - r.y - r.h, r.w, r.h);
    }
}

// =============================================================================
// Widgets:
// =============================================================================
static UiBox *ui_hspacer () {
    UiBox *box = ui_box(UI_BOX_INVISIBLE, "hspacer") { ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 0}); }
    return box;
}

static UiBox *ui_vspacer () {
    UiBox *box = ui_box(UI_BOX_INVISIBLE, "vspacer") { ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PCT_PARENT, 1, 0}); }
    return box;
}

static Void size_label (UiBox *box, U64 axis) {
    // Sizing done in the draw_label function.
}

static Void draw_label (UiBox *box) {
    if (! set_font(box)) return;

    tmem_new(tm);

    glBindTexture(GL_TEXTURE_2D, ui->font->atlas_texture);

    Bool first_frame     = box->start_frame == ui->frame;
    String text          = str(cast(CString, box->scratch));
    F32 x                = round(box->rect.x + box->style.padding.x);
    F32 y                = round(box->rect.y + box->rect.h - box->style.padding.y);
    U32 line_width       = 0;
    F32 descent          = cast(F32, ui->font->descent);
    F32 width            = cast(F32, ui->font->width);
    F32 x_pos            = x;
    SliceGlyphInfo infos = font_get_glyph_infos(ui->font, tm, text);

    array_iter (info, &infos, *) {
        AtlasSlot *slot = font_get_atlas_slot(ui->font, info);
        Vec2 top_left = {
            ui->font->is_mono ? (x_pos + slot->bearing_x) : (x + info->x + slot->bearing_x),
            y + info->y - descent - slot->bearing_y
        };
        Vec2 bottom_right = {top_left.x + slot->width, top_left.y + slot->height};

        draw_rect(
            .top_left          = top_left,
            .bottom_right      = bottom_right,
            .texture_rect      = {slot->x, slot->y, slot->width, slot->height},
            .text_color        = first_frame ? vec4(0,0,0,0) : box->style.text_color,
            .text_is_grayscale = (slot->pixel_mode == FT_PIXEL_MODE_GRAY),
        );

        x_pos += width;
        if (ARRAY_ITER_DONE) line_width = ui->font->is_mono ? (x_pos - x) : (info->x + slot->bearing_x + info->x_advance);
    }

    box->rect.w = line_width + 2*box->style.padding.x;
    box->rect.h = ui->font->height + 2*box->style.padding.y;
}

static UiBox *ui_label (CString id, String label) {
    UiBox *box = ui_box_str(UI_BOX_CLICK_THROUGH, str(id)) {
        Font *font = ui_config_get_font(UI_CONFIG_FONT_NORMAL);
        ui_style_font(UI_FONT, font);
        ui_style_f32(UI_FONT_SIZE, font->size);
        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_CUSTOM, 1, 1});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_CUSTOM, 1, 1});
        box->size_fn = size_label;
        box->draw_fn = draw_label;
        box->scratch = cast(U64, cstr(ui->frame_mem, label));
    }

    return box;
}

static UiBox *ui_icon (CString id, U32 size, U32 icon) {
    UiBox *label = ui_label(id, str_utf32_to_utf8(ui->frame_mem, icon));
    ui_style_box_from_config(label, UI_FONT, UI_CONFIG_FONT_ICONS);
    ui_style_box_f32(label, UI_FONT_SIZE, size);
    return label;
}

static UiBox *ui_checkbox (CString id, Bool *val) {
    UiBox *bg = ui_box(UI_BOX_REACTIVE|UI_BOX_CAN_FOCUS, id) {
        F32 s = 20;

        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, s, 1});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, s, 1});
        ui_style_from_config(UI_RADIUS, UI_CONFIG_RADIUS_1);
        ui_style_u32(UI_ALIGN_X, UI_ALIGN_MIDDLE);
        ui_style_u32(UI_ALIGN_Y, UI_ALIGN_MIDDLE);
        ui_style_from_config(UI_BG_COLOR, UI_CONFIG_BG_3);
        ui_style_from_config(UI_BORDER_COLOR, UI_CONFIG_BORDER_1_COLOR);
        ui_style_from_config(UI_BORDER_WIDTHS, UI_CONFIG_BORDER_1_WIDTH);
        ui_style_from_config(UI_INSET_SHADOW_WIDTH, UI_CONFIG_IN_SHADOW_1_WIDTH);
        ui_style_from_config(UI_INSET_SHADOW_COLOR, UI_CONFIG_IN_SHADOW_1_COLOR);
        ui_style_u32(UI_ANIMATION, UI_MASK_BG_COLOR);

        ui_style_rule(".focus") {
            ui_style_from_config(UI_BORDER_WIDTHS, UI_CONFIG_BORDER_FOCUS_WIDTH);
            ui_style_from_config(UI_BORDER_COLOR, UI_CONFIG_BORDER_FOCUS_COLOR);
        }

        if (*val) {
            ui_style_from_config(UI_BG_COLOR, UI_CONFIG_MAGENTA_1);
            ui_style_from_config(UI_BORDER_COLOR, UI_CONFIG_BORDER_2_COLOR);
            ui_icon("mark", 16, get_icon(ICON_CHECK));
        } else {
            ui_style_from_config(UI_BG_COLOR, UI_CONFIG_BG_3);
        }

        if (bg->signals.clicked) {
            *val = !*val;
        }
    }

    return bg;
}

istruct (UiImage) {
    Image *image;
    Bool blur;
    Vec4 tint;
    F32 pref_width;
};

static Void draw_image (UiBox *box) {
    Auto info = cast(UiImage *, box->scratch);
    flush_vertices();
    glBindTexture(GL_TEXTURE_2D, info->image->texture);
    draw_rect(
        .top_left          = box->rect.top_left,
        .bottom_right      = {box->rect.x + box->rect.w, box->rect.y + box->rect.h},
        .radius            = box->style.radius,
        .texture_rect      = {0, 0, info->image->width, info->image->height},
        .text_color        = (info->tint.w > 0) ? info->tint : vec4(1, 1, 1, 1),
        .text_is_grayscale = (info->tint.w > 0) ? 1 : 0,
    );
}

static UiBox *ui_image (CString id, Image *image, Bool blur, Vec4 tint, F32 pref_width) {
    UiBox *img = ui_box(UI_BOX_INVISIBLE, id) {
        img->draw_fn = draw_image;
        UiImage *info = mem_new(ui->frame_mem, UiImage);
        info->image = image;
        info->blur = blur;
        info->tint = tint;
        info->pref_width = pref_width;
        img->scratch = cast(U64, info);
        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, info->pref_width, 1});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, round(image->height * (info->pref_width / image->width)), 1});
        ui_style_from_config(UI_RADIUS, UI_CONFIG_RADIUS_2);

        ui_box(0, "overlay") {
            ui_style_size(UI_WIDTH, (UiSize){ UI_SIZE_PCT_PARENT, 1, 1});
            ui_style_size(UI_HEIGHT, (UiSize){ UI_SIZE_PCT_PARENT, 1, 1});
            ui_style_vec4(UI_RADIUS, img->style.radius);
            if (info->blur) ui_style_from_config(UI_BLUR_RADIUS, UI_CONFIG_BLUR);
        }
    }

    return img;
}

static UiBox *ui_toggle (CString id, Bool *val) {
    UiBox *bg = ui_box(UI_BOX_REACTIVE|UI_BOX_CAN_FOCUS, id) {
        F32 s = 24.0;

        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, 2*s, 1});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, s, 1});
        ui_style_vec4(UI_RADIUS, vec4(s/2, s/2, s/2, s/2));
        ui_style_from_config(UI_BG_COLOR, UI_CONFIG_BG_3);
        ui_style_from_config(UI_BORDER_COLOR, UI_CONFIG_BORDER_1_COLOR);
        ui_style_from_config(UI_BORDER_WIDTHS, UI_CONFIG_BORDER_1_WIDTH);
        ui_style_from_config(UI_INSET_SHADOW_WIDTH, UI_CONFIG_IN_SHADOW_1_WIDTH);
        ui_style_from_config(UI_INSET_SHADOW_COLOR, UI_CONFIG_IN_SHADOW_1_COLOR);
        ui_style_u32(UI_ANIMATION, UI_MASK_BG_COLOR);

        ui_style_rule(".focus") {
            ui_style_from_config(UI_BORDER_WIDTHS, UI_CONFIG_BORDER_FOCUS_WIDTH);
            ui_style_from_config(UI_BORDER_COLOR, UI_CONFIG_BORDER_FOCUS_COLOR);
        }

        if (*val) {
            ui_style_from_config(UI_BG_COLOR, UI_CONFIG_MAGENTA_1);
            ui_style_from_config(UI_BORDER_COLOR, UI_CONFIG_BORDER_2_COLOR);
        } else {
            ui_style_from_config(UI_BG_COLOR, UI_CONFIG_BG_3);
        }

        if (bg->signals.clicked) {
            *val = !*val;
        }

        ui_box(UI_BOX_CLICK_THROUGH, "toggle_knob") {
            F32 ks = 16.0;
            ui_style_f32(UI_EDGE_SOFTNESS, 1.3);
            ui_style_f32(UI_FLOAT_X, *val ? (2*s - ks - 4) : 4);
            ui_style_f32(UI_FLOAT_Y, 4);
            ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, ks, 1});
            ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, ks, 1});
            ui_style_vec4(UI_RADIUS, vec4(ks/2, ks/2, ks/2, ks/2));
            ui_style_from_config(UI_BG_COLOR, UI_CONFIG_FG_1);
            ui_style_from_config(UI_BG_COLOR2, UI_CONFIG_FG_2);
            ui_style_from_config(UI_OUTSET_SHADOW_WIDTH, UI_CONFIG_SHADOW_1_WIDTH);
            ui_style_from_config(UI_OUTSET_SHADOW_COLOR, UI_CONFIG_SHADOW_1_COLOR);
            ui_style_u32(UI_ANIMATION, UI_MASK_FLOAT_X);
            ui_style_from_config(UI_ANIMATION_TIME, UI_CONFIG_ANIMATION_TIME_1);
        }
    }

    return bg;
}

static UiBox *ui_button_str (String id, String label) {
    UiBox *button = ui_box_str(UI_BOX_REACTIVE|UI_BOX_CAN_FOCUS, id) {
        ui_tag("button");
        ui_style_u32(UI_ALIGN_Y, UI_ALIGN_MIDDLE);
        ui_style_u32(UI_ALIGN_X, UI_ALIGN_MIDDLE);
        ui_style_from_config(UI_BG_COLOR, UI_CONFIG_FG_3);
        ui_style_from_config(UI_BG_COLOR2, UI_CONFIG_FG_4);
        ui_style_from_config(UI_RADIUS, UI_CONFIG_RADIUS_1);
        ui_style_from_config(UI_OUTSET_SHADOW_WIDTH, UI_CONFIG_SHADOW_1_WIDTH);
        ui_style_from_config(UI_OUTSET_SHADOW_COLOR, UI_CONFIG_SHADOW_1_COLOR);
        ui_style_from_config(UI_PADDING, UI_CONFIG_PADDING_1);
        ui_style_vec2(UI_SHADOW_OFFSETS, vec2(0, -1));

        ui_style_rule(".button.focus") {
            ui_style_from_config(UI_BORDER_WIDTHS, UI_CONFIG_BORDER_FOCUS_WIDTH);
            ui_style_from_config(UI_BORDER_COLOR, UI_CONFIG_BORDER_FOCUS_COLOR);
        }

        ui_style_rule(".button.press") {
            ui_style_f32(UI_OUTSET_SHADOW_WIDTH, 0);
            ui_style_from_config(UI_BG_COLOR, UI_CONFIG_FG_4);
            ui_style_from_config(UI_BG_COLOR2, UI_CONFIG_FG_3);
            ui_style_from_config(UI_INSET_SHADOW_WIDTH, UI_CONFIG_IN_SHADOW_1_WIDTH);
            ui_style_from_config(UI_INSET_SHADOW_COLOR, UI_CONFIG_IN_SHADOW_1_COLOR);
        }

        if (button->signals.hovered) {
            ui_push_clip(button, true);
            ui_box(UI_BOX_CLICK_THROUGH, "button_highlight") {
                F32 s = button->rect.h/8;
                ui_style_f32(UI_EDGE_SOFTNESS, 60);
                ui_style_vec4(UI_RADIUS, vec4(s, s, s, s));
                ui_style_f32(UI_FLOAT_X, ui->mouse.x - button->rect.x - s);
                ui_style_f32(UI_FLOAT_Y, ui->mouse.y - button->rect.y - s);
                ui_style_from_config(UI_BG_COLOR, UI_CONFIG_HIGHLIGHT);
                ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, 2*s, 1});
                ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, 2*s, 1});
            }
            ui_pop_clip();
        }

        UiBox *label_box = ui_label("button_label", label);
        Font *font = ui_config_get_font(UI_CONFIG_FONT_MONO);
        ui_style_box_font(label_box, UI_FONT, font);
        ui_style_box_f32(label_box, UI_FONT_SIZE, font->size);
    }

    return button;
}

static UiBox *ui_button (CString id) {
    return ui_button_str(str(id), str(id));
}

static UiBox *ui_vscroll_bar (String label, UiRect rect, F32 ratio, F32 *val) {
    UiBox *container = ui_box_str(UI_BOX_REACTIVE, label) {
        ui_style_f32(UI_FLOAT_X, rect.x);
        ui_style_f32(UI_FLOAT_Y, rect.y);
        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_CHILDREN_SUM, 0, 1});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, rect.h, 0});
        ui_style_from_config(UI_BG_COLOR, UI_CONFIG_BG_3);
        ui_style_u32(UI_AXIS, UI_AXIS_VERTICAL);
        ui_style_f32(UI_EDGE_SOFTNESS, 0);

        ui_style_rule(".hover #scroll_bar_knob") { ui_style_from_config(UI_BG_COLOR, UI_CONFIG_FG_1); }
        ui_style_rule(".press #scroll_bar_knob") { ui_style_from_config(UI_BG_COLOR, UI_CONFIG_FG_1); }
        ui_style_rule("#scroll_bar_knob.hover")  { ui_style_from_config(UI_BG_COLOR, UI_CONFIG_FG_1); }
        ui_style_rule("#scroll_bar_knob.press")  { ui_style_from_config(UI_BG_COLOR, UI_CONFIG_FG_1); }

        F32 knob_size = round(rect.h * ratio);

        if (container->signals.pressed) {
            *val = ui->mouse.y - container->rect.y - knob_size/2;
            *val = clamp(*val, 0, rect.h - knob_size);
        }

        if (container->signals.hovered && (ui->event->tag == EVENT_SCROLL)) {
            *val -= (25 * ui->event->y);
            *val = clamp(*val, 0, rect.h - knob_size);
            ui_eat_event();
        }

        ui_box(UI_BOX_CLICK_THROUGH|UI_BOX_INVISIBLE, "scroll_bar_spacer") {
            ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, *val, 0});
        }

        UiBox *knob = ui_box(UI_BOX_REACTIVE, "scroll_bar_knob") {
            ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, rect.w, 1});
            ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, knob_size, 1});
            ui_style_from_config(UI_BG_COLOR, UI_CONFIG_FG_2);
            ui_style_f32(UI_EDGE_SOFTNESS, 0);

            if (knob->signals.pressed && ui->event->tag == EVENT_MOUSE_MOVE) {
                *val += ui->mouse_dt.y;
                *val = clamp(*val, 0, rect.h - knob_size);
            }
        }
    }

    return container;
}

static UiBox *ui_hscroll_bar (String label, UiRect rect, F32 ratio, F32 *val) {
    UiBox *container = ui_box_str(UI_BOX_REACTIVE, label) {
        ui_style_f32(UI_FLOAT_X, rect.x);
        ui_style_f32(UI_FLOAT_Y, rect.y);
        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, rect.w, 1});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_CHILDREN_SUM, 0, 1});
        ui_style_from_config(UI_BG_COLOR, UI_CONFIG_BG_3);
        ui_style_u32(UI_AXIS, UI_AXIS_HORIZONTAL);
        ui_style_f32(UI_EDGE_SOFTNESS, 0);

        ui_style_rule(".hover #scroll_bar_knob") { ui_style_from_config(UI_BG_COLOR, UI_CONFIG_FG_1); }
        ui_style_rule(".press #scroll_bar_knob") { ui_style_from_config(UI_BG_COLOR, UI_CONFIG_FG_1); }
        ui_style_rule("#scroll_bar_knob.hover")  { ui_style_from_config(UI_BG_COLOR, UI_CONFIG_FG_1); }
        ui_style_rule("#scroll_bar_knob.press")  { ui_style_from_config(UI_BG_COLOR, UI_CONFIG_FG_1); }

        F32 knob_size = rect.w * ratio;

        if (container->signals.pressed) {
            *val = ui->mouse.x - container->rect.x - knob_size/2;
            *val = clamp(*val, 0, rect.w - knob_size);
        }

        if (container->signals.hovered && (ui->event->tag == EVENT_SCROLL)) {
            *val -= (25 * ui->event->y);
            *val = clamp(*val, 0, rect.w - knob_size);
            ui_eat_event();
        }

        ui_box(UI_BOX_CLICK_THROUGH|UI_BOX_INVISIBLE, "scroll_bar_spacer") {
            ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, *val, 0});
        }

        UiBox *knob = ui_box(UI_BOX_REACTIVE, "scroll_bar_knob") {
            ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, rect.h, 1});
            ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, knob_size, 1});
            ui_style_from_config(UI_BG_COLOR, UI_CONFIG_FG_2);
            ui_style_f32(UI_EDGE_SOFTNESS, 0);

            if (knob->signals.pressed && ui->event->tag == EVENT_MOUSE_MOVE) {
                *val += ui->mouse_dt.x;
                *val = clamp(*val, 0, rect.w - knob_size);
            }
        }
    }

    return container;
}

static UiBox *ui_scroll_box_push (String label) {
    UiBox *container = ui_box_push_str(UI_BOX_REACTIVE, label);
    ui_style_box_u32(container, UI_OVERFLOW_X, true);
    ui_style_box_u32(container, UI_OVERFLOW_Y, true);
    container->scratch = ui->depth_first.count-1;
    ui_push_clip(container, true);
    return container;
}

static Void ui_scroll_box_pop () {
    UiBox *container = array_get_last(&ui->box_stack);

    Bool contains_focused = (ui->focus_idx >= container->scratch);
    if (contains_focused && ui->event->tag == EVENT_KEY_PRESS && ui->event->key == SDLK_TAB) {
        F32 fx1 = ui->focused->rect.x + ui->focused->rect.w;
        F32 cx1 = container->rect.x + container->rect.w;
        if (fx1 > cx1) {
            container->content.x -= fx1 - cx1;
        } else if (ui->focused->rect.x < container->rect.x) {
            container->content.x += container->rect.x - ui->focused->rect.x;
        }

        F32 fy1 = ui->focused->rect.y + ui->focused->rect.h;
        F32 cy1 = container->rect.y + container->rect.h;
        if (fy1 > cy1) {
            container->content.y -= fy1 - cy1;
        } else if (ui->focused->rect.y < container->rect.y) {
            container->content.y += container->rect.y - ui->focused->rect.y;
        }
    }

    F32 speed = 25;
    F32 bar_width = 8;

    if (container->rect.w < container->content.w) {
        F32 scroll_val = (fabs(container->content.x) / container->content.w) * container->rect.w;
        F32 ratio = container->rect.w / container->content.w;
        ui_hscroll_bar(str("scroll_bar_x"), (UiRect){0, container->rect.h - bar_width, container->rect.w, bar_width}, ratio, &scroll_val);
        container->content.x = -(scroll_val/container->rect.w*container->content.w);

        if (container->signals.hovered && (ui->event->tag == EVENT_SCROLL) && is_key_pressed(SDLK_LCTRL)) {
            container->content.x += speed * ui->event->y;
            ui_eat_event();
        }

        container->content.x = clamp(container->content.x, -(container->content.w - container->rect.w), 0);
    } else {
        container->content.x = 0;
    }

    if (container->rect.h < container->content.h) {
        F32 scroll_val = (fabs(container->content.y) / container->content.h) * container->rect.h;
        F32 ratio = container->rect.h / container->content.h;
        ui_vscroll_bar(str("scroll_bar_y"), (UiRect){container->rect.w - bar_width, 0, bar_width, container->rect.h}, ratio, &scroll_val);
        container->content.y = -(scroll_val/container->rect.h*container->content.h);

        if (container->signals.hovered && (ui->event->tag == EVENT_SCROLL) && !is_key_pressed(SDLK_LCTRL)) {
            container->content.y += speed * ui->event->y;
            ui_eat_event();
        }

        container->content.y = clamp(container->content.y, -(container->content.h - container->rect.h), 0);
    } else {
        container->content.y = 0;
    }

    ui_pop_clip();
    ui_pop_parent();
}

static Void ui_scroll_box_pop_ (Void *) {
    ui_scroll_box_pop();
}

#define ui_scroll_box(LABEL)\
    ui_scroll_box_push(str(LABEL));\
    if (cleanup(ui_scroll_box_pop_) U8 _; 1)

istruct (UiPopup) {
    Bool *shown;
    Bool sideways;
    UiBox *anchor;
};

static Void size_popup (UiBox *popup, U64 axis) {
    F32 size = 0;
    Bool cycle = false;

    array_iter(child, &popup->children) {
        if (child->style.size.v[axis].tag == UI_SIZE_PCT_PARENT) cycle = true;
        size += child->rect.size[axis];
    }

    if (cycle) {
        popup->rect.size[axis] = round(ui->root->rect.size[axis] / 2);
    } else {
        popup->rect.size[axis] = min(size + 2 * popup->style.padding.v[axis], ui->root->rect.size[axis] - 20.0);
    }
}

static Void layout_popup (UiBox *popup) {
    UiPopup *info = cast(UiPopup*, popup->scratch);
    UiRect anchor = info->anchor->rect;
    UiRect viewport = ui->root->rect;
    F32 popup_w = popup->rect.w;
    F32 popup_h = popup->rect.h;
    F32 margin = 6.0f;

    F32 space_left   = anchor.x - viewport.x;
    F32 space_right  = (viewport.x + viewport.w) - (anchor.x + anchor.w);
    F32 space_top    = anchor.y - viewport.y;
    F32 space_bottom = (viewport.y + viewport.h) - (anchor.y + anchor.h);

    // @todo Due to the complex layout logic of the popup which relies
    // on the size information from previous frames, we have to delay
    // drawing the popup for the first two frames in order to prevent
    // nasty flickering... We have to do this on top of also having to
    // use the deferred_layout_fns feature for popups...
    popup->flags &= ~UI_BOX_INVISIBLE;
    if (ui->frame - popup->start_frame < 2) popup->flags |= UI_BOX_INVISIBLE;

    enum { POPUP_LEFT, POPUP_RIGHT, POPUP_TOP, POPUP_BOTTOM } side;

    if (info->sideways) {
        side = space_left > space_right ? POPUP_LEFT : POPUP_RIGHT;
    } else {
        side = space_top > space_bottom ? POPUP_TOP : POPUP_BOTTOM;
    }

    F32 x = 0;
    F32 y = 0;

    switch (side) {
    case POPUP_RIGHT:
        x = anchor.x + anchor.w + margin;
        y = anchor.y + (anchor.h - popup_h) * 0.5f;
        break;
    case POPUP_LEFT:
        x = anchor.x - popup_w - margin;
        y = anchor.y + (anchor.h - popup_h) * 0.5f;
        break;
    case POPUP_BOTTOM:
        x = anchor.x + (anchor.w - popup_w) * 0.5f;
        y = anchor.y + anchor.h + margin;
        break;
    case POPUP_TOP:
        x = anchor.x + (anchor.w - popup_w) * 0.5f;
        y = anchor.y - popup_h - margin;
        break;
    }

    x = clamp(x, 0, viewport.w - popup_w);
    y = clamp(y, 0, viewport.h - popup_h);

    ui_style_box_f32(popup, UI_FLOAT_X, x);
    ui_style_box_f32(popup, UI_FLOAT_Y, y);
}

static UiBox *ui_popup_push (String id, Bool *shown, Bool sideways, UiBox *anchor) {
    ui_push_parent(ui->root);
    ui_push_clip(ui->root, false);

    UiBox *overlay = ui_box_push_str(UI_BOX_REACTIVE, id);
    ui_style_box_f32(overlay, UI_FLOAT_X, 0);
    ui_style_box_f32(overlay, UI_FLOAT_Y, 0);
    ui_style_box_size(overlay, UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});
    ui_style_box_size(overlay, UI_HEIGHT, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});

    *shown = true;
    if ((ui->event->tag == EVENT_KEY_PRESS) && (ui->event->key == SDLK_ESCAPE)) *shown = false;
    if (overlay->signals.clicked && ui->event->key == SDL_BUTTON_LEFT) *shown = false;

    UiBox *popup = ui_scroll_box_push(str("popup"));
    popup->size_fn = size_popup;
    UiPopup *info = mem_new(ui->frame_mem, UiPopup);
    info->sideways = sideways;
    info->shown = shown;
    info->anchor = anchor;
    popup->scratch = cast(U64, info);
    array_push_lit(&ui->deferred_layout_fns, layout_popup, popup);
    ui_style_box_size(popup, UI_WIDTH, (UiSize){UI_SIZE_CUSTOM, 1, 0});
    ui_style_box_size(popup, UI_HEIGHT, (UiSize){UI_SIZE_CUSTOM, 1, 0});
    ui_style_box_from_config(popup, UI_BG_COLOR, UI_CONFIG_BG_4);
    ui_style_box_from_config(popup, UI_RADIUS, UI_CONFIG_RADIUS_2);
    ui_style_box_from_config(popup, UI_PADDING, UI_CONFIG_PADDING_1);
    ui_style_box_from_config(popup, UI_BORDER_COLOR, UI_CONFIG_BORDER_1_COLOR);
    ui_style_box_from_config(popup, UI_BORDER_WIDTHS, UI_CONFIG_BORDER_1_WIDTH);
    ui_style_box_from_config(popup, UI_OUTSET_SHADOW_WIDTH, UI_CONFIG_SHADOW_1_WIDTH);
    ui_style_box_from_config(popup, UI_OUTSET_SHADOW_COLOR, UI_CONFIG_SHADOW_1_COLOR);
    ui_style_box_from_config(popup, UI_ANIMATION_TIME, UI_CONFIG_ANIMATION_TIME_3);
    ui_style_box_u32(popup, UI_ANIMATION, UI_MASK_BG_COLOR);
    ui_style_from_config(UI_BLUR_RADIUS, UI_CONFIG_BLUR);

    return popup;
}

static Void ui_popup_pop () {
    ui_scroll_box_pop();
    ui_pop_parent();
    ui_pop_clip();
    ui_pop_parent();
}

static Void ui_popup_pop_ (Void *) {
    ui_popup_pop();
}

#define ui_popup(LABEL, SHOWN, SIDEWAYS, ANCHOR)\
    ui_popup_push(str(LABEL), SHOWN, SIDEWAYS, ANCHOR);\
    if (cleanup(ui_popup_pop_) U8 _; 1)

static Void size_modal (UiBox *modal, U64 axis) {
    F32 size = 0;
    Bool cycle = false;

    array_iter(child, &modal->children) {
        if (child->style.size.v[axis].tag == UI_SIZE_PCT_PARENT) cycle = true;
        size += child->rect.size[axis];
    }

    if (cycle) {
        modal->rect.size[axis] = ui->root->rect.size[axis] - 20.0;
    } else {
        modal->rect.size[axis] = min(size + 2 * modal->style.padding.v[axis], ui->root->rect.size[axis] - 20.0);
    }
}

static Void layout_modal (UiBox *modal) {
    ui_style_box_f32(modal, UI_FLOAT_X, ui->root->rect.w/2 - modal->rect.w/2);
    ui_style_box_f32(modal, UI_FLOAT_Y, ui->root->rect.h/2 - modal->rect.h/2);
}

static UiBox *ui_modal_push (String id, Bool *shown) {
    ui_push_parent(ui->root);
    ui_push_clip(ui->root, false);

    UiBox *overlay = ui_box_push_str(UI_BOX_REACTIVE, id);
    ui_style_box_f32(overlay, UI_FLOAT_X, 0);
    ui_style_box_f32(overlay, UI_FLOAT_Y, 0);
    ui_style_box_size(overlay, UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});
    ui_style_box_size(overlay, UI_HEIGHT, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});

    *shown = true;
    if ((ui->event->tag == EVENT_KEY_PRESS) && (ui->event->key == SDLK_ESCAPE)) *shown = false;
    if (overlay->signals.clicked && ui->event->key == SDL_BUTTON_LEFT) *shown = false;

    UiBox *modal = ui_scroll_box_push(str("modal"));
    modal->size_fn = size_modal;
    array_push_lit(&ui->deferred_layout_fns, layout_modal, modal);
    ui_style_box_size(modal, UI_WIDTH, (UiSize){UI_SIZE_CUSTOM, 1, 0});
    ui_style_box_size(modal, UI_HEIGHT, (UiSize){UI_SIZE_CUSTOM, 1, 0});
    ui_style_box_from_config(modal, UI_BG_COLOR, UI_CONFIG_BG_4);
    ui_style_box_from_config(modal, UI_RADIUS, UI_CONFIG_RADIUS_2);
    ui_style_box_from_config(modal, UI_PADDING, UI_CONFIG_PADDING_1);
    ui_style_box_from_config(modal, UI_BORDER_COLOR, UI_CONFIG_BORDER_1_COLOR);
    ui_style_box_from_config(modal, UI_BORDER_WIDTHS, UI_CONFIG_BORDER_1_WIDTH);
    ui_style_box_from_config(modal, UI_OUTSET_SHADOW_WIDTH, UI_CONFIG_SHADOW_1_WIDTH);
    ui_style_box_from_config(modal, UI_OUTSET_SHADOW_COLOR, UI_CONFIG_SHADOW_1_COLOR);
    ui_style_box_from_config(modal, UI_BLUR_RADIUS, UI_CONFIG_BLUR);

    return overlay;
}

static Void ui_modal_pop () {
    ui_scroll_box_pop();
    ui_pop_parent();
    ui_pop_clip();
    ui_pop_parent();
}

static Void ui_modal_pop_ (Void *) {
    ui_modal_pop();
}

#define ui_modal(LABEL, SHOWN)\
    ui_modal_push(str(LABEL), SHOWN);\
    if (cleanup(ui_modal_pop_) U8 _; 1)

static UiBox *ui_tooltip_push (String id) {
    ui_push_parent(ui->root);
    ui_push_clip(ui->root, false);

    UiBox *tooltip = ui_box_push_str(0, id);
    ui_style_box_f32(tooltip, UI_FLOAT_X, ui->mouse.x);
    ui_style_box_f32(tooltip, UI_FLOAT_Y, ui->mouse.y + 20);
    ui_style_box_from_config(tooltip, UI_BG_COLOR, UI_CONFIG_BG_4);
    ui_style_box_from_config(tooltip, UI_RADIUS, UI_CONFIG_RADIUS_2);
    ui_style_box_from_config(tooltip, UI_PADDING, UI_CONFIG_PADDING_1);
    ui_style_box_from_config(tooltip, UI_BORDER_COLOR, UI_CONFIG_BORDER_1_COLOR);
    ui_style_box_from_config(tooltip, UI_BORDER_WIDTHS, UI_CONFIG_BORDER_1_WIDTH);
    ui_style_box_from_config(tooltip, UI_OUTSET_SHADOW_WIDTH, UI_CONFIG_SHADOW_1_WIDTH);
    ui_style_box_from_config(tooltip, UI_OUTSET_SHADOW_COLOR, UI_CONFIG_SHADOW_1_COLOR);
    ui_style_box_from_config(tooltip, UI_BLUR_RADIUS, UI_CONFIG_BLUR);
    ui_style_box_from_config(tooltip, UI_ANIMATION_TIME, UI_CONFIG_ANIMATION_TIME_3);
    ui_style_box_u32(tooltip, UI_ANIMATION, UI_MASK_BG_COLOR);

    return tooltip;
}

static Void ui_tooltip_pop () {
    ui_pop_parent();
    ui_pop_clip();
    ui_pop_parent();
}

static Void ui_tooltip_pop_ (Void *) {
    ui_tooltip_pop();
}

#define ui_tooltip(LABEL)\
    ui_tooltip_push(str(LABEL));\
    if (cleanup(ui_tooltip_pop_) U8 _; 1)

istruct (UiTextBox) {
    Mem *mem;
    Buf *buf;
    BufCursor cursor;
    Vec2 cursor_coord;
    Vec2 scroll_coord;
    Vec2 scroll_coord_n;
    F32 total_width;
    F32 total_height;
    Bool dragging;
    Bool single_line_mode;
};

static Void text_box_draw_line (UiTextBox *info, UiBox *box, U32 line_idx, String text, Vec4 color, F32 x, F32 y) {
    tmem_new(tm);
    glBindTexture(GL_TEXTURE_2D, ui->font->atlas_texture);

    U32 cell_w = ui->font->width;
    U32 cell_h = ui->font->height;
    SliceGlyphInfo infos = font_get_glyph_infos(ui->font, tm, text);

    x = floor(x - info->scroll_coord.x);

    F32 descent = cast(F32, ui->font->descent);
    F32 line_spacing = ui_config_get_f32(UI_CONFIG_LINE_SPACING);

    U64 selection_start = info->cursor.byte_offset;
    U64 selection_end   =  info->cursor.selection_offset;
    if (selection_end < selection_start) swap(selection_start, selection_end);

    U32 col_idx = 0;
    array_iter (glyph_info, &infos, *) {
        if (x > box->rect.x + box->rect.w) break;

        if (x + cell_w > box->rect.x) {
            BufCursor current = buf_cursor_new(info->buf, line_idx, col_idx);
            Bool selected = current.byte_offset >= selection_start && current.byte_offset < selection_end;

            if (selected) draw_rect(
                .color        = ui_config_get_vec4(UI_CONFIG_BG_SELECTION),
                .color2       = ui_config_get_vec4(UI_CONFIG_BG_SELECTION),
                .top_left     = {x, y - cell_h - line_spacing},
                .bottom_right = {x + cell_w, y},
            );

            AtlasSlot *slot = font_get_atlas_slot(ui->font, glyph_info);
            Vec2 top_left = {x + slot->bearing_x, y - descent - line_spacing/2 - slot->bearing_y};
            Vec2 bottom_right = {top_left.x + slot->width, top_left.y + slot->height};
            Vec4 final_text_color = selected ? ui_config_get_vec4(UI_CONFIG_TEXT_SELECTION) : color;

            draw_rect(
                .top_left     = top_left,
                .bottom_right = bottom_right,
                .texture_rect = {slot->x, slot->y, slot->width, slot->height},
                .text_color   = final_text_color,
                .text_is_grayscale = (slot->pixel_mode == FT_PIXEL_MODE_GRAY),
            );
        }

        x += cell_w;
        col_idx++;
    }
}

static Void text_box_draw (UiBox *box) {
    tmem_new(tm);

    UiBox *container = box->parent;
    UiTextBox *info = get_box_data(container, 0, 0);

    if (! set_font(container)) return;

    U32 cell_h = ui->font->height;
    U32 cell_w = ui->font->width;

    F32 line_spacing = ui_config_get_f32(UI_CONFIG_LINE_SPACING);
    info->total_width  = buf_get_widest_line(info->buf) * cell_w;
    info->total_height = buf_get_line_count(info->buf) * (cell_h + line_spacing);

    F32 line_height = cell_h + line_spacing;
    BufCursor pos = text_box_coord_to_cursor(info, box, box->rect.top_left);
    F32 y = box->rect.y + line_height - info->scroll_coord.y + (pos.line * line_height);

    buf_iter_lines (line, info->buf, tm, pos.line) {
        if (y - line_height > box->rect.y + box->rect.h) break;
        text_box_draw_line(info, box, cast(U32, line->idx), line->text, container->style.text_color, box->rect.x, floor(y));
        y += line_height;
    }

    if (box->signals.focused) draw_rect(
        .color = ui_config_get_vec4(UI_CONFIG_MAGENTA_1),
        .color2 = {-1},
        .top_left = info->cursor_coord,
        .bottom_right = { info->cursor_coord.x + 2, info->cursor_coord.y + cell_h },
    );
}

static Void text_box_vscroll (UiTextBox *info, UiBox *box, U32 line, UiAlign align) {
    F32 line_spacing = ui_config_get_f32(UI_CONFIG_LINE_SPACING);
    U32 cell_h = ui->font->height;
    info->scroll_coord_n.y = cast(F32, line) * (cell_h + line_spacing);

    F32 visible_h = box->rect.h;

    if (info->total_height <= visible_h) {
        info->scroll_coord_n.y = 0;
    } else if (align == UI_ALIGN_MIDDLE) {
        info->scroll_coord_n.y -= round(visible_h / 2);
    } else if (align == UI_ALIGN_END) {
        info->scroll_coord_n.y -= visible_h - cell_h - line_spacing;
    }
}

static Void text_box_hscroll (UiTextBox *info, UiBox *box, U32 column, UiAlign align) {
    U32 cell_w = ui->font->width;
    info->scroll_coord_n.x = cast(F32, column) * cell_w;

    F32 visible_w = box->rect.w;

    if (info->total_width <= visible_w) {
        info->scroll_coord_n.x = 0;
    } else if (align == UI_ALIGN_MIDDLE) {
        info->scroll_coord_n.x -= round(visible_w / 2);
    } else if (align == UI_ALIGN_END) {
        info->scroll_coord_n.x -= visible_w - cell_w;
    }
}

static Void text_box_scroll_into_view (UiTextBox *info, UiBox *box, BufCursor *pos, U32 padding) {
    U32 cell_w = ui->font->width;
    U32 cell_h = ui->font->height;
    F32 line_spacing = ui_config_get_f32(UI_CONFIG_LINE_SPACING);

    Vec2 coord = text_box_cursor_to_coord(info, box, pos);

    U32 x_padding = padding * cell_w;
    U32 y_padding = padding * (cell_h + line_spacing);

    if (coord.x < box->rect.x + x_padding) {
        text_box_hscroll(info, box, sat_sub32(pos->column, padding), UI_ALIGN_START);
    } else if (coord.x > box->rect.x + box->rect.w - x_padding) {
        text_box_hscroll(info, box, clamp(sat_add32(pos->column, padding), 0u, buf_get_widest_line(info->buf)), UI_ALIGN_END);
    }

    if (coord.y < box->rect.y + y_padding) {
        text_box_vscroll(info, box, sat_sub32(pos->line, padding), UI_ALIGN_START);
    } else if (coord.y + cell_h > box->rect.y + box->rect.h - y_padding) {
        text_box_vscroll(info, box, clamp(sat_add32(pos->line, padding), 0u, buf_get_line_count(info->buf)-1), UI_ALIGN_END);
    }
}

static BufCursor text_box_coord_to_cursor (UiTextBox *info, UiBox *box, Vec2 coord) {
    U32 line = 0;
    U32 column = 0;

    F32 cell_w = ui->font->width;
    F32 cell_h = ui->font->height;
    F32 line_spacing = ui_config_get_f32(UI_CONFIG_LINE_SPACING);

    coord.x = coord.x - box->rect.x + info->scroll_coord.x;
    coord.y = coord.y - box->rect.y + info->scroll_coord.y;

    line = clamp(coord.y / (cell_h + line_spacing), cast(F32, 0), cast(F32, buf_get_line_count(info->buf)-1));

    tmem_new(tm);
    String line_text = buf_get_line(info->buf, tm, line);

    U32 max_col = str_codepoint_count(line_text);
    column = clamp(round(coord.x / cell_w), 0u, max_col);

    return buf_cursor_new(info->buf, line, column);
}

static Vec2 text_box_cursor_to_coord (UiTextBox *info, UiBox *box, BufCursor *pos) {
    Vec2 coord = {};

    F32 char_width  = ui->font->width;
    F32 line_spacing = ui_config_get_f32(UI_CONFIG_LINE_SPACING);
    F32 line_height = ui->font->height + line_spacing;

    coord.y = pos->line * line_height + line_spacing/2;

    tmem_new(tm);
    String line_str = buf_get_line(info->buf, tm, pos->line);

    U32 i = 0;
    str_utf8_iter (it, line_str) {
        if (i >= pos->column) break;
        coord.x += char_width;
        i++;
    }

    coord.x += box->rect.x - info->scroll_coord.x;
    coord.y += box->rect.y - info->scroll_coord.y;

    return coord;
}

static UiBox *ui_text_box (String label, Buf *buf, Bool single_line_mode) {
    UiBox *container = ui_box_str(0, label) {
        UiTextBox *info = get_box_data(container, sizeof(UiTextBox), sizeof(UiTextBox));

        info->buf = buf;
        info->single_line_mode = single_line_mode;

        buf_cursor_clamp(info->buf, &info->cursor); // In case the buffer changed.

        set_font(container);
        Font *font = ui_config_get_font(UI_CONFIG_FONT_MONO);
        ui_style_font(UI_FONT, font);
        ui_style_f32(UI_FONT_SIZE, font->size);

        F32 line_spacing = ui_config_get_f32(UI_CONFIG_LINE_SPACING);

        if (info->single_line_mode) {
            U32 height = 2*container->style.padding.y + (ui->font ? ui->font->height : 12) + line_spacing;
            ui_style_box_size(container, UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, height, 1});
        }

        F32 visible_w = container->rect.w - 2*container->style.padding.x;
        F32 visible_h = container->rect.h - 2*container->style.padding.y;
        Bool scroll_y = info->total_height > visible_h && visible_h > 0;
        Bool scroll_x = info->total_width  > visible_w && visible_w > 0;
        F32 scrollbar_width = ui_config_get_f32(UI_CONFIG_SCROLLBAR_WIDTH);

        UiBox *text_box = ui_box(UI_BOX_CAN_FOCUS|UI_BOX_REACTIVE|UI_BOX_CLIPPING, "text") {
            ui_style_u32(UI_ANIMATION, UI_MASK_HEIGHT|UI_MASK_WIDTH);
            ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, container->rect.w - container->style.padding.x - (scroll_y ? scrollbar_width : 0), 1});
            ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, container->rect.h - container->style.padding.y - (scroll_x ? scrollbar_width : 0), 1});

            if (text_box->signals.hovered && ui->event->tag == EVENT_SCROLL) {
                U32 cell_w = ui->font->width;
                U32 cell_h = ui->font->height;

                if (scroll_y && !is_key_pressed(SDLK_LSHIFT)) {
                    info->scroll_coord_n.y -= (cell_h + line_spacing) * ui->event->y;
                    info->scroll_coord_n.y  = clamp(info->scroll_coord_n.y, 0, info->total_height - visible_h);
                    ui_eat_event();
                } else if (scroll_x) {
                    info->scroll_coord_n.x -= cell_w * ui->event->y;
                    info->scroll_coord_n.x  = clamp(info->scroll_coord_n.x, 0, info->total_width - visible_w);
                    ui_eat_event();
                }
            }

            text_box->draw_fn = text_box_draw;
        }

        if (scroll_y) {
            F32 ratio = visible_h / info->total_height;
            UiRect rect = { container->rect.w - scrollbar_width, 0, scrollbar_width, container->rect.h };
            if (scroll_x) rect.h -= scrollbar_width;

            F32 max_y_offset = max(0.0f, info->total_height - visible_h);
            F32 knob_height  = rect.h * (visible_h / info->total_height);
            F32 max_knob_v   = rect.h - knob_height;
            F32 before       = (info->scroll_coord.y / max_y_offset) * max_knob_v;
            F32 after        = before;

            ui_vscroll_bar(str("scroll_bar_y"), rect, ratio, &after);
            if (before != after) info->scroll_coord.y = info->scroll_coord_n.y = clamp(after / max_knob_v, 0, 1) * max_y_offset;
        }

        if (scroll_x && !info->single_line_mode) {
            F32 ratio = visible_w / info->total_width;
            UiRect rect = { 0, container->rect.h - scrollbar_width, container->rect.w, scrollbar_width };
            if (scroll_y) rect.w -= scrollbar_width;

            F32 max_x_offset = max(0.0f, info->total_width - visible_w);
            F32 knob_width   = rect.w * (visible_w / info->total_width);
            F32 max_knob_h   = rect.w - knob_width;
            F32 before       = (info->scroll_coord.x / max_x_offset) * max_knob_h;
            F32 after        = before;

            ui_hscroll_bar(str("scroll_bar_x"), rect, ratio, &after);
            if (before != after) info->scroll_coord.x = info->scroll_coord_n.x = clamp(after / max_knob_h, 0, 1) * max_x_offset;
        }

        if (text_box->signals.focused && ui->event->tag == EVENT_KEY_PRESS) {
            switch (ui->event->key) {
            case SDLK_DELETE:
                if (ui->event->mods & SDL_KMOD_CTRL) {
                    buf_cursor_move_right_word(info->buf, &info->cursor, false);
                    buf_delete(info->buf, &info->cursor);
                } else {
                    buf_cursor_move_right(info->buf, &info->cursor, false);
                    buf_delete(info->buf, &info->cursor);
                }
                ui_eat_event();
                text_box_scroll_into_view(info, text_box, &info->cursor, 4);
                break;
            case SDLK_W:
                if (ui->event->mods & SDL_KMOD_CTRL) {
                    buf_cursor_move_left_word(info->buf, &info->cursor, false);
                    buf_delete(info->buf, &info->cursor);
                    text_box_scroll_into_view(info, text_box, &info->cursor, 4);
                    ui_eat_event();
                }
                break;
            case SDLK_A:
                if (ui->event->mods & SDL_KMOD_CTRL) {
                    buf_cursor_move_to_end(info->buf, &info->cursor, true);
                    buf_cursor_move_to_start(info->buf, &info->cursor, false);
                    text_box_scroll_into_view(info, text_box, &info->cursor, 4);
                    ui_eat_event();
                }
                break;
            case SDLK_V:
                if (ui->event->mods & SDL_KMOD_CTRL) {
                    String text = get_clipboard_text(ui->frame_mem);
                    buf_insert(info->buf, &info->cursor, text);
                }
                break;
            case SDLK_X:
                if (ui->event->mods & SDL_KMOD_CTRL) {
                    String text = buf_get_selection(info->buf, &info->cursor);
                    if (text.count) {
                        set_clipboard_text(text);
                        buf_delete(info->buf, &info->cursor);
                    }
                }
                break;
            case SDLK_C:
                if (ui->event->mods & SDL_KMOD_CTRL) {
                    String text = buf_get_selection(info->buf, &info->cursor);
                    if (text.count) set_clipboard_text(text);
                }
                break;
            case SDLK_RETURN:
                if (info->single_line_mode) break;

                Bool special_case = buf_cursor_at_end_no_newline(info->buf, &info->cursor);
                buf_insert(info->buf, &info->cursor, str("\n"));

                if (special_case) {
                    // @todo This is a stupid hack for the case when we insert at the end
                    // of the buffer but the buffer doesn't end with a newline. We have to
                    // insert 2 newlines in that case, but the cursor ends up in a weird
                    // state.
                    info->cursor.byte_offset--;
                    info->cursor.selection_offset--;
                    buf_insert(info->buf, &info->cursor, str("\n"));
                }

                text_box_scroll_into_view(info, text_box, &info->cursor, 4);
                ui_eat_event();
                break;
            case SDLK_BACKSPACE:
                if (info->cursor.byte_offset == info->cursor.selection_offset) buf_cursor_move_left(info->buf, &info->cursor, false);
                buf_delete(info->buf, &info->cursor);
                text_box_scroll_into_view(info, text_box, &info->cursor, 4);
                ui_eat_event();
                break;
            case SDLK_LEFT:
                buf_cursor_move_left(info->buf, &info->cursor, !(ui->event->mods & SDL_KMOD_SHIFT));
                text_box_scroll_into_view(info, text_box, &info->cursor, 4);
                ui_eat_event();
                break;
            case SDLK_RIGHT:
                buf_cursor_move_right(info->buf, &info->cursor, !(ui->event->mods & SDL_KMOD_SHIFT));
                text_box_scroll_into_view(info, text_box, &info->cursor, 4);
                ui_eat_event();
                break;
            case SDLK_UP:
                buf_cursor_move_up(info->buf, &info->cursor, !(ui->event->mods & SDL_KMOD_SHIFT));
                text_box_scroll_into_view(info, text_box, &info->cursor, 4);
                ui_eat_event();
                break;
            case SDLK_DOWN:
                buf_cursor_move_down(info->buf, &info->cursor, !(ui->event->mods & SDL_KMOD_SHIFT));
                text_box_scroll_into_view(info, text_box, &info->cursor, 4);
                ui_eat_event();
                break;
            }
        }

        if (text_box->signals.clicked && ui->event->key == SDL_BUTTON_LEFT) {
            info->dragging = false;
        }

        if (text_box->signals.pressed) {
            grab_focus(text_box);
            U32 soff = info->cursor.selection_offset;
            info->cursor = text_box_coord_to_cursor(info, text_box, ui->mouse);
            info->cursor.selection_offset = soff;

            if (info->dragging) {
                text_box_scroll_into_view(info, text_box, &info->cursor, 0);
            } else {
                info->dragging = true;
                info->cursor.selection_offset = info->cursor.byte_offset;
            }
        }

        if (text_box->signals.focused && ui->event->tag == EVENT_TEXT_INPUT) {
            buf_insert(info->buf, &info->cursor, ui->event->text);
            text_box_scroll_into_view(info, text_box, &info->cursor, 4);
            ui_eat_event();
        }

        animate_vec2(&info->scroll_coord, info->scroll_coord_n, ui_config_get_f32(UI_CONFIG_ANIMATION_TIME_1));
        if (ui->font) info->cursor_coord = text_box_cursor_to_coord(info, text_box, &info->cursor);
    }

    return container;
}

static UiBox *ui_entry (String id, Buf *buf, F32 width_in_chars, String hint) {
    UiBox *container = ui_box_str(UI_BOX_INVISIBLE, id) {
        UiBox *text_box = ui_text_box(str("text"), buf, true);
        ui_style_box_from_config(text_box, UI_RADIUS, UI_CONFIG_RADIUS_1);
        ui_style_box_from_config(text_box, UI_BG_COLOR, UI_CONFIG_BG_3);
        ui_style_box_from_config(text_box, UI_BORDER_COLOR, UI_CONFIG_BORDER_1_COLOR);
        ui_style_box_from_config(text_box, UI_BORDER_WIDTHS, UI_CONFIG_BORDER_1_WIDTH);
        ui_style_box_from_config(text_box, UI_INSET_SHADOW_WIDTH, UI_CONFIG_IN_SHADOW_1_WIDTH);
        ui_style_box_from_config(text_box, UI_INSET_SHADOW_COLOR, UI_CONFIG_IN_SHADOW_1_COLOR);
        ui_style_box_from_config(text_box, UI_PADDING, UI_CONFIG_PADDING_1);
        F32 width = width_in_chars*(text_box->style.font ? text_box->style.font->width : 12) + 2*text_box->style.padding.x;
        ui_style_box_size(text_box, UI_WIDTH, (UiSize){UI_SIZE_PIXELS, width, 1});

        if (hint.count && buf_get_count(buf) == 0) {
            UiBox *h = ui_label("hint", hint);
            UiBox *inner_text = array_get(&text_box->children, 0);
            ui_style_box_f32(h, UI_FLOAT_X, inner_text->rect.x - container->rect.x);
            ui_style_box_f32(h, UI_FLOAT_Y, inner_text->rect.y - container->rect.y);
            ui_style_box_font(h, UI_FONT, text_box->next_style.font);
            ui_style_box_f32(h, UI_FONT_SIZE, text_box->next_style.font_size);
            ui_style_box_from_config(h, UI_TEXT_COLOR, UI_CONFIG_TEXT_COLOR_2);
        }
    }

    return container;
}

istruct (UiIntPicker) {
    Mem *mem;
    I64 val;
    Buf *buf;
};

static UiBox *ui_int_picker (String id, I64 *val, I64 min, I64 max, U8 width_in_chars) {
    UiBox *container = ui_box_str(UI_BOX_REACTIVE, id) {
        UiIntPicker *info = get_box_data(container, sizeof(UiIntPicker), sizeof(UiIntPicker));
        if (! info->buf) info->buf = buf_new(info->mem, str(""));

        if (container->start_frame == ui->frame || info->val != *val) {
            String str = astr_fmt(ui->frame_mem, "%li", *val);
            buf_clear(info->buf);
            buf_insert(info->buf, &(BufCursor){}, str);
            info->val = *val;
        }

        UiBox *entry = ui_entry(str("entry"), info->buf, width_in_chars, str(""));

        Bool valid = true;
        {
            String text = buf_get_str(info->buf, ui->frame_mem);
            array_iter (c, &text) {
                if (c == '-' && ARRAY_IDX == 0) continue;
                if (c < '0' || c > '9') { valid = false; break; }
            }
            I64 v;
            if (valid) valid = str_to_i64(cstr(ui->frame_mem, text), &v, 10);
            if (valid && (v < min || v > max)) valid = false;
            if (valid) *val = v;
        }

        if (! valid) ui_style_box_from_config(entry, UI_TEXT_COLOR, UI_CONFIG_RED_TEXT);

        if (container->signals.hovered) {
            ui_tooltip("tooltip") ui_label("label", astr_fmt(ui->frame_mem, "Integer in range [%li, %li].", min, max));

            if (valid && (ui->event->tag == EVENT_SCROLL)) {
                if (ui->event->y > 0) {
                    *val += 1;
                } else {
                    *val -= 1;
                }

                *val = clamp(*val, min, max);
                ui_eat_event();
            }
        }
    }

    return container;
}

static UiBox *ui_slider_str (String label, F32 *val) {
    UiBox *container = ui_box_str(UI_BOX_REACTIVE|UI_BOX_CAN_FOCUS, label) {
        ui_tag("slider");
        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, 20, 1});
        ui_style_f32(UI_EDGE_SOFTNESS, 0);
        ui_style_f32(UI_SPACING, 0);
        ui_style_u32(UI_ALIGN_Y, UI_ALIGN_MIDDLE);

        ui_style_rule(".focus") {
            ui_style_from_config(UI_BORDER_WIDTHS, UI_CONFIG_BORDER_FOCUS_WIDTH);
            ui_style_from_config(UI_BORDER_COLOR, UI_CONFIG_BORDER_FOCUS_COLOR);
        }

        if (container->signals.focused && (ui->event->tag == EVENT_KEY_PRESS) && (ui->event->key == SDLK_LEFT)) {
            *val -= .1;
            *val = clamp(*val, 0, 1);
            ui_eat_event();
        }

        if (container->signals.focused && (ui->event->tag == EVENT_KEY_PRESS) && (ui->event->key == SDLK_RIGHT)) {
            *val += .1;
            *val = clamp(*val, 0, 1);
            ui_eat_event();
        }

        if (container->signals.pressed) {
            *val = (ui->mouse.x - container->rect.x) / container->rect.w;
            *val = clamp(*val, 0, 1);
        }

        if (container->signals.hovered && (ui->event->tag == EVENT_SCROLL)) {
            *val = *val - (10*ui->event->y) / container->rect.w;
            *val = clamp(*val, 0, 1);
            ui_eat_event();
        }

        ui_box(UI_BOX_CLICK_THROUGH, "slider_track") {
            ui_style_f32(UI_FLOAT_X, 0);
            ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});
            ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, 4, 0});
            ui_style_from_config(UI_BG_COLOR, UI_CONFIG_FG_1);
            ui_style_f32(UI_EDGE_SOFTNESS, 0);

            ui_box(UI_BOX_CLICK_THROUGH, "slider_track_fill") {
                ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, *val, 0});
                ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});
                ui_style_from_config(UI_BG_COLOR, UI_CONFIG_MAGENTA_1);
                ui_style_f32(UI_EDGE_SOFTNESS, 0);
            }
        }

        F32 knob_size = max(8, container->rect.h - 8);

        ui_box(UI_BOX_CLICK_THROUGH|UI_BOX_INVISIBLE, "slider_spacer") {
            F32 spacer_width = max(0, *val - knob_size/(2*max(knob_size, container->rect.w)));
            assert_dbg(spacer_width <= 1.0);
            assert_dbg(spacer_width >= 0.0);
            ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, spacer_width, 0});
            ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, 2, 0});
        }

        ui_box(UI_BOX_CLICK_THROUGH, "slider_knob") {
            ui_style_f32(UI_EDGE_SOFTNESS, 1.3);
            ui_style_from_config(UI_BG_COLOR, UI_CONFIG_SLIDER_KNOB);
            ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, knob_size, 1});
            ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, knob_size, 1});
            ui_style_vec4(UI_RADIUS, vec4(knob_size/2, knob_size/2, knob_size/2, knob_size/2));
        }
    }

    return container;
}

static UiBox *ui_slider (CString label, F32 *val) {
    return ui_slider_str(str(label), val);
}

// Note that the grid will only display correctly as long as
// it's size is set to UI_SIZE_PIXELS or UI_SIZE_PCT_PARENT.
// If it has a downwards dependent size (UI_SIZE_CHILDREN_SUM)
// it will just collapse.
//
// The children of the grid must only be grid_cells:
//
//     ui_grid() {
//         ui_grid_cell(){}
//         ui_grid_cell(){}
//         ui_grid_cell(){}
//     }
//
static UiBox *ui_grid_push (String label) {
    UiBox *grid = ui_box_push_str(0, label);
    ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});
    ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});
    return grid;
}

static Void ui_grid_pop () {
    UiBox *grid = array_get_last(&ui->box_stack);

    F32 rows = 0;
    F32 cols = 0;

    array_iter (cell, &grid->children) {
        Vec4 coords = *cast(Vec4*, cell->scratch);
        if ((coords.x + coords.z) > rows) rows = coords.x + coords.z;
        if ((coords.y + coords.w) > cols) cols = coords.y + coords.w;
    }

    F32 cell_width  = floor(grid->rect.w / rows);
    F32 cell_height = floor(grid->rect.h / cols);

    array_iter (cell, &grid->children) {
        Vec4 coords = *cast(Vec4*, cell->scratch);
        cell->next_style.floating[0] = coords.x * cell_width;
        cell->next_style.floating[1] = coords.y * cell_height;
        cell->next_style.size.width  = (UiSize){UI_SIZE_PIXELS, coords.z * cell_width, 1};
        cell->next_style.size.height = (UiSize){UI_SIZE_PIXELS, coords.w * cell_height, 1};
    }

    ui_pop_parent();
}

static Void ui_grid_pop_ (Void *) { ui_grid_pop(); }

#define ui_grid(L)\
    ui_grid_push(str(L));\
    if (cleanup(ui_grid_pop_) U8 _; 1)

// The unit of measurement for the x/y/w/h parameters is basic cells.
// That is, think of the grid as made up of basic cells. This function
// constructs super cells by defining on which basic cell they start,
// and how many basic cells they cover.
static UiBox *ui_grid_cell_push (F32 x, F32 y, F32 w, F32 h) {
    UiBox *cell = ui_box_push_fmt(0, "grid_cell_%f_%f", x, y);
    ui_style_f32(UI_FLOAT_X, 0);
    ui_style_f32(UI_FLOAT_Y, 0);
    ui_style_from_config(UI_PADDING, UI_CONFIG_PADDING_1);
    ui_style_from_config(UI_BG_COLOR, UI_CONFIG_BG_3);
    ui_style_from_config(UI_BORDER_WIDTHS, UI_CONFIG_BORDER_1_WIDTH);
    ui_style_from_config(UI_BORDER_COLOR, UI_CONFIG_BORDER_1_COLOR);
    ui_style_f32(UI_EDGE_SOFTNESS, 0);

    Vec4 *coords = mem_new(ui->frame_mem, Vec4);
    coords->x = x;
    coords->y = y;
    coords->z = w;
    coords->w = h;
    cell->scratch = cast(U64, coords);

    return cell;
}

static Void ui_grid_cell_pop  ()       { ui_pop_parent(); }
static Void ui_grid_cell_pop_ (Void *) { ui_grid_cell_pop(); }

#define ui_grid_cell(...)\
    ui_grid_cell_push(__VA_ARGS__);\
    if (cleanup(ui_grid_cell_pop_) U8 _; 1)

istruct (SatValPicker) {
    F32 hue;
    F32 sat;
    F32 val;
};

static Void draw_color_sat_val_picker (UiBox *box) {
    SatValPicker *info = cast(SatValPicker*, box->scratch);
    UiRect *r = &box->rect;
    Vec4 c = hsva_to_rgba(vec4(info->hue, 1, 1, 1));
    Vec4 lc = c;

    draw_rect(
        .top_left     = r->top_left,
        .bottom_right = {r->x+r->w, r->y+r->h},
        .color        = lc,
        .color2       = {-1},
    );

    SliceVertex v = draw_rect(
        .top_left     = r->top_left,
        .bottom_right = {r->x+r->w, r->y+r->h},
        .color        = {1, 1, 1, 0},
        .color2       = {-1},
    );
    v.data[0].color = vec4(1, 1, 1, 1);
    v.data[1].color = vec4(1, 1, 1, 1);
    v.data[5].color = vec4(1, 1, 1, 1);

    draw_rect(
        .top_left     = r->top_left,
        .bottom_right = {r->x+r->w, r->y+r->h},
        .color        = {0, 0, 0, 0},
        .color2       = {0, 0, 0, 1},
    );

    F32 half = ui_config_get_font(UI_CONFIG_FONT_NORMAL)->size;
    Vec2 center = {
        box->rect.x + (info->sat * box->rect.w),
        box->rect.y + (1 - info->val) * box->rect.h
    };
    draw_rect(
        .edge_softness = 1,
        .radius        = { half, half, half, half },
        .top_left      = { center.x-half, center.y-half },
        .bottom_right  = { center.x+half, center.y+half },
        .color         = hsva_to_rgba(vec4(info->hue, info->sat, info->val, 1)),
        .color2        = {-1},
        .border_color  = {1, 1, 1, 1},
        .border_widths = {2, 2, 2, 2},
    );
}

static UiBox *ui_color_sat_val_picker (String id, F32 hue, F32 *sat, F32 *val) {
    UiBox *container = ui_box_str(UI_BOX_REACTIVE, id) {
        SatValPicker *data = mem_new(ui->frame_mem, SatValPicker);
        data->hue = hue;
        data->sat = *sat;
        data->val = *val;
        container->scratch = cast(U64, data);
        container->draw_fn = draw_color_sat_val_picker;
        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, 200, 0});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, 200, 0});

        if (container->signals.clicked || (container->signals.pressed && ui->event->tag == EVENT_MOUSE_MOVE)) {
            *sat = (ui->mouse.x - container->rect.x) / container->rect.w;
            *sat = clamp(*sat, 0, 1);
            *val = 1 - (ui->mouse.y - container->rect.y) / container->rect.h;
            *val = clamp(*val, 0, 1);
        }
    }

    return container;
}

static Void draw_color_hue_picker (UiBox *box) {
    F32 *hue = cast(F32*, box->scratch);
    F32 segment = box->rect.h / 6;
    UiRect r = box->rect;
    r.h = segment;

    for (U64 i = 0; i < 6; ++i) {
        Vec4 col1 = hsva_to_rgba(vec4(cast(F32,i)/6, 1, 1, 1));
        Vec4 col2 = hsva_to_rgba(vec4(cast(F32, i+1)/6, 1, 1, 1));
        draw_rect(
            .top_left     = r.top_left,
            .bottom_right = {r.x+r.w, r.y+r.h},
            .color        = col1,
            .color2       = col2,
        );
        r.y += segment;
    }

    F32 half = ui_config_get_font(UI_CONFIG_FONT_NORMAL)->size;
    Vec2 center = {
        box->rect.x + box->rect.w/2,
        box->rect.y + *hue * box->rect.h,
    };
    draw_rect(
        .edge_softness = 1,
        .radius        = { half, half, half, half },
        .top_left      = { center.x-half, center.y-half },
        .bottom_right  = { center.x+half, center.y+half },
        .color         = hsva_to_rgba(vec4(*hue, 1, 1, 1)),
        .color2        = {-1},
        .border_color  = {1, 1, 1, 1},
        .border_widths = {2, 2, 2, 2},
    );
}

static UiBox *ui_color_hue_picker (String id, F32 *hue) {
    UiBox *container = ui_box_str(UI_BOX_REACTIVE, id) {
        container->draw_fn = draw_color_hue_picker;
        container->scratch = cast(U64, hue);
        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, 20, 1});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, 200, 0});

        if (container->signals.clicked || (container->signals.pressed && ui->event->tag == EVENT_MOUSE_MOVE)) {
            *hue = (ui->mouse.y - container->rect.y) / container->rect.h;
            *hue = clamp(*hue, 0, 1);
        }
    }

    return container;
}

static Void draw_color_alpha_picker (UiBox *box) {
    F32 *alpha = cast(F32*, box->scratch);
    UiRect *r = &box->rect;

    draw_rect(
        .top_left     = r->top_left,
        .bottom_right = {r->x+r->w, r->y+r->h},
        .color        = {1, 1, 1, 1},
        .color2       = {0, 0, 0, 1},
    );

    F32 half = ui_config_get_font(UI_CONFIG_FONT_NORMAL)->size;
    Vec2 center = {
        box->rect.x + box->rect.w/2,
        box->rect.y + (1 - *alpha) * box->rect.h,
    };
    draw_rect(
        .edge_softness = 1,
        .radius        = { half, half, half, half },
        .top_left      = { center.x-half, center.y-half },
        .bottom_right  = { center.x+half, center.y+half },
        .color         = { *alpha, *alpha, *alpha, 1 },
        .color2        = {-1},
        .border_color  = {1, 1, 1, 1},
        .border_widths = {2, 2, 2, 2},
    );
}

static UiBox *ui_color_alpha_picker (String id, F32 *alpha) {
    UiBox *container = ui_box_str(UI_BOX_REACTIVE, id) {
        container->draw_fn = draw_color_alpha_picker;
        container->scratch = cast(U64, alpha);
        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, 20, 1});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, 200, 0});

        if (container->signals.clicked || (container->signals.pressed && ui->event->tag == EVENT_MOUSE_MOVE)) {
            *alpha = (ui->mouse.y - container->rect.y) / container->rect.h;
            *alpha = 1 - clamp(*alpha, 0, 1);
        }
    }

    return container;
}

ienum (UiColorPickerMode, U8) {
    COLOR_PICKER_HEX,
    COLOR_PICKER_RGBA,
    COLOR_PICKER_HSVA,
};

istruct (UiColorPicker) {
    Mem *mem;
    F32 h, s, v, a;
    Bool valid;
    Buf *buf;
};

static UiBox *ui_color_picker (String id, UiColorPickerMode mode, F32 *h, F32 *s, F32 *v, F32 *a) {
    UiBox *container = ui_box_str(0, id) {
        UiColorPicker *info = get_box_data(container, sizeof(UiColorPicker), sizeof(UiColorPicker));
        if (! info->buf) info->buf = buf_new(info->mem, str(""));

        if (container->start_frame == ui->frame || info->h != *h || info->s != *s || info->v != *v || info->a != *a) {
            buf_clear(info->buf);
            info->valid = true;
            info->h = *h;
            info->s = *s;
            info->v = *v;
            info->a = *a;

            switch (mode) {
            case COLOR_PICKER_HEX: {
                Vec4 c = hsva_to_rgba(vec4(*h, *s, *v, *a));
                String str = astr_fmt(ui->frame_mem, "#%02x%02x%02x%02x", cast(U32, round(c.x*255)), cast(U32, round(c.y*255)), cast(U32, round(c.z*255)), cast(U32, round(c.w*255)));
                buf_insert(info->buf, &(BufCursor){}, str);
            } break;

            case COLOR_PICKER_HSVA: {
                String str = astr_fmt(ui->frame_mem, "%u, %u, %u, %u", cast(U32, round(*h*255)), cast(U32, round(*s*255)), cast(U32, round(*v*255)), cast(U32, round(*a*255)));
                buf_insert(info->buf, &(BufCursor){}, str);
            } break;

            case COLOR_PICKER_RGBA: {
                Vec4 c = hsva_to_rgba(vec4(*h, *s, *v, *a));
                String str = astr_fmt(ui->frame_mem, "%u, %u, %u, %u", cast(U32, round(c.x*255)), cast(U32, round(c.y*255)), cast(U32, round(c.z*255)), cast(U32, round(c.w*255)));
                buf_insert(info->buf, &(BufCursor){}, str);
            } break;
            }
        }

        EventTag event_tag = ui->event->tag;
        UiBox *entry = ui_entry(str("entry"), info->buf, 18, str(""));
        container->next_style.size.width.strictness = 1;

        if (event_tag != ui->event->tag) {
            String text = buf_get_str(info->buf, ui->frame_mem);
            Vec4 hsva = {};
            info->valid = true;

            switch (mode) {
            case COLOR_PICKER_HEX: {
                info->valid = (text.count == 9);
                if (info->valid && text.data[0] != '#') info->valid = false;
                for (U64 i = 0; i < 4; ++i) {
                    if (! info->valid) break;
                    String token = str_slice(text, 1+2*i, 2);
                    U64 v;
                    info->valid = str_to_u64(cstr(ui->frame_mem, token), &v, 16);
                    if (info->valid && v > 255) info->valid = false;
                    if (info->valid) hsva.v[i] = cast(F32, v) / 255.f;
                }
            } break;

            case COLOR_PICKER_HSVA:
            case COLOR_PICKER_RGBA: {
                ArrayString tokens;
                array_init(&tokens, ui->frame_mem);
                str_split(text, str(", "), 0, 0, &tokens);
                if (tokens.count != 4) info->valid = false;
                array_iter (token, &tokens) {
                    if (! info->valid) break;
                    U64 v;
                    info->valid = str_to_u64(cstr(ui->frame_mem, token), &v, 10);
                    if (info->valid && v > 255) info->valid = false;
                    if (info->valid) hsva.v[ARRAY_IDX] = cast(F32, v) / 255.f;
                }
            } break;
            }

            if (info->valid) {
                if (mode != COLOR_PICKER_HSVA) hsva = rgba_to_hsva(hsva);
                *h = hsva.x;
                *s = hsva.y;
                *v = hsva.z;
                *a = hsva.w;
            }
        }

        if (! info->valid) {
            UiBox *inner = array_get(&entry->children, 0);
            ui_style_box_from_config(inner, UI_TEXT_COLOR, UI_CONFIG_RED_TEXT);
        }
    }

    return container;
}

// =============================================================================
// Frame:
// =============================================================================
static Void ui_eat_event () {
    ui->event->tag = EVENT_EATEN;
}

static Void update_input_state (Event *event) {
    ui->event = event;

    switch (event->tag) {
    case EVENT_DUMMY:       break;
    case EVENT_EATEN:       break;
    case EVENT_SCROLL:      break;
    case EVENT_WINDOW_SIZE: break;
    case EVENT_TEXT_INPUT:  break;
    case EVENT_KEY_PRESS:   map_add(&ui->pressed_keys, event->key, 0); break;
    case EVENT_KEY_RELEASE: map_remove(&ui->pressed_keys, event->key); break;
    case EVENT_MOUSE_MOVE:
        ui->mouse_dt.x = event->x - ui->mouse.x;
        ui->mouse_dt.y = event->y - ui->mouse.y;
        ui->mouse.x = event->x;
        ui->mouse.y = event->y;
        break;
    }
}

static Void grab_focus (UiBox *box) {
    U64 idx = array_find(&ui->depth_first, IT == box);
    assert_dbg(idx != ARRAY_NIL_IDX);
    ui->focus_idx = idx;
    ui->focused = box;
}

static Void find_next_focus () {
    U64 start = ui->focus_idx;
    while (true) {
        ui->focus_idx = (ui->focus_idx + 1) % ui->depth_first.count;
        ui->focused = array_get(&ui->depth_first, ui->focus_idx);
        if (ui->focused->flags & UI_BOX_CAN_FOCUS) break;
        if (ui->focus_idx == start) break;
    }
}

static Void find_prev_focus () {
    U64 start = ui->focus_idx;
    while (true) {
        ui->focus_idx = (ui->focus_idx - 1);
        if (ui->focus_idx == UINT64_MAX) ui->focus_idx = ui->depth_first.count - 1;
        ui->focused = array_get(&ui->depth_first, ui->focus_idx);
        if (ui->focused->flags & UI_BOX_CAN_FOCUS) break;
        if (ui->focus_idx == start) break;
    }
}

static Bool ui_is_animating () {
    return ui->animation_running;
}

static Void ui_frame (Void(*app_build)(), F64 dt) {
    ui->dt = dt;
    ui->animation_running = false;

    array_iter (event, &events, *) {
        update_input_state(event);

        UiRect *root_clip = array_ref_last(&ui->clip_stack);
        root_clip->w = win_width;
        root_clip->h = win_height;

        if (ui->depth_first.count) {
            if ((ui->event->tag == EVENT_KEY_PRESS) && (event->key == SDLK_TAB)) {
                if (event->mods & SDL_KMOD_SHIFT) find_prev_focus();
                else                              find_next_focus();
            }
        }

        ui->deferred_layout_fns.count = 0;
        ui->depth_first.count = 0;

        ui->root = ui_box(0, "root") {
            ui_config_def_font(UI_CONFIG_FONT_NORMAL, font_get(ui->font_cache, str("data/fonts/NotoSans-Regular.ttf"), 12, false));
            ui_config_def_font(UI_CONFIG_FONT_BOLD,   font_get(ui->font_cache, str("data/fonts/NotoSans-Bold.ttf"), 12, false));
            ui_config_def_font(UI_CONFIG_FONT_MONO,   font_get(ui->font_cache, str("data/fonts/FiraMono-Bold Powerline.otf"), 12, true));
            ui_config_def_font(UI_CONFIG_FONT_ICONS,  font_get(ui->font_cache, str("data/fonts/icons.ttf"), 16, true));
            ui_config_def_f32(UI_CONFIG_ANIMATION_TIME_1, .3);
            ui_config_def_f32(UI_CONFIG_ANIMATION_TIME_2, 1);
            ui_config_def_f32(UI_CONFIG_ANIMATION_TIME_3, 2);
            ui_config_def_f32(UI_CONFIG_LINE_SPACING, 2);
            ui_config_def_f32(UI_CONFIG_SCROLLBAR_WIDTH, 10);
            ui_config_def_vec2(UI_CONFIG_PADDING_1, vec2(8, 8));
            ui_config_def_vec4(UI_CONFIG_RADIUS_1, vec4(4, 4, 4, 4));
            ui_config_def_vec4(UI_CONFIG_RADIUS_2, vec4(8, 8, 8, 8));
            ui_config_def_vec4(UI_CONFIG_BORDER_1_COLOR, vec4(0, 0, 0, .8));
            ui_config_def_vec4(UI_CONFIG_BORDER_2_COLOR, vec4(0, 0, 0, .3));
            ui_config_def_vec4(UI_CONFIG_BORDER_1_WIDTH, vec4(1, 1, 1, 1));
            ui_config_def_f32(UI_CONFIG_IN_SHADOW_1_WIDTH, 2);
            ui_config_def_vec4(UI_CONFIG_IN_SHADOW_1_COLOR, vec4(0, 0, 0, .4));
            ui_config_def_vec4(UI_CONFIG_SHADOW_1_COLOR, vec4(0, 0, 0, .8));
            ui_config_def_f32(UI_CONFIG_SHADOW_1_WIDTH, 1);
            ui_config_def_vec4(UI_CONFIG_BORDER_FOCUS_WIDTH, vec4(2, 2, 2, 2));
            ui_config_def_vec4(UI_CONFIG_BORDER_FOCUS_COLOR, vec4(1, 1, 1, .8));
            ui_config_def_vec4(UI_CONFIG_BLUE_TEXT, vec4(0, 0, 1, 1));
            ui_config_def_vec4(UI_CONFIG_RED_1, vec4(1, 0, 0, 1));
            ui_config_def_vec4(UI_CONFIG_RED_TEXT, vec4(1, 0, 0, 1));
            ui_config_def_vec4(UI_CONFIG_MAGENTA_1, hsva_to_rgba(vec4(.8, .4, 1, .8f)));
            ui_config_def_vec4(UI_CONFIG_BG_1, vec4(.15, .15, .15, 1));
            ui_config_def_vec4(UI_CONFIG_BG_2, vec4(.2, .2, .2, 1));
            ui_config_def_vec4(UI_CONFIG_BG_3, vec4(0, 0, 0, .4));
            ui_config_def_vec4(UI_CONFIG_BG_4, vec4(0, 0, 0, .6));
            ui_config_def_vec4(UI_CONFIG_BG_SELECTION, vec4(0, 1, 1, 1));
            ui_config_def_vec4(UI_CONFIG_FG_1, vec4(1, 1, 1, .8));
            ui_config_def_vec4(UI_CONFIG_FG_2, vec4(1, 1, 1, .5));
            ui_config_def_vec4(UI_CONFIG_FG_3, vec4(.3, .3, .3, .8));
            ui_config_def_vec4(UI_CONFIG_FG_4, vec4(.2, .2, .2, .8));
            ui_config_def_vec4(UI_CONFIG_TEXT_SELECTION, vec4(0, 0, 0, 1));
            ui_config_def_vec4(UI_CONFIG_TEXT_COLOR_1, vec4(1, 1, 1, 1));
            ui_config_def_vec4(UI_CONFIG_TEXT_COLOR_2, vec4(1, 1, 1, .4));
            ui_config_def_f32(UI_CONFIG_BLUR, 3);
            ui_config_def_vec4(UI_CONFIG_HIGHLIGHT, vec4(1, 1, 1, .05));
            ui_config_def_vec4(UI_CONFIG_SLIDER_KNOB, vec4(1, 1, 1, 1));

            ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, win_width, 0});
            ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, win_height, 0});
            ui_style_vec2(UI_PADDING, vec2(0, 0));
            ui_style_f32(UI_SPACING, 0);
            ui->root->rect.w = win_width;
            ui->root->rect.h = win_height;

            app_build();
        }

        // Remove unused boxes from the cache.
        map_iter (slot, &ui->box_cache) {
            Auto box = slot->val;
            if (box->gc_flag != ui->gc_flag) {
                array_push(&ui->free_boxes, box);
                free_box_data(box);

                // @todo This should be officially supported by the map.
                slot->hash = MAP_HASH_OF_TOMB_ENTRY;
                ui->box_cache.umap.count--;
                ui->box_cache.umap.tomb_count++;
                MAP_IDX--;
            }
        }

        ui->gc_flag = !ui->gc_flag;
    }

    apply_style_rules(ui->root);
    compute_layout(ui->root);

    array_iter (it, &ui->deferred_layout_fns) {
        it.fn(it.box);
        apply_style_rules(it.box);
        compute_layout(it.box->parent);
    }

    find_topmost_hovered_box(ui->root);
    draw_box(ui->root);

    ui->frame++;

    arena_pop_all(cast(Arena*, ui->frame_mem));
}

static Void ui_init () {
    Arena *perm_arena  = arena_new(mem_root, 1*KB);
    Arena *frame_arena = arena_new(mem_root, 64*KB);
    ui = mem_new(cast(Mem*, perm_arena), Ui);
    ui->perm_mem = cast(Mem*, perm_arena);
    ui->frame_mem = cast(Mem*, frame_arena);
    array_init(&ui->free_boxes, ui->perm_mem);
    array_init(&ui->box_stack, ui->perm_mem);
    array_init(&ui->clip_stack, ui->perm_mem);
    array_init(&ui->depth_first, ui->perm_mem);
    array_init(&ui->deferred_layout_fns, ui->perm_mem);
    map_init(&ui->box_cache, ui->perm_mem);
    map_init(&ui->pressed_keys, ui->perm_mem);
    map_init(&ui->box_data, ui->perm_mem);
    array_push_lit(&ui->clip_stack, .w=win_width, .h=win_height);
    ui->font_cache = font_cache_new(ui->perm_mem, flush_vertices, 64);
}

// =============================================================================
// App layer:
// =============================================================================
istruct (App) {
    Buf *buf1;
    Buf *buf2;

    Bool modal_shown;
    Bool popup_shown;

    U32 view;

    F32 slider;
    Bool toggle;

    I64 intval;
    Image image;

    F32 hue;
    F32 sat;
    F32 val;
    F32 alpha;
};

App *app;

static Void build_text_view () {
    UiBox *box = ui_text_box(str("text_box"), app->buf1, false);
    ui_style_box_size(box, UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 3./4, 0});
    ui_style_box_size(box, UI_HEIGHT, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});
    ui_style_box_vec2(box, UI_PADDING, (Vec2){8, 8});
}

static Void build_clock_view () {
    ui_box(UI_BOX_CLICK_THROUGH, "clock_box") {
        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_CHILDREN_SUM, 1, 0});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_CHILDREN_SUM, 1, 0});
        ui_style_u32(UI_ALIGN_X, UI_ALIGN_MIDDLE);
        ui_style_u32(UI_ALIGN_Y, UI_ALIGN_MIDDLE);

        Time time = os_get_wall_time();
        String time_str = astr_fmt(ui->frame_mem, "%02u:%02u:%02u", time.hours, time.minutes, time.seconds);
        UiBox *clock = ui_label("clock", time_str);
        ui_style_box_vec2(clock, UI_PADDING, vec2(40, 40));
        ui_style_box_from_config(clock, UI_FONT, UI_CONFIG_FONT_MONO);
        ui_style_box_f32(clock, UI_FONT_SIZE, 100.0);
    }
}

static Void build_color_view () {
    ui_box(0, "color_view") {
        ui_style_vec2(UI_PADDING, vec2(16.0, 16));
        ui_style_f32(UI_SPACING, 10.0);
        ui_style_u32(UI_AXIS, UI_AXIS_VERTICAL);
        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, 250, 1});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, 350, 1});

        ui_box(0, "graphical_pickers"){
            ui_style_f32(UI_SPACING, 8.0);
            ui_color_sat_val_picker(str("sat_val"), app->hue, &app->sat, &app->val);
            ui_color_hue_picker(str("hue"), &app->hue);
            ui_color_alpha_picker(str("alpha"), &app->alpha);
        }

        ui_box(UI_BOX_INVISIBLE, "spacer") ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PIXELS, 10, 1});

        ui_box(0, "hex_picker") {
            ui_style_u32(UI_ALIGN_Y, UI_ALIGN_MIDDLE);
            ui_label("label", str("HEX: "));
            ui_hspacer();
            ui_color_picker(str("picker"), COLOR_PICKER_HEX, &app->hue, &app->sat, &app->val, &app->alpha);
        }

        ui_box(0, "rgba_picker") {
            ui_style_u32(UI_ALIGN_Y, UI_ALIGN_MIDDLE);
            ui_label("label", str("RGBA: "));
            ui_hspacer();
            ui_color_picker(str("picker"), COLOR_PICKER_RGBA, &app->hue, &app->sat, &app->val, &app->alpha);
        }

        ui_box(0, "hsva_picker") {
            ui_style_u32(UI_ALIGN_Y, UI_ALIGN_MIDDLE);
            ui_label("label", str("HSVA: "));
            ui_hspacer();
            ui_color_picker(str("picker"), COLOR_PICKER_HSVA, &app->hue, &app->sat, &app->val, &app->alpha);
        }
    }
}

static Void build_tile_view () {
}

static Void build_misc_view () {
    ui_scroll_box("misc_view") {
        ui_tag("vbox");
        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 3./4, 0});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});
        ui_style_u32(UI_ANIMATION, UI_MASK_WIDTH);

        ui_style_rule("#misc_view") { ui_style_vec2(UI_PADDING, vec2(80, 16)); }

        ui_box(0, "box2_0") {
            ui_tag("hbox");
            ui_tag("item");

            ui_style_rule("#Foo4") { ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 0}); }
            ui_button("Foo4");
            ui_button("Foo5");
        }

        ui_box(0, "box2_1") {
            ui_tag("hbox");
            ui_tag("item");

            ui_button("Foo6");
            ui_button("Foo7");
        }

        ui_box(0, "box2_2") {
            ui_tag("hbox");
            ui_tag("item");
            ui_style_u32(UI_ALIGN_X, UI_ALIGN_MIDDLE);

            ui_button("Foo8");
            ui_button("Foo9");
        }

        ui_box(0, "box2_3") {
            ui_tag("hbox");
            ui_tag("item");
            ui_style_u32(UI_ALIGN_X, UI_ALIGN_END);

            ui_button("Foo10");
            ui_button("Foo11");
        }

        ui_scroll_box("box2_4") {
            ui_tag("hbox");
            ui_tag("item");
            for (U64 i = 0; i < cast(U64, 10*app->slider); ++i) {
                String str = astr_fmt(ui->frame_mem, "Foo_%lu", i);
                ui_button_str(str, str);
            }
        }

        ui_box_fmt(0, "slider") {
            ui_tag("hbox");
            ui_tag("item");
            ui_slider("Slider", &app->slider);
        }

        ui_box(0, "box2_5") {
            ui_tag("hbox");
            ui_tag("item");
            ui_style_u32(UI_ALIGN_X, UI_ALIGN_END);

            ui_icon("icon1", 16, get_icon(ICON_TODO));
            ui_icon("icon2", 16, get_icon(ICON_FIRE));
            ui_icon("icon3", 16, get_icon(ICON_EYE));
            ui_icon("icon4", 16, get_icon(ICON_ALARM));
        }

        ui_box(0, "box2_6") {
            ui_tag("hbox");
            ui_tag("item");

            ui_toggle("toggle", &app->toggle);
            ui_checkbox("checkbox", &app->toggle);

            UiBox *popup_button = ui_button("popup_button");
            if (app->popup_shown || popup_button->signals.clicked) {
                ui_tag_box(popup_button, "press");
                ui_popup("popup", &app->popup_shown, false, popup_button) {
                    build_color_view();
                }
            }

            ui_entry(str("entry"), app->buf2, 30, str("Type something..."));
        }

        ui_box(0, "box2_7") {
            ui_tag("hbox");
            ui_tag("item");

            ui_int_picker(str("int_picker"), &app->intval, 0, 10, 3);
            ui_int_picker(str("int_picker2"), &app->intval, 0, 10, 3);
        }

        ui_box(0, "box2_8") {
            ui_tag("hbox");
            ui_tag("item");

            UiBox *img = ui_image("image", &app->image, false, vec4(0,0,0,0), 200);
            UiBox *img_overlay = array_get(&img->children, 0);
            ui_style_box_f32(img_overlay, UI_OUTSET_SHADOW_WIDTH, 2);
            ui_style_box_vec4(img_overlay, UI_OUTSET_SHADOW_COLOR, vec4(0, 0, 0, 1));
        }
    }
}

static Void build_grid_view () {
    ui_scroll_box("second_view") {
        ui_tag("vbox");
        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 3./4, 0});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});
        ui_style_u32(UI_ANIMATION, UI_MASK_WIDTH);

        ui_style_rule("#second_view") ui_style_vec2(UI_PADDING, vec2(80, 16));

        ui_grid("test_grid") {
            ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PCT_PARENT, 3./4, 0});

            ui_grid_cell(0, 0, 3, 2) { ui_button("1"); }
            ui_grid_cell(3, 0, 5, 2) { ui_button("1"); }
            ui_grid_cell(0, 2, 3, 5) { ui_button("1"); }
            ui_grid_cell(3, 2, 5, 2) { ui_button("1"); }
            ui_grid_cell(3, 4, 3, 2) {
                ui_grid("test_grid") {
                    ui_grid_cell(0, 0, 3, 2);
                    ui_grid_cell(3, 0, 5, 2);
                    ui_grid_cell(0, 2, 3, 5);
                    ui_grid_cell(3, 2, 5, 2);
                    ui_grid_cell(3, 4, 3, 2);
                    ui_grid_cell(6, 4, 2, 2);
                    ui_grid_cell(3, 6, 5, 1);
                }
            }
            ui_grid_cell(6, 4, 2, 2) { ui_button("1"); }
            ui_grid_cell(3, 6, 5, 1) { ui_button("1"); }
        }
    }
}

static Void show_modal () {
    ui_modal("modal", &app->modal_shown) {
        build_color_view();
    }
}

static Void app_build () {
    ui_style_from_config(UI_BG_COLOR, UI_CONFIG_BG_1);

    ui_style_rule(".vbox") {
        ui_style_vec2(UI_PADDING, vec2(8, 8));
        ui_style_f32(UI_SPACING, 8.0);
        ui_style_u32(UI_AXIS, UI_AXIS_VERTICAL);
        ui_style_from_config(UI_BG_COLOR, UI_CONFIG_BG_1);
        ui_style_vec4(UI_BORDER_COLOR, vec4(0, 0, 0, .4));
        ui_style_f32(UI_EDGE_SOFTNESS, 0);
    }

    ui_style_rule(".hbox") {
        ui_style_vec2(UI_PADDING, vec2(8, 8));
        ui_style_f32(UI_SPACING, 8.0);
        ui_style_u32(UI_AXIS, UI_AXIS_HORIZONTAL);
        ui_style_u32(UI_ALIGN_Y, UI_ALIGN_MIDDLE);
        ui_style_vec4(UI_BG_COLOR, vec4(0, 0, 0, .2));
        ui_style_vec4(UI_BORDER_COLOR, vec4(0, 0, 0, .4));
        ui_style_f32(UI_EDGE_SOFTNESS, 0);
    }

    ui_style_rule(".hbox.item") {
        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_CHILDREN_SUM, 0, 0});
        ui_style_vec4(UI_BORDER_WIDTHS, vec4(1, 1, 1, 1));
    }

    ui_box(0, "sub_root") {
        ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});
        ui_style_size(UI_HEIGHT, (UiSize){UI_SIZE_PCT_PARENT, 1, 0});

        ui_box(0, "box1") {
            ui_tag("vbox");
            ui_style_vec4(UI_BORDER_WIDTHS, vec4(1, 0, 0, 0));
            ui_style_size(UI_WIDTH, (UiSize){.tag=UI_SIZE_PCT_PARENT, .value=1./4});
            ui_style_size(UI_HEIGHT, (UiSize){.tag=UI_SIZE_PCT_PARENT, .value=1});

            if (ui_button("Foo1")->signals.clicked && ui->event->key == SDL_BUTTON_LEFT) {
                app->modal_shown = !app->modal_shown;
            }

            if (app->modal_shown) {
                show_modal();
            }

            ui_style_rule("#Foo2") { ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PCT_PARENT, 1, 0}); }
            ui_style_rule("#Foo3") { ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, 80, 0}); }
            ui_style_rule("#Foo4") { ui_style_size(UI_WIDTH, (UiSize){UI_SIZE_PIXELS, 80, 0}); }

            UiBox *foo2 = ui_button("Foo2");
            UiBox *foo3 = ui_button("Foo3");
            UiBox *foo4 = ui_button("Foo4");
            UiBox *foo5 = ui_button("Foo5");

            if (foo2->signals.clicked && ui->event->key == SDL_BUTTON_LEFT) app->view = 0;
            if (foo3->signals.clicked && ui->event->key == SDL_BUTTON_LEFT) app->view = 1;
            if (foo4->signals.clicked && ui->event->key == SDL_BUTTON_LEFT) app->view = 2;
            if (foo5->signals.clicked && ui->event->key == SDL_BUTTON_LEFT) app->view = 3;

            switch (app->view) {
            case 0: ui_tag_box(foo2, "press"); break;
            case 1: ui_tag_box(foo3, "press"); break;
            case 2: ui_tag_box(foo4, "press"); break;
            case 3: ui_tag_box(foo5, "press"); break;
            }
        }

        switch (app->view) {
        case 0: build_misc_view(); break;
        case 1: build_grid_view(); break;
        case 2: build_text_view(); break;
        case 3: build_tile_view(); break;
        }
    }
}

static Void app_init () {
    app = mem_new(ui->perm_mem, App);
    app->view = 3;
    app->image = load_image("data/images/screenshot.png", false);
    app->slider = .5;
    app->buf1 = buf_new_from_file(ui->perm_mem, str("/home/zagor/Documents/test.txt"));
    app->buf2 = buf_new(ui->perm_mem, str("asdf"));
    app->hue = .3;
}

// @todo
//
// - tile widgets with tabs
// - shortcut picker
// - dropdown
// - time picker
// - refactor ui.c into multiple modules
// - when no text in single line entry cursor can move down
// - wrappers around the SDLK_ shit
// - sanitize pasted string for newlines if in single line mode
// - file picker
// - date picker
// - scrollbox for large homogenous lists
// - rich text (markup)
// - line wrapping in text box
// - support for tabs in text box
