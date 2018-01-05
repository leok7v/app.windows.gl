#include "std.h"

#pragma warning(disable: 4505)  // unreferenced local function has been removed (rect_width_compare)

#pragma warning(push)
#pragma warning(disable: 4100; disable: 4244; disable: 4305; disable: 4018) 
#define STB_TRUETYPE_IMPLEMENTATION
#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
#include "stb_truetype.h"
#pragma warning(pop)

#define IMPLEMENT_APP
#include "app.h"
#define IMPLEMENT_OGL
#include "ogl.h"
#define IMPLEMENT_MEM
#include "mem.h"
#include "resource.h"


BEGIN_C

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

static void glCheck() {
#ifdef _DEBUG
    int error = glGetError();
    if (error != 0) {
        trace("glGetError()=%d", error);
        exit(1);
    }
#endif
}


static void load_fonts(void) {
    stbtt_pack_context pc;
/*
    char filename[256];
    char *win = getenv("windir");
    if (win == null) win = getenv("SystemRoot");
    if (win == null) {
        sprintf(filename, "arial.ttf");
    } else {
        sprintf(filename, "%s\\fonts\\arial.ttf", win);
    }
//  strcpy(filename, "c:/tmp/a.ttf");
    byte* ttf_buffer = (byte*)mem_map(filename, &bytes, true);
*/
    HMODULE module = GetModuleHandle(null);
    HRSRC resource_info = FindResourceA(module, MAKEINTRESOURCE(IDR_MONO_FONT), RT_RCDATA);
    HANDLE resource = LoadResource(module, resource_info);
    byte* ttf_buffer = (byte*)LockResource(resource);
    int bytes = (int)SizeofResource(module, resource_info);
    stbtt_PackBegin(&pc, temp_bitmap[0], BITMAP_W, BITMAP_H, 0, 1, NULL);
    double time = time_in_milliseconds();
    for (int i = 0; i < 2; i++) {
        stbtt_PackSetOversampling(&pc, 1, 1);
        stbtt_PackFontRange(&pc, ttf_buffer, 0, scale[i], 32, 95, chardata[i*3+0]+32);
        stbtt_PackSetOversampling(&pc, 2, 2);
        stbtt_PackFontRange(&pc, ttf_buffer, 0, scale[i], 32, 95, chardata[i*3+1]+32);
        stbtt_PackSetOversampling(&pc, 3, 1);
        stbtt_PackFontRange(&pc, ttf_buffer, 0, scale[i], 32, 95, chardata[i*3+2]+32);
    }
    stbtt_PackEnd(&pc);
    trace("%.1f", time_in_milliseconds() - time);
    UnlockResource(resource);
//  mem_unmap(ttf_buffer, bytes);

    glGenTextures(1, &font_tex);
    glBindTexture(GL_TEXTURE_2D, font_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, BITMAP_W, BITMAP_H, 0, GL_ALPHA, GL_UNSIGNED_BYTE, temp_bitmap); // only this works on NVIDIA Apple MBP
    // glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, BITMAP_W, BITMAP_H, 0, GL_LUMINANCE8, GL_UNSIGNED_BYTE, temp_bitmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

static int black_on_white;

static void draw_init() {
    glDisable(GL_CULL_FACE); glCheck();
    glDisable(GL_TEXTURE_2D); glCheck();
    glDisable(GL_LIGHTING); glCheck();
    glDisable(GL_DEPTH_TEST); glCheck();
    glViewport(0, 0, sx, sy); glCheck();
    if (black_on_white) {
        glClearColor(255, 255, 255, 0); glCheck();
    } else {
        glClearColor(0, 0, 0, 0); glCheck();
    }
    glClear(GL_COLOR_BUFFER_BIT); glCheck();
    glMatrixMode(GL_PROJECTION); glCheck();
    glLoadIdentity(); glCheck();
    float projection_matrix[16] = { 0 };
    glOrtho2D_np(projection_matrix, 0, (float)sx, (float)sy, 0); glCheck(); // near -1 far +1
    glMultMatrixf(projection_matrix); glCheck();
    glMatrixMode(GL_MODELVIEW); glCheck();
    glLoadIdentity(); glCheck();
}

//#define QUADS 
//#define TRIANGLES

#ifdef QUADS

void drawBoxTC(float x0, float y0, float x1, float y1, float s0, float t0, float s1, float t1) {
   glTexCoord2f(s0,t0); glVertex2f(x0,y0);
   glTexCoord2f(s1,t0); glVertex2f(x1,y0);
   glTexCoord2f(s1,t1); glVertex2f(x1,y1);
   glTexCoord2f(s0,t1); glVertex2f(x0,y1);
}

#else

static void drawBoxTC(float x0, float y0, float x1, float y1, float s0, float t0, float s1, float t1) {
#ifdef TRIANGLES
    glTexCoord2f(s0, t0); glVertex2f(x0, y0);
    glTexCoord2f(s1, t0); glVertex2f(x1, y0);
    glTexCoord2f(s1, t1); glVertex2f(x1, y1);

    glTexCoord2f(s1, t1); glVertex2f(x1, y1);
    glTexCoord2f(s0, t1); glVertex2f(x0, y1);
    glTexCoord2f(s0, t0); glVertex2f(x0, y0);
#else
    GLfloat vertices[] = { x0,y0, x1,y0, x1,y1, x1,y1, x0,y1, x0,y0};
    GLfloat texture[]  = { s0,t0, s1,t0, s1,t1, s1,t1, s0,t1, s0,t0};
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glTexCoordPointer(2, GL_FLOAT, 0, texture);
    glDrawArrays(GL_TRIANGLES, 0, 6); 
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
#endif
}

#endif

static int integer_align;

static void print(float x, float y, int font, char *text) {
   glEnable(GL_TEXTURE_2D);
   glBindTexture(GL_TEXTURE_2D, font_tex);
#ifdef QUADS
   glBegin(GL_QUADS);
#elif defined(TRIANGLES)
   glBegin(GL_TRIANGLES);
#endif
   while (*text) {
      stbtt_aligned_quad q;
      stbtt_GetPackedQuad(chardata[font], BITMAP_W, BITMAP_H, *text++, &x, &y, &q, font ? 0 : integer_align);
      drawBoxTC(q.x0,q.y0,q.x1,q.y1, q.s0,q.t0,q.s1,q.t1);
   }
#if defined(TRIANGLES) || defined(QUADS)
   glEnd();
#endif
}

static int font=2;
static int translating;
static int rotating;
static int srgb;
static float rotate_t, translate_t;
static int show_tex;


static void draw_bitmap2(HDC dc, int dx, int dy, int dw, int dh, int sx, int sy, int sw, int sh, int stride, GLuint texture_id, byte* buffer, int format) {
    RECT rc = {0};          
    GetClientRect(WindowFromDC(dc), &rc);
//  int width = rc.right - rc.left; 
    int height = rc.bottom-rc.top;
    glViewport(dx, height - 1 - dy - dh, dw, dh); glCheck();
    glLoadIdentity(); glCheck();
    glPixelZoom(1.0, 1.0); glCheck();
    glBindTexture(GL_TEXTURE_2D, texture_id); glCheck();
    glPixelStorei(GL_UNPACK_ROW_LENGTH, stride); glCheck();
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, sx); glCheck();
    // can use: buffer + (sx + sy * stride) instead of GL_UNPACK_ROW_LENGTH and GL_UNPACK_SKIP_PIXELS
    glPixelStorei(GL_UNPACK_SKIP_ROWS, sy); glCheck();
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, sw, sh, format, GL_UNSIGNED_BYTE, buffer); glCheck();
    glMatrixMode(GL_TEXTURE); glCheck();
    glScalef(1.0f / sw, 1.0f / sh, 1); glCheck();
    glBegin(GL_QUADS);  // Beginning of drawing frame
    glTexCoord2i( 0, sh);  glVertex2f(-1, -1);	// bottom left
    glTexCoord2i(sw, sh);  glVertex2f( 1, -1);	// bottom right
    glTexCoord2i(sw,  0);  glVertex2f( 1,  1);	// top right
    glTexCoord2i( 0,  0);  glVertex2f(-1,  1);	// top left
    glEnd(); glCheck();
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0); glCheck();
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0); glCheck();
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0); glCheck();
}

static void draw_world() {
   int sfont = sf[font];
   float x = 20;
   glEnable(GL_BLEND); glCheck();
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glCheck();
   if (black_on_white) {
      glColor3f(0, 0, 0);
   } else {
      glColor3f(1, 1, 1);
   }
   print(80, 30, sfont, "Controls:");
   print(100, 60, sfont, "S: toggle font size");
   print(100, 85, sfont, "O: toggle oversampling");
   print(100,110, sfont, "T: toggle translation");
   print(100,135, sfont, "R: toggle rotation");
   print(100,160, sfont, "P: toggle pixel-snap (only non-oversampled)");
   print(100,185, sfont, "G: toggle srgb gamma-correction");
   print(100,210, sfont, black_on_white ? "B: toggle to white-on-black" : "B: toggle to black-on-white");
   print(100,235, sfont, "V: view font texture");
   print(80, 300, sfont, "Current font:");
   if (!show_tex) {
      if (font < 3) {
         print(100, 350, sfont, "Font height: 24 pixels");
      } else {
         print(100, 350, sfont, "Font height: 14 pixels");
      }
   }
   if (font % 3==1) {
      print(100, 325, sfont, "2x2 oversampled text at 1:1");
   } else if (font % 3 == 2) {
      print(100, 325, sfont, "3x1 oversampled text at 1:1");
   } else if (integer_align) {
      print(100, 325, sfont, "1:1 text, one texel = one pixel, snapped to integer coordinates");
   } else {
      print(100, 325, sfont, "1:1 text, one texel = one pixel");
   }
   if (show_tex) {
      glBegin(GL_QUADS); glCheck();
      drawBoxTC(200,400, 200 + BITMAP_W, 300 + BITMAP_H, 0, 0, 1, 1);
      glEnd(); glCheck();
   } else {
      glMatrixMode(GL_MODELVIEW); glCheck();
      glTranslatef(200, 350, 0); glCheck();
      if (translating) {
         x += fmod(translate_t*8,30);
      }
      if (rotating) {
         glTranslatef(100, 150, 0); glCheck();
         glRotatef(rotate_t*2, 0, 0, 1); glCheck();
         glTranslatef(-100, -150, 0); glCheck();
      }
      print(x,100, font, "This is a test");
      print(x,130, font, "Now is the time for all good men to come to the aid of their country.");
      print(x,160, font, "The quick brown fox jumps over the lazy dog.");
      print(x,190, font, "0123456789");
   }
}

void app_size(app_size_t* sizes) {
    sizes->w     = 1024; sizes->h     =  768;
    sizes->min_w =  640; sizes->min_h =  640;
    sizes->max_w = 1920; sizes->max_h = 1080;
}

const char* app_title() { return "App"; }

void app_paint() {
    draw_init();
    draw_world();
}

void app_keyboard(int state, int key, int character) {
    (void)(state, key);
    printf("ch=%c %d 0x%02X\n", character, character, character);
    _flushall();
    if (character == 'q' || character == 'Q') {
        app_quit(0);
    }
    switch (character) {
        case 'o': case 'O':
            font = (font+1) % 3 + (font/3)*3;
            break;
        case 's': case 'S':
            font = (font+3) % 6;
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
            if (srgb)
                glEnable(GL_FRAMEBUFFER_SRGB_EXT);
            else
                glDisable(GL_FRAMEBUFFER_SRGB_EXT);
            break;
        case 'v': case 'V':
            show_tex = !show_tex;
            break;
        case 'b': case 'B':
            black_on_white = !black_on_white;
            break;
        default: break;
    }
    InvalidateRect(window, null, false);
}

void app_pointer(int state, int index, int x, int y, float pressure, float proximity) {
    (void)(state, index, x, y, pressure, proximity);    
}

int app_main(void* context, int argc, const char** argv) {
    (void)context; (void)argc; (void)argv;
    printf("Hello Windows!\n");       
    fprintf(stderr, "Hello sdterr\n");
//  _flushall();
    trace("Hello trace\n");         
    load_fonts();
//  _flushall();
//  app_exit(153);
    return 0;
}

static_init(app) {
    // this code will be executed before main() or WinMain()
}

END_C
