#include "FrameProcessing.h"

using namespace Ptrack;

int main() {
	// hide console window
	HWND hWnd = GetConsoleWindow();
	ShowWindow(hWnd, SW_HIDE);

	// start processing
	FrameProcessing fp;
	fp.process();
	return 0;
}