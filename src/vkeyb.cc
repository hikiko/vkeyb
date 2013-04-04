/* 
vkeyb - camera motion detection virtual keyboard
Copyright (C) 2012 Eleni Maria Stea <elene.mst@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <math.h>
#include <GL/gl.h>
#include <X11/keysym.h>
#include <imago2.h>
#include "vkeyb.h"

static KeySym charmap[] = {
	XK_Greek_ALPHA,
	XK_Greek_BETA,
	XK_Greek_GAMMA,
	XK_Greek_DELTA,
	XK_Greek_EPSILON,
	XK_Greek_ZETA,
	XK_Greek_ETA,
	XK_Greek_THETA,
	XK_Greek_IOTA,
	XK_Greek_KAPPA,
	XK_Greek_LAMBDA,
	XK_Greek_MU,
	XK_Greek_NU,
	XK_Greek_XI,
	XK_Greek_OMICRON,
	XK_Greek_PI,
	XK_Greek_RHO,
	XK_Greek_SIGMA,
	XK_Greek_TAU,
	XK_Greek_UPSILON,
	XK_Greek_PHI,
	XK_Greek_CHI,
	XK_Greek_PSI,
	XK_Greek_OMEGA,
	' ', '\b', '\n',
	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'
};

static unsigned int load_texture(const char *fname);

VKeyb::VKeyb()
{
	offset = 0;
	if(!(tex = load_texture("data/glyphs.png"))) {
		throw 1;
	}
	num_glyphs = 53;
	visible_glyphs = 24;
}

VKeyb::~VKeyb()
{
	glDeleteTextures(1, &tex);
}

void VKeyb::show() const
{
	float uoffs = floor(offset) / num_glyphs;
	float umax = (float)visible_glyphs / num_glyphs;

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, tex);

	glBegin(GL_QUADS);
	glColor3f(1, 1, 1);
	glTexCoord2f(uoffs, 1);
	glVertex2f(-1, -1);
	glTexCoord2f(uoffs + umax, 1);
	glVertex2f(1, -1);
	glTexCoord2f(uoffs + umax, 0);
	glVertex2f(1, 1);
	glTexCoord2f(uoffs, 0);
	glVertex2f(-1, 1);
	glEnd();

	glDisable(GL_TEXTURE_2D);

	float rect_width = 2.0 / visible_glyphs;

	glLineWidth(2.0);
	glBegin(GL_LINE_LOOP);
	glColor3f(1, 0, 0);
	glVertex2f(0, -1);
	glVertex2f(rect_width, -1);
	glVertex2f(rect_width, 1);
	glVertex2f(0, 1);
	glEnd();
}

void VKeyb::move(float offs)
{
	float tmp = offset + offs;

	if(tmp < 0.0) {
		offset = fmod(num_glyphs + tmp, num_glyphs);
	} else {
		offset = fmod(tmp, num_glyphs);
	}
}


static unsigned int load_texture(const char *fname)
{
	void *pixels;
	int xsz, ysz;
	unsigned int tex;

	if(!(pixels = img_load_pixels(fname, &xsz, &ysz, IMG_FMT_RGBA32))) {
		fprintf(stderr, "failed to load image: %s\n", fname);
		return 0;
	}

	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, xsz, ysz, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

	if(glGetError() != GL_NO_ERROR) {
		img_free_pixels(pixels);
		return 0;
	}

	img_free_pixels(pixels);
	return tex;
}

int VKeyb::active_glyph() const
{
	return (int)(offset + visible_glyphs / 2) % num_glyphs;
}

KeySym VKeyb::active_key() const
{
	return charmap[active_glyph()];
}
