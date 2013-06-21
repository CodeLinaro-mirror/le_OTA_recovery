/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include <fcntl.h>
#include <stdio.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>

#include <linux/fb.h>
#include <linux/kd.h>

#include <pixelflinger/pixelflinger.h>

#include "font_10x18.h"
#include "minui.h"

#include <linux/msm_mdp.h>
#include "linux/msm_ion.h"
#include <errno.h>

#define OVERLAY_EN

#if defined(RECOVERY_BGRA)
#define PIXEL_FORMAT GGL_PIXEL_FORMAT_BGRA_8888
#define PIXEL_SIZE   4
#define MDP_PIXEL_FORMAT MDP_BGRA_8888
#elif defined(RECOVERY_RGBX)
#define PIXEL_FORMAT GGL_PIXEL_FORMAT_RGBX_8888
#define PIXEL_SIZE   4
#define MDP_PIXEL_FORMAT MDP_RGBA_8888
#else
#define PIXEL_FORMAT GGL_PIXEL_FORMAT_RGB_565
#define PIXEL_SIZE   2
#define MDP_PIXEL_FORMAT MDP_RGB_565
#endif

#define NUM_BUFFERS 2
#define SIZE 0x7E9000

typedef struct {
    GGLSurface texture;
    unsigned cwidth;
    unsigned cheight;
    unsigned ascent;
} GRFont;

static GRFont *gr_font = 0;
static GGLContext *gr_context = 0;
static GGLSurface gr_font_texture;
static GGLSurface gr_framebuffer[NUM_BUFFERS];
static GGLSurface gr_mem_surface;
static unsigned gr_active_fb = 0;
static unsigned double_buffering = 0;

static int gr_fb_fd = -1;
static int gr_vt_fd = -1;

static struct fb_var_screeninfo vi;
static struct fb_fix_screeninfo fi;

inline size_t roundUpToPageSize(size_t x) {
    return (x + (PAGE_SIZE-1)) & ~(PAGE_SIZE-1);
}

#ifdef OVERLAY_EN
static int overlay_id;
unsigned char *mem_buf = NULL;
static int ion_fd = -1;
static int mem_fd = -1;

struct ion_handle_data handle_data;

int alloc_mem(unsigned int size, unsigned char **mem_buf)
{
    int result = -1;
    struct ion_fd_data fd_data;
    struct ion_allocation_data ionAllocData;
    fd_data.fd = 0;

    ion_fd = open("/dev/ion", O_RDWR|O_DSYNC);
    if (ion_fd < 0) {
            perror("ERROR: Can't open ion ");
            return -1;
    }

    ionAllocData.flags = 0;
    ionAllocData.len = size;
    ionAllocData.align = sysconf(_SC_PAGESIZE);
    ionAllocData.heap_mask =
            ION_HEAP(ION_IOMMU_HEAP_ID) |
            ION_HEAP(ION_CP_MM_HEAP_ID);

    result = ioctl(ion_fd, ION_IOC_ALLOC,  &ionAllocData);
    if(result){
            perror("ERROR: ION_IOC_ALLOC Failed ");
            return -1;
    } else {

        fd_data.handle = ionAllocData.handle;
        handle_data.handle = ionAllocData.handle;
        if(ioctl(ion_fd, ION_IOC_MAP, &fd_data)){
            perror("ERROR: ION_IOC_MAP Failed ");
        }else {
            *mem_buf = (unsigned char *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_data.fd, 0);
            mem_fd = fd_data.fd;
            if (*mem_buf == MAP_FAILED) {
                    perror("ERROR: mem_buf MAP_FAILED ");
                    return -1;
        }
        printf("MEM Allocation successful \n");
        }
    }

    return 0;
}

int free_mem (unsigned char *mem_buf) {

    int ret = 0;

	printf("Unmap and Free memory \n");

    munmap(mem_buf, SIZE);

    ret = ioctl(ion_fd, ION_IOC_FREE, &handle_data);
    if (ret < 0) {
        perror("free_mem failed ");
    }

    close(mem_fd);
    close(ion_fd);
    mem_buf = NULL;
    return 0;
}


static int Setup_RGBPipe( int fd)
{
    struct mdp_overlay overlay;
    int ret = 0;

    memset(&overlay, 0 , sizeof (struct mdp_overlay));

	/* Fill Overlay Data */

	overlay.src.width  = gr_framebuffer[0].width;
	overlay.src.height = gr_framebuffer[0].height;
	overlay.src.format = MDP_PIXEL_FORMAT;
	overlay.src_rect.x = 0;
	overlay.src_rect.y = 0;
	overlay.src_rect.w = gr_framebuffer[0].width;
	overlay.src_rect.h = gr_framebuffer[0].height;
	overlay.dst_rect.x = 0;
	overlay.dst_rect.y = 0;
	overlay.dst_rect.w = gr_framebuffer[0].width;
	overlay.dst_rect.h = gr_framebuffer[0].height;
	overlay.z_order = 0;
	overlay.alpha = 0xFF;
	overlay.is_fg = 0;
	overlay.transp_mask = MDP_TRANSP_NOP;
	overlay.flags = 0;
	overlay.id = MSMFB_NEW_REQUEST;

	ret = ioctl(fd, MSMFB_OVERLAY_SET, &overlay);
	if (ret < 0) {
        fprintf(stderr, " Overlay Set Failed ret: %d errno: %d \n", ret, errno);
        ret = -1;
        goto err;
	}
	overlay_id = overlay.id;
	fprintf(stderr, " Setup_RGBPipe width: %d height: %d ovid: %d \n",
		overlay.src.width, overlay.src.height, overlay_id);

err:
	return ret;
}

int Display_Frame(int fd, int memory_id) {

	int ret = 0;
	struct msmfb_overlay_data ovdata;
	struct mdp_display_commit ext_commit;

	if ( (fd != 0) && (memory_id != 0) && (overlay_id != 0)) {
		ovdata.id = overlay_id;
		ovdata.data.flags = 0;
		ovdata.data.offset = 0;
		ovdata.data.memory_id = memory_id;

		ret = ioctl(fd, MSMFB_OVERLAY_PLAY, &ovdata);
		if (ret < 0) {
			perror("Overlay Play Failed ");
			ret = -1;
			goto err;
		}

		memset(&ext_commit, 0, sizeof(struct mdp_display_commit));
		ext_commit.flags = MDP_DISPLAY_COMMIT_OVERLAY;
		ext_commit.wait_for_finish = 1;
		if (ioctl(fd, MSMFB_DISPLAY_COMMIT, &ext_commit) < 0) {
			perror("ERROR: Display MSMFB_DISPLAY_COMMIT failed!");
			ret = -1;
			goto err;
		}
	}
err:
	return ret;
}

int Clear_MDPSetup(int fd) {

    int ret = 0;
	struct mdp_display_commit ext_commit;

	if ( (fd != 0) && (overlay_id != 0)) {
		ret = ioctl(fd, MSMFB_OVERLAY_UNSET, &overlay_id);
		if (overlay_id < 0) {
			perror("Overlay Unset Failed");
			ret = -1;
			goto err;
		}

		memset(&ext_commit, 0, sizeof(struct mdp_display_commit));
		ext_commit.flags = MDP_DISPLAY_COMMIT_OVERLAY;
		ext_commit.wait_for_finish = 1;
		if (ioctl(fd, MSMFB_DISPLAY_COMMIT, &ext_commit) < 0) {
			perror("ERROR: Clear MSMFB_DISPLAY_COMMIT failed!");
			ret = -1;
			goto err;
		}
		overlay_id = -1;
	}
err:
    return ret;
}
#endif

static int get_framebuffer(GGLSurface *fb)
{
    int fd;
    void *bits;

    fd = open("/dev/graphics/fb0", O_RDWR);
    if (fd < 0) {
        perror("cannot open fb0");
        return -1;
    }

    if (ioctl(fd, FBIOGET_VSCREENINFO, &vi) < 0) {
        perror("failed to get fb0 info");
        close(fd);
        return -1;
    }

    vi.bits_per_pixel = PIXEL_SIZE * 8;
    if (PIXEL_FORMAT == GGL_PIXEL_FORMAT_BGRA_8888) {
      vi.red.offset     = 8;
      vi.red.length     = 8;
      vi.green.offset   = 16;
      vi.green.length   = 8;
      vi.blue.offset    = 24;
      vi.blue.length    = 8;
      vi.transp.offset  = 0;
      vi.transp.length  = 8;
    } else if (PIXEL_FORMAT == GGL_PIXEL_FORMAT_RGBX_8888) {
      vi.red.offset     = 24;
      vi.red.length     = 8;
      vi.green.offset   = 16;
      vi.green.length   = 8;
      vi.blue.offset    = 8;
      vi.blue.length    = 8;
      vi.transp.offset  = 0;
      vi.transp.length  = 8;
    } else { /* RGB565*/
      vi.red.offset     = 11;
      vi.red.length     = 5;
      vi.green.offset   = 5;
      vi.green.length   = 6;
      vi.blue.offset    = 0;
      vi.blue.length    = 5;
      vi.transp.offset  = 0;
      vi.transp.length  = 0;
    }
    if (ioctl(fd, FBIOPUT_VSCREENINFO, &vi) < 0) {
        perror("failed to put fb0 info");
        close(fd);
        return -1;
    }

    if (ioctl(fd, FBIOGET_FSCREENINFO, &fi) < 0) {
        perror("failed to get fb0 info");
        close(fd);
        return -1;
    }

    size_t size = roundUpToPageSize(vi.yres * fi.line_length) * 2;
    bits = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (bits == MAP_FAILED) {
        perror("failed to mmap framebuffer");
        close(fd);
        return -1;
    }

    fb->version = sizeof(*fb);
    fb->width = vi.xres;
    fb->height = vi.yres;
    fb->stride = fi.line_length/PIXEL_SIZE;
    fb->data = bits;
    fb->format = PIXEL_FORMAT;
    memset(fb->data, 0, roundUpToPageSize(vi.yres * fi.line_length));

    fb++;

    /* check if we can use double buffering */
    if (vi.yres * fi.line_length * 2 > fi.smem_len)
        return fd;

    double_buffering = 1;

    fb->version = sizeof(*fb);
    fb->width = vi.xres;
    fb->height = vi.yres;
    fb->stride = fi.line_length/PIXEL_SIZE;
    fb->data = (void*) (((unsigned) bits) + roundUpToPageSize(vi.yres * fi.line_length));
    fb->format = PIXEL_FORMAT;
    memset(fb->data, 0, roundUpToPageSize(vi.yres * fi.line_length));

    return fd;
}

static void get_memory_surface(GGLSurface* ms) {
  ms->version = sizeof(*ms);
  ms->width = vi.xres;
  ms->height = vi.yres;
  ms->stride = fi.line_length/PIXEL_SIZE;
  ms->data = malloc(fi.line_length * vi.yres);
  ms->format = PIXEL_FORMAT;
}

static void set_active_framebuffer(unsigned n)
{
    if (n > 1 || !double_buffering) return;
    vi.yres_virtual = vi.yres * NUM_BUFFERS;
    vi.yoffset = n * vi.yres;
    vi.bits_per_pixel = PIXEL_SIZE * 8;
}

void gr_flip(void)
{
#ifndef OVERLAY_EN
    GGLContext *gl = gr_context;

    /* swap front and back buffers */
    if (double_buffering)
        gr_active_fb = (gr_active_fb + 1) & 1;

    /* copy data from the in-memory surface to the buffer we're about
     * to make active. */
    memcpy(gr_framebuffer[gr_active_fb].data, gr_mem_surface.data,
           fi.line_length * vi.yres);

    /* inform the display driver */
    set_active_framebuffer(gr_active_fb);
#else
	memcpy(mem_buf, gr_mem_surface.data,
		   fi.line_length * vi.yres);
	Display_Frame(gr_fb_fd, mem_fd);
#endif
}

void gr_color(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    GGLContext *gl = gr_context;
    GGLint color[4];
    color[0] = ((r << 8) | r) + 1;
    color[1] = ((g << 8) | g) + 1;
    color[2] = ((b << 8) | b) + 1;
    color[3] = ((a << 8) | a) + 1;
    gl->color4xv(gl, color);
}

int gr_measure(const char *s)
{
    return gr_font->cwidth * strlen(s);
}

void gr_font_size(int *x, int *y)
{
    *x = gr_font->cwidth;
    *y = gr_font->cheight;
}

int gr_text(int x, int y, const char *s)
{
    GGLContext *gl = gr_context;
    GRFont *font = gr_font;
    unsigned off;

    y -= font->ascent;

    gl->bindTexture(gl, &font->texture);
    gl->texEnvi(gl, GGL_TEXTURE_ENV, GGL_TEXTURE_ENV_MODE, GGL_REPLACE);
    gl->texGeni(gl, GGL_S, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->texGeni(gl, GGL_T, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->enable(gl, GGL_TEXTURE_2D);

    while((off = *s++)) {
        off -= 32;
        if (off < 96) {
            gl->texCoord2i(gl, (off * font->cwidth) - x, 0 - y);
            gl->recti(gl, x, y, x + font->cwidth, y + font->cheight);
        }
        x += font->cwidth;
    }

    return x;
}

void gr_texticon(int x, int y, gr_surface icon) {
    if (gr_context == NULL || icon == NULL) {
        return;
    }
    GGLContext* gl = gr_context;

    gl->bindTexture(gl, (GGLSurface*) icon);
    gl->texEnvi(gl, GGL_TEXTURE_ENV, GGL_TEXTURE_ENV_MODE, GGL_REPLACE);
    gl->texGeni(gl, GGL_S, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->texGeni(gl, GGL_T, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->enable(gl, GGL_TEXTURE_2D);

    int w = gr_get_width(icon);
    int h = gr_get_height(icon);

    gl->texCoord2i(gl, -x, -y);
    gl->recti(gl, x, y, x+gr_get_width(icon), y+gr_get_height(icon));
}

void gr_fill(int x, int y, int w, int h)
{
    GGLContext *gl = gr_context;
    gl->disable(gl, GGL_TEXTURE_2D);
    gl->recti(gl, x, y, w, h);
}

void gr_blit(gr_surface source, int sx, int sy, int w, int h, int dx, int dy) {
    if (gr_context == NULL || source == NULL) {
        return;
    }
    GGLContext *gl = gr_context;

    gl->bindTexture(gl, (GGLSurface*) source);
    gl->texEnvi(gl, GGL_TEXTURE_ENV, GGL_TEXTURE_ENV_MODE, GGL_REPLACE);
    gl->texGeni(gl, GGL_S, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->texGeni(gl, GGL_T, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->enable(gl, GGL_TEXTURE_2D);
    gl->texCoord2i(gl, sx - dx, sy - dy);
    gl->recti(gl, dx, dy, dx + w, dy + h);
}

unsigned int gr_get_width(gr_surface surface) {
    if (surface == NULL) {
        return 0;
    }
    return ((GGLSurface*) surface)->width;
}

unsigned int gr_get_height(gr_surface surface) {
    if (surface == NULL) {
        return 0;
    }
    return ((GGLSurface*) surface)->height;
}

static void gr_init_font(void)
{
    GGLSurface *ftex;
    unsigned char *bits, *rle;
    unsigned char *in, data;

    gr_font = calloc(sizeof(*gr_font), 1);
    ftex = &gr_font->texture;

    bits = malloc(font.width * font.height);

    ftex->version = sizeof(*ftex);
    ftex->width = font.width;
    ftex->height = font.height;
    ftex->stride = font.width;
    ftex->data = (void*) bits;
    ftex->format = GGL_PIXEL_FORMAT_A_8;

    in = font.rundata;
    while((data = *in++)) {
        memset(bits, (data & 0x80) ? 255 : 0, data & 0x7f);
        bits += (data & 0x7f);
    }

    gr_font->cwidth = font.cwidth;
    gr_font->cheight = font.cheight;
    gr_font->ascent = font.cheight - 2;
}

int gr_init(void)
{
    gglInit(&gr_context);
    GGLContext *gl = gr_context;

    gr_init_font();
    gr_vt_fd = open("/dev/tty0", O_RDWR | O_SYNC);
    if (gr_vt_fd < 0) {
        // This is non-fatal; post-Cupcake kernels don't have tty0.
        perror("can't open /dev/tty0");
    } else if (ioctl(gr_vt_fd, KDSETMODE, (void*) KD_GRAPHICS)) {
        // However, if we do open tty0, we expect the ioctl to work.
        perror("failed KDSETMODE to KD_GRAPHICS on tty0");
        gr_exit();
        return -1;
    }

    gr_fb_fd = get_framebuffer(gr_framebuffer);
    if (gr_fb_fd < 0) {
        gr_exit();
        return -1;
    }

    get_memory_surface(&gr_mem_surface);

    fprintf(stderr, "framebuffer: fd %d (%d x %d)\n",
            gr_fb_fd, gr_framebuffer[0].width, gr_framebuffer[0].height);

        /* start with 0 as front (displayed) and 1 as back (drawing) */
    gr_active_fb = 0;
    set_active_framebuffer(0);
    gl->colorBuffer(gl, &gr_mem_surface);

    gl->activeTexture(gl, 0);
    gl->enable(gl, GGL_BLEND);
    gl->blendFunc(gl, GGL_SRC_ALPHA, GGL_ONE_MINUS_SRC_ALPHA);

    gr_fb_blank(true);
    gr_fb_blank(false);

#ifdef OVERLAY_EN
	alloc_mem(SIZE, &mem_buf);
	Setup_RGBPipe(gr_fb_fd);
#endif

    return 0;
}

void gr_exit(void)
{

#ifdef OVERLAY_EN
	Clear_MDPSetup(gr_fb_fd);
	free_mem(mem_buf);
#endif
    close(gr_fb_fd);
    gr_fb_fd = -1;

    free(gr_mem_surface.data);

    ioctl(gr_vt_fd, KDSETMODE, (void*) KD_TEXT);
    close(gr_vt_fd);
    gr_vt_fd = -1;
}

int gr_fb_width(void)
{
    return gr_framebuffer[0].width;
}

int gr_fb_height(void)
{
    return gr_framebuffer[0].height;
}

gr_pixel *gr_fb_data(void)
{
    return (unsigned short *) gr_mem_surface.data;
}

void gr_fb_blank(bool blank)
{
    int ret;

    ret = ioctl(gr_fb_fd, FBIOBLANK, blank ? FB_BLANK_POWERDOWN : FB_BLANK_UNBLANK);
    if (ret < 0)
        perror("ioctl(): blank");
}
