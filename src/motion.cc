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

#include <unistd.h>
#include <stdio.h>
#include <sys/time.h>
#include "motion.h"

#define NUM_FEATURES 400
#define MHI_DURATION 1000
#define OFFSET 30

static unsigned long get_msec();

bool stop_capture = false;
int pipefd[2];
cv::Mat frm;
pthread_t ptd;

bool start_capture()
{
	if(pipe(pipefd) == -1) {
		perror("failed to create synchronization pipe");
		return false;
	}

	int res = pthread_create(&ptd, 0, capture_thread, 0);
	if(res != 0) {
		fprintf(stderr, "Failed to create capturing thread: %s\n", strerror(res));
		return false;
	}
	return true;
}

void *capture_thread(void *arg)
{
	cv::VideoCapture cap(0);
	if(!cap.isOpened()) {
		fprintf(stderr, "failed to open video capture device\n");
	}

	cv::Mat next_frm, prev_frm, frm8b, prev_frm8b, colimg;

	cap >> next_frm;
	cv::flip(next_frm, next_frm, 1);
	colimg = next_frm.clone();
	cv::cvtColor(next_frm, next_frm, CV_RGB2GRAY);
	next_frm.convertTo(frm8b, CV_8UC1);

	while(!stop_capture) {
		prev_frm = next_frm.clone();
		prev_frm.convertTo(prev_frm8b, CV_8UC1);

		cap >> next_frm;
		cv::flip(next_frm, next_frm, 1);
		colimg = next_frm.clone();
		cv::cvtColor(next_frm, next_frm, CV_RGB2GRAY);
		next_frm.convertTo(frm8b, CV_8UC1);

		double direction = calculate_motion_dir(frm8b, prev_frm8b, colimg);
		write(pipefd[1], &direction, sizeof direction);
		frm = colimg.clone();
	}
	return 0;
}

double calculate_motion_dir(cv::Mat &frm8b, cv::Mat &prev_frm8b, cv::Mat &colimg)
{
	std::vector<cv::Point2f> prev_corners;
	std::vector<cv::Point2f> corners;
	std::vector<unsigned char> status;
	std::vector<float> err;

	cv::goodFeaturesToTrack(prev_frm8b, prev_corners, (float)NUM_FEATURES, 0.01, 3.0);
	cv::goodFeaturesToTrack(frm8b, corners, (float)NUM_FEATURES, 0.01, 3.0);
	cv::calcOpticalFlowPyrLK(prev_frm8b, frm8b, prev_corners, corners, status, err);

	cv::Point motion_vector = cv::Point((int)((double)colimg.cols / 2.0), (int)((double)colimg.rows / 2.0));

	for(size_t i=0; i<status.size(); i++) {
		if(!status[i])
			continue;

		cv::Point p, q;
		p.x = prev_corners[i].x;
		p.y = prev_corners[i].y;

		q.x = corners[i].x;
		q.y = corners[i].y;

		double angle = atan2((double)(q.y - p.y), (double)(q.x - p.x));
		double hypotenuse = sqrt(pow((double)(q.y - p.y), 2.0) + pow((double)(q.x - p.x), 2.0));

		p.x = (int)(q.x - hypotenuse * cos(angle));
		p.y = (int)(q.y - hypotenuse * sin(angle));

		cv::line(colimg, q, p, cv::Scalar(255, 255, 0), 1, CV_AA, 0);

		if(hypotenuse > 3) {
			cv::Point vec2d = cv::Point(q.x - p.x, q.y - p.y);
			motion_vector = cv::Point(motion_vector.x + vec2d.x, motion_vector.y + vec2d.y);
		}

/*
		q.x = (int)(p.x + cos(angle) + M_PI / 4.0);
		q.y = (int)(p.y + sin(angle) + M_PI / 4.0);

		cv::line(colimg, q, p, cv::Scalar(0, 0, 255), 1, CV_AA, 0);

		q.x = (int)(p.x + cos(angle) - M_PI / 4.0);
		q.y = (int)(p.y + sin(angle) - M_PI / 4.0);

		cv::line(colimg, q, p, cv::Scalar(255, 0, 0), 1, CV_AA, 0);*/

	}

	cv::Point ctr = cv::Point((int)((double)colimg.cols / 2.0), (int)((double)colimg.rows / 2.0));
	cv::Point xproj = cv::Point(motion_vector.x, ctr.y);

	cv::line(colimg, motion_vector, ctr, cv::Scalar(255, 0, 0), 3, CV_AA, 0);
	cv::line(colimg, xproj, ctr, cv::Scalar(0, 0, 255), 3, CV_AA, 0);

	return (xproj.x - ctr.x);
}

double calculate_orientation(cv::Mat &frm, cv::Mat &prev_frm)
{
	cv::Mat silhouette;
	cv::absdiff(frm, prev_frm, silhouette);

	cv::Mat sil8;
	cv::cvtColor(silhouette, silhouette, CV_RGB2GRAY);
	silhouette.convertTo(sil8, CV_8UC1);

	double max_val, min_val;
	cv::minMaxLoc(sil8, &min_val, &max_val);

	cv::threshold(sil8, sil8, 119, max_val, CV_THRESH_BINARY);

	cv::Mat mhi;
	sil8.convertTo(mhi, CV_32FC1, 1.0 / (float)UCHAR_MAX);

	cv::Mat mask = cv::Mat(mhi.size().width, mhi.size().height, CV_8UC1);
	cv::Mat orientation = mhi.clone();

	double duration = MHI_DURATION;
	unsigned long timestamp = get_msec() + duration;

	double min_delta = abs(timestamp);
	double max_delta = abs(timestamp) + 500.0;

	cv::updateMotionHistory(sil8, mhi, timestamp, duration);
	cv::calcMotionGradient(mhi, mask, orientation, min_delta, max_delta, 3);

	double global_orientation = cv::calcGlobalOrientation(orientation, mask, mhi, timestamp, duration);

	return global_orientation;
}


static unsigned long get_msec()
{
	static struct timeval tv0;
	struct timeval tv;

	gettimeofday(&tv, 0);
	if(tv0.tv_sec == 0 && tv0.tv_usec == 0) {
		tv0 = tv;
		return 0;
	}
	return (tv.tv_sec - tv0.tv_sec) * 1000 + (tv.tv_usec - tv0.tv_usec) / 1000;
}
