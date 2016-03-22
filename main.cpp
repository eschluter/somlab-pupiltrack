/* Erik Schluter
 * 1/28/2016
 */
#include "FrameProcessing.h"

int main(int argc, char *argv[]) {
	HANDLE	hStdin, hStdout;

	// hide console window
	HWND hWnd = GetConsoleWindow();
	ShowWindow(hWnd, SW_HIDE);

	// open up data pipe handles
	hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	hStdin	= GetStdHandle(STD_INPUT_HANDLE);
	if ( (hStdout == INVALID_HANDLE_VALUE) || (hStdin == INVALID_HANDLE_VALUE) ) {
		MessageBox(NULL, "Invalid data pipe handles", "Error", MB_OK);
		ExitProcess(1);
	}

	FrameProcessing fp(hStdin, hStdout);
	fp.process();
	return 0;
}