#pragma once

#include "opencv2/opencv.hpp"
#include <vector>
#include <windows.h>
#ifdef PTGREY
#include "FlyCapture2.h"
using namespace FlyCapture2;
#endif

using namespace std;
using namespace cv;

namespace Ptrack {

class FrameProcessing {

	private:
#ifdef PTGREY
		FlyCapture2::Error		err;
		FlyCapture2::BusManager	busMgr;
		FlyCapture2::PGRGuid	guid;
		FlyCapture2::Camera		cam;
		FlyCapture2::Image		rawImg;
		FlyCapture2::Image		colorImage;
		IplImage*				tempImg;
		bool bInitialized;
#else
		cv::VideoCapture cap;	// camera device
#endif
		Mat frame;			// raw image
		Mat gframe;			// grey scaled image copy
		vector<vector<Point>> contours;
		vector<Vec4i> hierarchy;
		int thresh;
		const unsigned int bufsize;
		
		HANDLE	hThread;
		bool	pause;

	public:
		FrameProcessing();
		~FrameProcessing();

	public:
		int		process();
		void	StartMessengerThread();

	private:
		void	applyMorphologyOperation(Mat*, Mat*, int, int);

		static DWORD WINAPI	StartMessenger(LPVOID pParam);
		void				CheckControlMessages();
#ifdef PTGREY
		IplImage* ConvertImageToOpenCV(Image* pImage);
#endif

};

}