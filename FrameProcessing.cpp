#include "FrameProcessing.h"
#include <sstream>
#include <atomic>

namespace Ptrack {

static atomic_bool shouldExit = false;
static atomic_char msgFlag = 0x0;

FrameProcessing::FrameProcessing()
:
thresh(75),
bufsize(20),
#ifdef PTGREY
bInitialized(false),
#endif
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
#ifdef PTGREY
			cam.RetrieveBuffer(&rawImg);
			tempImg = ConvertImageToOpenCV(&rawImg);
			frame = cvarrToMat(tempImg);
			cvReleaseImageHeader(&tempImg);
			cvReleaseImage(&tempImg);
#else
			cap >> frame;
#endif
			cvtColor(frame, gframe, CV_BGR2GRAY);
			GaussianBlur(gframe, gframe, Size(5, 5), 2, 2);
			threshold(gframe, gframe, thresh, 255, THRESH_BINARY_INV);
			applyMorphologyOperation(&gframe, &gframe, 4, MORPH_OPEN);
			applyMorphologyOperation(&gframe, &gframe, 20, MORPH_CLOSE);
			findContours(gframe, contours, hierarchy, RETR_LIST, CHAIN_APPROX_NONE, Point(0, 0));

			if (contours.size() > 0) {
				size_t count = contours[0].size();
				if (count < 6)
					continue;

				Mat pointsf;
				Mat(contours[0]).convertTo(pointsf, CV_32F);
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
#ifdef PTGREY
			cam.StopCapture();
			cam.Disconnect();
#else
			cap.release();
#endif
			pause = true;
			msgFlag = 0;
			break;
		case 'r':
#ifdef PTGREY
			// retrieve first camera's id
			err = busMgr.GetCameraFromIndex(0, &guid);
			if (err != PGRERROR_OK) {
				MessageBox(NULL, (LPCSTR)err.GetDescription(), "Error", MB_OK);
				while ( (err != PGRERROR_OK) && !shouldExit)
					err = busMgr.GetCameraFromIndex(0, &guid);
			}
			// attempt camera connect
			err = cam.Connect(&guid);
			if (err != PGRERROR_OK) {
				MessageBox(NULL, (LPCSTR)err.GetDescription(), "Error", MB_OK);
				while ( (err != PGRERROR_OK) && !shouldExit)
					err = cam.Connect(&guid);
			}
			// set frame rate
			VideoMode vm;
			FrameRate fr;
			err = cam.GetVideoModeAndFrameRate(&vm, &fr);
			if (err != PGRERROR_OK) {
				err = cam.SetVideoModeAndFrameRate(vm, FRAMERATE_60);
				if (err != PGRERROR_OK) {
					MessageBox(NULL, (LPCSTR)err.GetDescription(), "Error", MB_OK);
				}
			}
			// start capture
			err = cam.StartCapture();
			if (err != PGRERROR_OK) {
				MessageBox(NULL, (LPCSTR)err.GetDescription(), "Error", MB_OK);
				while ((err != PGRERROR_OK) && !shouldExit)
					err = cam.StartCapture();
			}
#else
			while (!cap.isOpened() && !shouldExit) {
				cap.open(0);
			}
#endif
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
	hThread = CreateThread(NULL, 0, StartMessenger, this, 0, &threadId);
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

#ifdef PTGREY
// This method adapted from Point Grey Flycapture example program
IplImage* FrameProcessing::ConvertImageToOpenCV(Image* pImage)
{
	IplImage* cvImage = NULL;
	bool bColor = true;
	CvSize mySize;
	mySize.height = pImage->GetRows();
	mySize.width = pImage->GetCols();

	switch (pImage->GetPixelFormat())
	{
	case PIXEL_FORMAT_MONO8:	 cvImage = cvCreateImageHeader(mySize, 8, 1);
		cvImage->depth = IPL_DEPTH_8U;
		cvImage->nChannels = 1;
		bColor = false;
		break;
	case PIXEL_FORMAT_411YUV8:   cvImage = cvCreateImageHeader(mySize, 8, 3);
		cvImage->depth = IPL_DEPTH_8U;
		cvImage->nChannels = 3;
		break;
	case PIXEL_FORMAT_422YUV8:   cvImage = cvCreateImageHeader(mySize, 8, 3);
		cvImage->depth = IPL_DEPTH_8U;
		cvImage->nChannels = 3;
		break;
	case PIXEL_FORMAT_444YUV8:   cvImage = cvCreateImageHeader(mySize, 8, 3);
		cvImage->depth = IPL_DEPTH_8U;
		cvImage->nChannels = 3;
		break;
	case PIXEL_FORMAT_RGB8:      cvImage = cvCreateImageHeader(mySize, 8, 3);
		cvImage->depth = IPL_DEPTH_8U;
		cvImage->nChannels = 3;
		break;
	case PIXEL_FORMAT_MONO16:    cvImage = cvCreateImageHeader(mySize, 16, 1);
		cvImage->depth = IPL_DEPTH_16U;
		cvImage->nChannels = 1;
		bColor = false;
		break;
	case PIXEL_FORMAT_RGB16:     cvImage = cvCreateImageHeader(mySize, 16, 3);
		cvImage->depth = IPL_DEPTH_16U;
		cvImage->nChannels = 3;
		break;
	case PIXEL_FORMAT_S_MONO16:  cvImage = cvCreateImageHeader(mySize, 16, 1);
		cvImage->depth = IPL_DEPTH_16U;
		cvImage->nChannels = 1;
		bColor = false;
		break;
	case PIXEL_FORMAT_S_RGB16:   cvImage = cvCreateImageHeader(mySize, 16, 3);
		cvImage->depth = IPL_DEPTH_16U;
		cvImage->nChannels = 3;
		break;
	case PIXEL_FORMAT_RAW8:      cvImage = cvCreateImageHeader(mySize, 8, 3);
		cvImage->depth = IPL_DEPTH_8U;
		cvImage->nChannels = 3;
		break;
	case PIXEL_FORMAT_RAW16:     cvImage = cvCreateImageHeader(mySize, 8, 3);
		cvImage->depth = IPL_DEPTH_8U;
		cvImage->nChannels = 3;
		break;
	case PIXEL_FORMAT_MONO12:
		MessageBox(NULL, "Not supported in OpenCV", "Error", MB_OK);
		bColor = false;
		break;
	case PIXEL_FORMAT_RAW12:
		MessageBox(NULL, "Not supported in OpenCV", "Error", MB_OK);
		break;
	case PIXEL_FORMAT_BGR:       cvImage = cvCreateImageHeader(mySize, 8, 3);
		cvImage->depth = IPL_DEPTH_8U;
		cvImage->nChannels = 3;
		break;
	case PIXEL_FORMAT_BGRU:      cvImage = cvCreateImageHeader(mySize, 8, 4);
		cvImage->depth = IPL_DEPTH_8U;
		cvImage->nChannels = 4;
		break;
	case PIXEL_FORMAT_RGBU:      cvImage = cvCreateImageHeader(mySize, 8, 4);
		cvImage->depth = IPL_DEPTH_8U;
		cvImage->nChannels = 4;
		break;
	default:
		MessageBox(NULL, "Unknown pixel type", "Error", MB_OK);
		return NULL;
	}

	if (bColor) {
		if (!bInitialized) {
			colorImage.SetData(new unsigned char[pImage->GetCols() * pImage->GetRows() * 3], pImage->GetCols() * pImage->GetRows() * 3);
			bInitialized = true;
		}

		pImage->Convert(PIXEL_FORMAT_BGR, &colorImage); //needs to be as BGR to be saved

		cvImage->width = colorImage.GetCols();
		cvImage->height = colorImage.GetRows();
		cvImage->widthStep = colorImage.GetStride();

		cvImage->origin = 0; //interleaved color channels

		cvImage->imageDataOrigin = (char*)colorImage.GetData(); //DataOrigin and Data same pointer, no ROI
		cvImage->imageData = (char*)(colorImage.GetData());
		cvImage->widthStep = colorImage.GetStride();
		cvImage->nSize = sizeof(IplImage);
		cvImage->imageSize = cvImage->height * cvImage->widthStep;
	}
	else
	{
		cvImage->imageDataOrigin = (char*)(pImage->GetData());
		cvImage->imageData = (char*)(pImage->GetData());
		cvImage->widthStep = pImage->GetStride();
		cvImage->nSize = sizeof(IplImage);
		cvImage->imageSize = cvImage->height * cvImage->widthStep;
	}
	return cvImage;
}
#endif

}