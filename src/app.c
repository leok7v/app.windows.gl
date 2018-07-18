#include "std.h"
#include "app.h"
#include "mswin.h"

#pragma warning(disable: 4505)  // unreferenced local function has been removed (rect_width_compare)
#pragma warning(disable: 4100)  // unreferenced parameter

#pragma warning(push)
#pragma warning(disable: 4100; disable: 4244; disable: 4305; disable: 4018) 
#define STB_TRUETYPE_IMPLEMENTATION
#define STB_RECT_PACK_IMPLEMENTATION
// #include "stb_rect_pack.h" // already included by stb_truetype
#include "stb_truetype.h"
#pragma warning(pop)

#define IMPLEMENT_APP
#include "app.h"
#define IMPLEMENT_OGL
#include "ogl.h"
#define IMPLEMENT_MEM
#include "mem.h"

BEGIN_C

app_t* app;

#define GL_FRAMEBUFFER_SRGB_EXT           0x8DB9

#define SIZE_X  1024
#define SIZE_Y  768

static stbtt_packedchar chardata[6][128];

static int sx = SIZE_X, sy = SIZE_Y;

#define BITMAP_W 512
#define BITMAP_H 512
static unsigned char temp_bitmap[BITMAP_W][BITMAP_H];
static GLuint font_tex;

static float scale[2] = { 24.0f, 14.0f };

static int sf[6] = { 0,1,2, 0,1,2 };

static void load_fonts(app_t* a) {
    stbtt_pack_context pc;
    int bytes = 0;
    void* data = null;
    app->asset(a, "MONO_FONT", &data, &bytes);
    byte* ttf_buffer = (byte*)data;
    stbtt_PackBegin(&pc, temp_bitmap[0], BITMAP_W, BITMAP_H, 0, 1, NULL);
    for (int i = 0; i < 2; i++) {
        stbtt_PackSetOversampling(&pc, 1, 1);
        stbtt_PackFontRange(&pc, ttf_buffer, 0, scale[i], 32, 95, chardata[i*3+0]+32);
        stbtt_PackSetOversampling(&pc, 2, 2);
        stbtt_PackFontRange(&pc, ttf_buffer, 0, scale[i], 32, 95, chardata[i*3+1]+32);
        stbtt_PackSetOversampling(&pc, 3, 1);
        stbtt_PackFontRange(&pc, ttf_buffer, 0, scale[i], 32, 95, chardata[i*3+2]+32);
    }
    stbtt_PackEnd(&pc);
    gl_check(glGenTextures(1, &font_tex));
    gl_check(glBindTexture(GL_TEXTURE_2D, font_tex));
    gl_check(glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, BITMAP_W, BITMAP_H, 0, GL_ALPHA, GL_UNSIGNED_BYTE, temp_bitmap)); // this works only on NVIDIA Apple MBP
//  gl_check(glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, BITMAP_W, BITMAP_H, 0, GL_LUMINANCE8, GL_UNSIGNED_BYTE, temp_bitmap));
    gl_check(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    gl_check(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
}

static int black_on_white;

static void draw_init(int w, int h) {
    gl_check(glDisable(GL_CULL_FACE));
    gl_check(glDisable(GL_TEXTURE_2D));
    gl_check(glDisable(GL_LIGHTING));
    gl_check(glDisable(GL_DEPTH_TEST));
    gl_check(glViewport(0, 0, w, h));
    if (black_on_white) {
        gl_check(glClearColor(255, 192, 128, 0));
    } else {
        gl_check(glClearColor(0, 0, 0, 0));
    }
    gl_check(glClear(GL_COLOR_BUFFER_BIT));
    gl_check(glMatrixMode(GL_PROJECTION));
    gl_check(glLoadIdentity());
    float projection_matrix[16] = { 0 };
    glOrtho2D_np(projection_matrix, 0, (float)w, (float)h, 0); // near -1 far +1
    gl_check(glMultMatrixf(projection_matrix));
    gl_check(glMatrixMode(GL_MODELVIEW));
    gl_check(glLoadIdentity());
}

#define QUADS 1

#if QUADS

void drawBoxTC(float x0, float y0, float x1, float y1, float s0, float t0, float s1, float t1) {
    glTexCoord2f(s0, t0); glVertex2f(x0, y0); // cannot and should not call gl_check() inside glBegin/glEnd
    glTexCoord2f(s1, t0); glVertex2f(x1, y0);
    glTexCoord2f(s1, t1); glVertex2f(x1, y1);
    glTexCoord2f(s0, t1); glVertex2f(x0, y1);
}

#else

static void drawBoxTC(float x0, float y0, float x1, float y1, float s0, float t0, float s1, float t1) {
#if TRIANGLES
    gl_check(glTexCoord2f(s0,t0); glVertex2f(x0,y0));
    gl_check(glTexCoord2f(s1,t0); glVertex2f(x1,y0));
    gl_check(glTexCoord2f(s1,t1); glVertex2f(x1,y1));

    gl_check(glTexCoord2f(s1,t1); glVertex2f(x1,y1));
    gl_check(glTexCoord2f(s0,t1); glVertex2f(x0,y1));
    gl_check(glTexCoord2f(s0,t0); glVertex2f(x0,y0));
#endif
    GLfloat vertices[] = { x0,y0, x1,y0, x1,y1, x1,y1, x0,y1, x0,y0};
    GLfloat texture[]  = { s0,t0, s1,t0, s1,t1, s1,t1, s0,t1, s0,t0};
    gl_check(glEnableClientState(GL_VERTEX_ARRAY));
    gl_check(glEnableClientState(GL_TEXTURE_COORD_ARRAY));
    gl_check(glVertexPointer(2, GL_FLOAT, 0, vertices));
    gl_check(glTexCoordPointer(2, GL_FLOAT, 0, texture));
    gl_check(glDrawArrays(GL_TRIANGLES, 0, 6)); 
    gl_check(glDisableClientState(GL_TEXTURE_COORD_ARRAY));
    gl_check(glDisableClientState(GL_VERTEX_ARRAY));
}

#endif

static int integer_align;

static void print(int x, int y, int font, const char *text) {
    gl_check(glEnable(GL_TEXTURE_2D));
    gl_check(glBindTexture(GL_TEXTURE_2D, font_tex));
#ifdef QUADS
    glBegin(GL_QUADS);
#else
    glBegin(GL_TRIANGLES);
#endif
    float fx = (float)x;
    float fy = (float)y;
    while (*text) {
        stbtt_aligned_quad q;
        stbtt_GetPackedQuad(chardata[font], BITMAP_W, BITMAP_H, *text++, &fx, &fy, &q, font ? 0 : integer_align);
        drawBoxTC(q.x0, q.y0, q.x1, q.y1, q.s0, q.t0, q.s1, q.t1);
    }
    gl_check(glEnd());
}

static int font=2;
static int translating;
static int rotating;
static int srgb;
static float rotate_t, translate_t;
static int show_tex;

static void draw_world(int w, int h) {
    int sfont = sf[font];
    int x = 20;
    gl_check(glEnable(GL_BLEND));
    gl_check(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    if (black_on_white) {
        gl_check(glColor3f(0, 0, 0));
    } else {
        gl_check(glColor3f(1, 1, 1));
    }
    print(w - 100, 20, sfont, "_ [/] X");
    print(80, 30, sfont, "Controls:");
    print(100, 60, sfont, "S: toggle font size");
    print(100, 85, sfont, "O: toggle oversampling");
    print(100, 110, sfont, "T: toggle translation");
    print(100, 135, sfont, "R: toggle rotation");
    print(100, 160, sfont, "P: toggle pixel-snap (only non-oversampled)");
    print(100, 185, sfont, "G: toggle srgb gamma-correction");
    print(100, 210, sfont, black_on_white ? "B: toggle to white-on-black" : "B: toggle to black-on-white");
    print(100, 235, sfont, "V: view font texture");
    print(80, 300, sfont, "Current font:");
    if (!show_tex) {
        if (font < 3) {
            print(100, 350, sfont, "Font height: 24 pixels");
        } else {
            print(100, 350, sfont, "Font height: 14 pixels");
        }
    }
    if (font % 3 == 1) {
        print(100, 325, sfont, "2x2 oversampled text at 1:1");
    } else if (font % 3 == 2) {
        print(100, 325, sfont, "3x1 oversampled text at 1:1");
    } else if (integer_align) {
        print(100, 325, sfont, "1:1 text, one texel = one pixel, snapped to integer coordinates");
    } else {
        print(100, 325, sfont, "1:1 text, one texel = one pixel");
    }
    if (show_tex) {
        glBegin(GL_QUADS);
        drawBoxTC(200, 400, 200 + BITMAP_W, 300 + BITMAP_H, 0, 0, 1, 1);
        gl_check(glEnd());
    } else {
        glMatrixMode(GL_MODELVIEW);
        glTranslatef(200, 350, 0);
        if (translating) {
            x += (int)fmod(translate_t * 8, 30);
        }
        if (rotating) {
            glTranslatef(100, 150, 0);
            glRotatef(rotate_t * 2, 0, 0, 1);
            glTranslatef(-100, -150, 0);
        }
        print(x, 100, font, "This is a test");
        print(x, 130, font, "Now is the time for all good men to come to the aid of their country.");
        print(x, 160, font, "The quick brown fox jumps over the lazy dog.");
        print(x, 190, font, "0123456789");
    }
}

void app_pointer(int state, int index, int x, int y, float pressure, float proximity) {
    (void)(state, index, x, y, pressure, proximity);    
}

static void begin(app_t* a) {
    // both stdout and stderr redirected to OutputDebugString
    printf("Hello stdout\n");
    fprintf(stderr, "Hello stderr\n");
    for (int i = 0; i < a->argc; i++) {
        fprintf(stderr, "argv[%d]=%s\n", i, a->argv[i]);
    }
/*
    fprintf(stderr, "strerr(ENOMEM) = %s\n", strerr(ENOMEM));
    // for the range 1..42 (EWOULDBLOCK) errno is used 
    // winerror.h: ERROR_TOO_MANY_SEMAPHORES == 100 errno.h: EADDRINUSE 100
    fprintf(stderr, "strerr(EADDRINUSE) = %s\n", strerr(EADDRINUSE)); // errno.h: 100
    fprintf(stderr, "strerr(ERROR_TOO_MANY_SEMAPHORES) = %s\n", strerr(ERROR_TOO_MANY_SEMAPHORES)); // winerror: 100
    fprintf(stderr, "strerr(ERROR_INVALID_LIST_FORMAT) = %s\n", strerr(ERROR_INVALID_LIST_FORMAT)); // winerror: 153
    fprintf(stderr, "strerr(E_NOINTERFACE) = %s\n", strerr(E_NOINTERFACE));
*/
    load_fonts(a);
//  app->exit(a, 153);
}

static void changed(app_t* a) {  }

static void paint(app_t* a, int x, int y, int w, int h) {
/*
    gl_check(glViewport(0, 0, w, h));
    gl_check(glClearColor(0.3f, 0.4f, 0.4f, 1.0f));
    gl_check(glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE))
    gl_check(glDepthMask(GL_TRUE))
    gl_check(glDrawBuffer(GL_BACK));
    gl_check(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
    glBegin(GL_TRIANGLES);
    gl_check(glEnd());
*/
    draw_init(w, h);
    draw_world(w, h);
}

static void keyboard(app_t* a, int state, int key, int character) {
    (void)(state, key);
    traceln("ch=%c %d 0x%02X", character, character, character);
    switch (character) {
        case 'q': case 'Q':
            a->quit(a, 0);
            break;
        case 'f': case 'F':
            app->presentation = app->presentation == PRESENTATION_FULL_SCREEN ? PRESENTATION_NORMAL : PRESENTATION_FULL_SCREEN;
//          traceln("app->presentation=%d %s", app->presentation, app->presentation == PRESENTATION_FULL_SCREEN ? "FULL_SCREEN" : "NORMAL");
            app->w--;
            app->notify(app);
            break;
        case 'o': case 'O':
            font = (font + 1) % 3 + (font / 3) * 3;
            break;
        case 's': case 'S':
            font = (font + 3) % 6;
            break;
        case 't': case 'T':
            translating = !translating;
            translate_t = 0;
            break;
        case 'r': case 'R':
            rotating = !rotating;
            rotate_t = 0;
            break;
        case 'p': case 'P':
            integer_align = !integer_align;
            break;
        case 'g': case 'G':
            srgb = !srgb;
            if (srgb) {
                gl_check(glEnable(GL_FRAMEBUFFER_SRGB_EXT));
            } else {
                gl_check(glDisable(GL_FRAMEBUFFER_SRGB_EXT));
            }
            break;
        case 'v': case 'V':
            show_tex = !show_tex;
            break;
        case 'b': case 'B':
            black_on_white = !black_on_white;
            break;
        default: break;
    }
    a->invalidate(a);
}

static void touch(app_t* a, int state, int index, int x, int y, float pressure, float proximity) {}

static bool closing(app_t* a) { return true; }

static void end(app_t* a) {}

void app_init(app_t* a) {
    app = a;
    app->begin = begin;
    app->changed = changed;
    app->paint = paint;
    app->keyboard = keyboard;
    app->touch = touch;
    app->closing = closing;
    app->end = end;
    if (app->w <= 0 && app->h <= 0) {
        app->x = 50;
        app->y = 20;
        app->w = 1024;
        app->h = 768;
        app->min_w = 640;
        app->min_h = 480;
    } else {
        app->min_w = app->w;
        app->min_h = app->h;
    }
    app->max_w = 1920;
    app->max_h = 1080;
    app->title = "App";
}

static_init(app) {
    // this code will be executed before main() or WinMain() but not guaranteed to be executed in static libraries
}

END_C
