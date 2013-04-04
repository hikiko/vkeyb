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

#ifndef VKEYB_H_
#define VKEYB_H_

#include <X11/Xlib.h>

class VKeyb {
private:
	int num_glyphs;
	int visible_glyphs;
	float offset;
	unsigned int tex;

public:
	VKeyb();
	~VKeyb();

	void show() const;
	void move(float offs);

	int active_glyph() const;
	KeySym active_key() const;
};

#endif
