#include "FrameProcessing.h"
#include <sstream>
#include <atomic>

namespace Ptrack {

static atomic_bool shouldExit	= false;
static atomic_char msgFlag		= 0x0;

FrameProcessing::FrameProcessing()
:
thresh(75),
bufsize(20),
pause(true)
{
}

FrameProcessing::~FrameProcessing() {
}

int FrameProcessing::process() {

	CHAR	chBuf[20];
	DWORD	dwWritten;
	stringstream tempStr;
	HANDLE	hStdout = GetStdHandle(STD_OUTPUT_HANDLE);

	if (hStdout == INVALID_HANDLE_VALUE) {
		MessageBox(NULL, "Invalid out pipe handle", "Error", MB_OK);
		ExitProcess(1);
	}

	// startup messenger
	StartMessengerThread();

	while (!shouldExit) {
		if (!pause) {
			cap >> frame;
			cvtColor(frame, gframe, CV_BGR2GRAY);
			GaussianBlur(gframe, gframe, Size(5, 5), 2, 2);
			threshold(gframe, gframe, thresh, 255, THRESH_BINARY_INV);
			applyMorphologyOperation(&gframe, &gframe, 4, MORPH_OPEN);
			applyMorphologyOperation(&gframe, &gframe, 20, MORPH_CLOSE);
			findContours(gframe, contours, hierarchy, RETR_LIST, CHAIN_APPROX_NONE, Point(0, 0));

			int i = 0;
			if (contours.size() > 0) {
				size_t count = contours[i].size();
				if (count < 6)
					continue;

				Mat pointsf;
				Mat(contours[i]).convertTo(pointsf, CV_32F);
				RotatedRect box = fitEllipse(pointsf);

				if (MAX(box.size.width, box.size.height) > MIN(box.size.width, box.size.height) * 30)
					continue;

				ellipse(frame, box, Scalar(0, 0, 255), 1, CV_AA);
				ellipse(frame, box.center, box.size*0.5f, box.angle, 0, 360, Scalar(0, 255, 255), 1, CV_AA);
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
		switch (msgFlag) {
		case 'p':
			destroyWindow("Pupil Detection");
			cap.release();
			pause = true;
			msgFlag = 0;
			break;
		case 'r':
			while (!cap.isOpened() && !shouldExit) {
				cap.open(0);
			}
			namedWindow("Pupil Detection", CV_WINDOW_AUTOSIZE);
			createTrackbar("Threshold", "Pupil Detection", &thresh, 270);
			pause = false;
			msgFlag = 0;
		}
	}
	return 0;
}


void FrameProcessing::applyMorphologyOperation(Mat *src, Mat *dest, int kSize, int op) {

	Mat element = getStructuringElement(MORPH_RECT, Size(2 * kSize + 1, 2 * kSize + 1), Point(kSize, kSize));

	// Apply operation
	morphologyEx(*src, *dest, op, element);
}

// MESSENGER THREAD-----------------------------------------------------
DWORD WINAPI FrameProcessing::StartMessenger(LPVOID pParam) {
	FrameProcessing* fp = (FrameProcessing*)pParam;
	fp->CheckControlMessages();
	return 0;
}

void FrameProcessing::StartMessengerThread() {
	DWORD threadId;
	hThread = CreateThread(NULL, 0, StartMessenger, NULL, 0, &threadId);
}

void FrameProcessing::CheckControlMessages() {

	CHAR	readBuf[]{'\0','\0'};
	DWORD	dwRead{0};
	DWORD	bytesAvail{0};
	HANDLE	hStdin = GetStdHandle(STD_INPUT_HANDLE);

	if (hStdin == INVALID_HANDLE_VALUE) {
		MessageBox(NULL, "Invalid in pipe handle", "Error", MB_OK);
		ExitProcess(1);
	}

	// check for control messages from Eyetrack application
	// 'p' - pause, 'r' - resume, 'k' - kill process and exit
	while (!shouldExit) {
		PeekNamedPipe(hStdin, NULL, 0, NULL, &bytesAvail, NULL);
		if (bytesAvail) {
			BOOL bSuccess = ReadFile(hStdin, readBuf, 2, &dwRead, NULL);
			if (bSuccess && (dwRead > 0)) {
				switch (readBuf[0]) {
				case 'p':
					msgFlag = 0x70;
					break;
				case 'r':
					msgFlag = 0x72;
					break;
				case 'k':
					shouldExit = true;
					return;
				}
			}
		}
		Sleep(100);
	}
}

}