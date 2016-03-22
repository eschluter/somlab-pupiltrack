/* Erik Schluter
 * 1/28/2016
 */
#pragma once

#include "opencv2/opencv.hpp"
#include <iostream>
#include <vector>
#include <windows.h>

using namespace std;
using namespace cv;

class FrameProcessing
{
	private:
		Mat						frame;  // raw image
		Mat						frame2;
		Mat						f_frame; // copied image for feature extraction

	private:
		vector<vector<Point> >	contours;
		vector<Vec4i>			hierarchy;
		int						thresh;
		const int				bufsize;
		HANDLE					hStdout;
		HANDLE					hStdin;

	public:
		FrameProcessing();
		FrameProcessing(HANDLE, HANDLE);
		~FrameProcessing();

	public:
		int						process();

	private:
		void					applyMorphologyOperation(Mat*, Mat*, int, int, int);
};
