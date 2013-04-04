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

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glut.h>

#include <opencv2/opencv.hpp>

#include <imago2.h>
#include "vkeyb.h"
#include "motion.h"

int init(void);
void shutdown(void);
int create_window(int xsz, int ysz);
void display(void);
void show_frame(float frm_width);
int handle_event(XEvent *xev);
void reshape(int w, int h);
void keyb(int key, int pressed);
void send_key(KeySym key);
void motion(int x, int y);
void cam_motion(double orient);
void button(int x, int y, int bn, int state);
void activate(int enter);

Display *dpy;		/* X11 display structure (x server connection) */
Window win;			/* X window */
GLXContext ctx;		/* OpenGL context */

unsigned int frm_tex;
bool tex_created;

double size = 0.1;
VKeyb *vkeyb;

int must_redraw;

static double orient = 0.0;


int main (int argc, char** argv)
{
	glutInit(&argc, argv);

	if(init() == -1) {
		return 1;
	}
	atexit(shutdown);

	glEnable(GL_CULL_FACE);

	for(;;) {
		fd_set fdset;

		FD_ZERO(&fdset);

		//retreive the X server socket
		int xsock = ConnectionNumber(dpy);
		FD_SET(xsock, &fdset);
		FD_SET(pipefd[0], &fdset);

		int maxfd = (xsock > pipefd[0] ? xsock : pipefd[0]) + 1;
		select(maxfd, &fdset, 0, 0, 0);

		if(FD_ISSET(pipefd[0], &fdset)) {
			// process all pending events ...
			while(XPending(dpy)) {
				XEvent xev;
				XNextEvent(dpy, &xev);
				handle_event(&xev);
			}
		}

		if(FD_ISSET(pipefd[0], &fdset)) {
			if(read(pipefd[0], &orient, sizeof orient) < (int)sizeof orient) {
				fprintf(stderr, "read from pipe failed\n");
			}
			else {
				glBindTexture(GL_TEXTURE_2D, frm_tex);
				if(!tex_created) {
					glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, frm.cols, frm.rows, 0, GL_BGR, GL_UNSIGNED_BYTE, frm.data);
					tex_created = true;
				}
				else {
					glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frm.cols, frm.rows, GL_BGR, GL_UNSIGNED_BYTE, frm.data);
				}

				cam_motion(orient);
				must_redraw = true;
			}
		}

		// ... and then do a single redisplay if needed
		if(must_redraw) {
			display();
		}

	}

	shutdown();
	return 0;
}

int init(void)
{
	Screen *scr;
	int width, height;

	if(!(dpy = XOpenDisplay(0))) {
		fprintf(stderr, "failed to connect to the X server\n");
		return -1;
	}
	scr = ScreenOfDisplay(dpy, DefaultScreen(dpy));

	width = WidthOfScreen(scr);
	height = width / 16;

	if(create_window(width, height) == -1) {
		XCloseDisplay(dpy);
		return -1;
	}
	XMoveWindow(dpy, win, 0, HeightOfScreen(scr) - height);

	try {
		vkeyb = new VKeyb;
	}
	catch(...) {
		fprintf(stderr, "failed to initialize virtual keyboard\n");
		return -1;
	}

	// register a passive grab
	Window root = RootWindow(dpy, DefaultScreen(dpy));
	XGrabKey(dpy, XKeysymToKeycode(dpy, 'e'), ControlMask, root, False, GrabModeAsync, GrabModeAsync);

	// create texture
	glGenTextures(1, &frm_tex);
	glBindTexture(GL_TEXTURE_2D, frm_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	// start the capturing thread
	if(!start_capture())
		return -1;

	return 0;
}

void shutdown(void)
{
	glXMakeCurrent(dpy, None, 0);
	glXDestroyContext(dpy, ctx);
	XDestroyWindow(dpy, win);
	XCloseDisplay(dpy);
}

int create_window(int xsz, int ysz)
{
	int scr;
	Window root;
	XSetWindowAttributes xattr;
	XVisualInfo *vis;
	unsigned int attr_valid;
	long evmask;

	int glattr[] = {
		GLX_RGBA,
		GLX_RED_SIZE, 8,
		GLX_GREEN_SIZE, 8,
		GLX_BLUE_SIZE, 8,
		GLX_DEPTH_SIZE, 24,
		GLX_DOUBLEBUFFER,
		None
	};

	scr = DefaultScreen(dpy);
	root = RootWindow(dpy, scr);

	if(!(vis = glXChooseVisual(dpy, scr, glattr))) {
		fprintf(stderr, "failed to find a suitable visual\n");
		return -1;
	}

	if(!(ctx = glXCreateContext(dpy, vis, 0, True))) {
		fprintf(stderr, "failed to create OpenGL context\n");
		XFree(vis);
		return -1;
	}

	xattr.background_pixel = xattr.border_pixel = BlackPixel(dpy, scr);
	xattr.colormap = XCreateColormap(dpy, root, vis->visual, AllocNone);
	xattr.override_redirect = True;
	attr_valid = CWColormap | CWBackPixel | CWBorderPixel | CWOverrideRedirect;

	if(!(win = XCreateWindow(dpy, root, 0, 0, xsz, ysz, 0, vis->depth, InputOutput,
					vis->visual, attr_valid, &xattr))) {
		fprintf(stderr, "failed to create X window\n");
		glXDestroyContext(dpy, ctx);
		XFree(vis);
		return -1;
	}
	XFree(vis);

	evmask = StructureNotifyMask | VisibilityChangeMask | KeyPressMask | PointerMotionMask |
		ButtonPressMask | ButtonReleaseMask | ExposureMask | EnterWindowMask | LeaveWindowMask;
	XSelectInput(dpy, win, evmask);

	XMapWindow(dpy, win);

	glXMakeCurrent(dpy, win, ctx);
	return 0;
}


void display(void)
{
	glClearColor(1, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	vkeyb->show();
	show_frame(0.1);

	glRasterPos2i(-1, -1);

	char buf[64];
	snprintf(buf, sizeof buf, "%f %s", orient, orient > 0 ? "->" : orient < 0 ? "<-" : " ");
	char *ptr = buf;
	while(*ptr) {
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *ptr++);
	}

	glXSwapBuffers(dpy, win);

	must_redraw = 0;
	assert(glGetError() == GL_NO_ERROR);
}

void show_frame(float frm_width)
{
	frm_width *= 2;

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, frm_tex);

	glBegin(GL_QUADS);
	glColor3f(1.0, 1.0, 1.0);
	glTexCoord2f(0, 1); glVertex2f(-1, -1);
	glTexCoord2f(1, 1); glVertex2f(frm_width - 1, -1);
	glTexCoord2f(1, 0); glVertex2f(frm_width - 1, 1);
	glTexCoord2f(0, 0); glVertex2f(-1, 1);
	glEnd();

	glDisable(GL_TEXTURE_2D);
}


int handle_event(XEvent *xev)
{
	static int mapped;
	KeySym sym;

	switch(xev->type) {
	case ConfigureNotify:
		reshape(xev->xconfigure.width, xev->xconfigure.height);
		break;

	case MapNotify:
		mapped = 1;
		break;
	case UnmapNotify:
		mapped = 0;
		break;

	case Expose:
		if(mapped && xev->xexpose.count == 0) {
			display();
		}
		break;

	case KeyPress:
		sym = XLookupKeysym(&xev->xkey, 0);
		keyb(sym, 1);
		break;

	case MotionNotify:
		motion(xev->xmotion.x, xev->xmotion.y);
		break;

	case ButtonPress:
		button(xev->xbutton.x, xev->xbutton.y, xev->xbutton.button, 1);
		break;
	case ButtonRelease:
		button(xev->xbutton.x, xev->xbutton.y, xev->xbutton.button, 0);
		break;

	case EnterNotify:
		activate(1);
		break;
	case LeaveNotify:
		activate(0);
		break;

	default:
		break;
	}

	return 0;
}


void reshape(int w, int h)
{
	glViewport(0, 0, w, h);
}

void keyb(int key, int pressed)
{
	if(!pressed) {
		return;
	}

	switch (key) {
	case XK_Escape:
		exit(0);

	case 'e':
		printf("sending key: %c\n", (char)vkeyb->active_key());
		send_key(vkeyb->active_key());
		break;

	}
}

void send_key(KeySym key)
{
	Window win;
	XEvent ev;
	int junk;

	XGetInputFocus(dpy, &win, &junk);

	memset(&ev, 0, sizeof ev);
	ev.type = KeyPress;
	ev.xkey.window = win;
	ev.xkey.keycode = XKeysymToKeycode(dpy, key);
	ev.xkey.state = 0;
	ev.xkey.time = CurrentTime;

	XSendEvent(dpy, InputFocus, False, NoEventMask, &ev);
}

static int prev_x = -1;

void motion(int x, int y)
{
	if(prev_x == -1) {
		prev_x = x;
	}

	vkeyb->move((x - prev_x) / 25.0);
	prev_x = x;

	must_redraw = 1;
}

void cam_motion(double orient)
{
	if(orient > 0) {
		vkeyb->move(1);
	}

	if(orient < 0) {
		vkeyb->move(-1);
	}

	must_redraw = 1;
}

void button(int x, int y, int bn, int state)
{
	if(bn == 3 && state) {
		exit(0);
	}
}


void activate(int enter)
{
	prev_x = -1;
}
