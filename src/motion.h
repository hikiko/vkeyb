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

#ifndef MOTION_H_
#define MOTION_H_

#include <opencv2/opencv.hpp>

extern bool stop_capture;
extern int pipefd[2];
extern cv::Mat frm;
extern pthread_t ptd;

bool start_capture();
void *capture_thread(void *arg);
double calculate_motion_dir(cv::Mat &frm8b, cv::Mat &prev_frm8b, cv::Mat &colimg);
double calculate_orientation(cv::Mat &frm, cv::Mat &prev_frm);

#endif /* MOTION_H_ */
