#pragma once

#include "opencv2/opencv.hpp"
#include <vector>
#include <windows.h>

using namespace std;
using namespace cv;

namespace Ptrack {

class FrameProcessing {

	private:
		VideoCapture			cap;	// camera device
		Mat						frame;  // raw image
		Mat						gframe; // grey scaled image copy
		vector<vector<Point> >	contours;
		vector<Vec4i>			hierarchy;
		int						thresh;
		const unsigned int		bufsize;
		
		HANDLE					hThread;
		bool					pause;

	public:
		FrameProcessing();
		~FrameProcessing();

	public:
		int						process();
		void					StartMessengerThread();

	private:
		void					applyMorphologyOperation(Mat*, Mat*, int, int);

		static DWORD WINAPI		StartMessenger(LPVOID pParam);
		void					CheckControlMessages();

};

}