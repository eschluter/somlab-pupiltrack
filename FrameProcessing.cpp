/* Erik Schluter
 * 1/28/2016
 */
#include "FrameProcessing.h"
#include <sstream>

FrameProcessing::FrameProcessing()
:
thresh(75),
bufsize(20)
{
}

FrameProcessing::FrameProcessing(HANDLE in, HANDLE out)
:
thresh(75),
bufsize(20)
{
	hStdin	= in;
	hStdout	= out;
}

FrameProcessing::~FrameProcessing() {
}

int FrameProcessing::process() {

	bool	pause		= false;
	int		msgboxID;
	CHAR	pStr[2]		= "p";
	CHAR	chBuf[20];
	CHAR	readBuf[2];
	DWORD	dwRead, dwWritten;
	DWORD	bytesAvail	= 0;
	BOOL	bSuccess;
	stringstream tempStr;
	
	// Initialize default camera
	VideoCapture	cap;
	cap.open(0);

	// attempt to connect to camera device (dev 0 is default)
	while(!cap.isOpened()) {
		if (!pause) {
			msgboxID = MessageBox(NULL, "No camera connected! Connect usb camera then press RETRY\n Or select CANCEL to use Arrington input.", 
										"video error", MB_RETRYCANCEL);
			switch (msgboxID) {
				case IDRETRY:
					// try to connect again
					cap.open(0);
					break;
				case IDCANCEL:
					// send pause signal to eyetrack which switches to arrington input
					pause = true;
					WriteFile(hStdout, pStr, bufsize, &dwWritten, NULL);
					break;
			}
		}
		// check for control messages from Eyetrack application
		PeekNamedPipe(hStdin, NULL, 0, NULL, &bytesAvail, NULL);
		if (bytesAvail) {
			bSuccess = ReadFile(hStdin, readBuf, 2, &dwRead, NULL);
			if (bSuccess && (dwRead > 0)) {
				switch (readBuf[0]) {
					case 'r':
						pause = false;
						break;
					case 'k':
						return 0;
				}
			}
		}
	}

	// Initialize camera window
	namedWindow("Pupil Detection", CV_WINDOW_AUTOSIZE);
	// Initialize trackbar for threshold value
	createTrackbar("Threshold", "Pupil Detection", &thresh, 270);

	while (true) {
		if (!pause) {
			cap >> frame;
			cvtColor(frame, frame2, CV_BGR2GRAY);
			GaussianBlur(frame2, frame2, Size(5,5), 2, 2);
			threshold(frame2, frame2, thresh, 255, THRESH_BINARY_INV);
			applyMorphologyOperation(&frame2, &frame2, 4, MORPH_ELLIPSE, MORPH_OPEN);
			applyMorphologyOperation(&frame2, &frame2, 20, MORPH_ELLIPSE, MORPH_CLOSE);
			f_frame = frame2.clone();
			findContours(f_frame, contours, hierarchy, RETR_LIST, CHAIN_APPROX_NONE, Point(0,0));

			int i = 0;
			if (contours.size() > 0) {
				size_t count = contours[i].size();
				if( count < 6 )
					continue;

				Mat pointsf;
				Mat(contours[i]).convertTo(pointsf, CV_32F);
				RotatedRect box = fitEllipse(pointsf);

				if( MAX(box.size.width, box.size.height) > MIN(box.size.width, box.size.height)*30 )
					continue;

				ellipse(frame, box, Scalar(0,0,255), 1, CV_AA);
				ellipse(frame, box.center, box.size*0.5f, box.angle, 0, 360, Scalar(0,255,255), 1, CV_AA);
				// package data to send
				tempStr.str(std::string());
				tempStr << box.center.x << "," << box.center.y;
				strcpy(chBuf, tempStr.str().c_str());
				// Send data into pipe (write to stdout)
				WriteFile(hStdout, chBuf, bufsize, &dwWritten, NULL);
			}
			imshow("Pupil Detection", frame);
			waitKey(1);
		}
		// check for control messages from Eyetrack application
		// 'p' - pause, 'r' - resume, 'k' - kill process and exit
		PeekNamedPipe(hStdin, NULL, 0, NULL, &bytesAvail, NULL);
		if (bytesAvail) {
			bSuccess = ReadFile(hStdin, readBuf, 2, &dwRead, NULL);
			if (bSuccess && (dwRead > 0)) {
				switch (readBuf[0]) {
					case 'p':
						destroyWindow("Pupil Detection");
						pause = true;
						break;
					case 'r':
						namedWindow("Pupil Detection", CV_WINDOW_AUTOSIZE);
						createTrackbar("Threshold", "Pupil Detection", &thresh, 270);
						pause = false;
						break;
					case 'k':
						return 0;
				}
			}
		}
	}
	return 0;
}


void FrameProcessing::applyMorphologyOperation(Mat *src, Mat *dest, int kSize, int kType, int op) {

	Mat element = getStructuringElement(MORPH_RECT, Size(2*kSize + 1, 2*kSize + 1), Point(kSize, kSize));

	// Apply operation
	morphologyEx(*src, *dest, op, element);
}