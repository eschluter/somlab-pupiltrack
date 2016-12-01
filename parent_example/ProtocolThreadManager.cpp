// Erik Schluter
// 3/28/2016

UINT PupiltrackThread(LPVOID pParam) {
	PupiltrackThreadManager *pupiltrackThreadManager = (PupiltrackThreadManager*)pParam;

	// continuously read from pipe
	pupiltrackThreadManager->runPupiltrack();

	return 0;
}

PupiltrackThreadManager::PupiltrackThreadManager(void)
:
cX(0),
cY(0),
isPaused(false)
{
	g_hChildStd_IN_Rd	= NULL;
	g_hChildStd_IN_Wr	= NULL;
	g_hChildStd_OUT_Rd	= NULL;
	g_hChildStd_OUT_Wr	= NULL;
	g_hInputFile		= NULL;

	logFilename						= (string)"C:\\somlab\\exe\\Eyetrack\\output\\EyeTrackPupiltrackThreadManagerLog_" + log.getDateTimeStr() + (string)".txt";

	// initialize data pipe
	InitializePipe();
	CreateChildProcess();
}

PupiltrackThreadManager::~PupiltrackThreadManager(void) {
}

CWinThread* PupiltrackThreadManager::startPupiltrack(const int &threadPriority, const string &threadName, const string &callingMethod, 
													EyeTrackView *pEyeTrackView, CWnd *pSecondaryDisplay) {
	// Setup event objects to wait on
	pupiltrackTimingHandles[0]	= PupiltrackKillEvent;
	pupiltrackTimingHandles[1]	= PupiltrackPauseEvent;
	pupiltrackTimingHandles[2]	= PupiltrackResumeEvent;

	// reset all timing signals
	PupiltrackInitEvent.ResetEvent();
	PupiltrackPauseEvent.ResetEvent();
	PupiltrackContinueEvent.ResetEvent();
	PupiltrackRestartEvent.ResetEvent();
	PupiltrackKillEvent.ResetEvent();

	return launchThread(PupiltrackThread, threadPriority, threadName, callingMethod, pEyeTrackView, pSecondaryDisplay, PupiltrackInitEvent);
}

CWinThread* PupiltrackThreadManager::stopPupiltrack(const string &callingMethod) {
	return killThread(callingMethod, PupiltrackKillEvent, true, 300);
}

void PupiltrackThreadManager::runPupiltrack() {

	DWORD	pupiltrackThreadReturn	= 0;
	bool	shouldExit				= false;

	// signal thread executing
	PupiltrackInitEvent.SetEvent();
	Sleep(200); //give the pupiltrack process a startup time buffer just in case
	WriteToPipe('r'); // pupiltrack processing thread is paused on startup - wake it
	executionState = Going;

	while (!shouldExit) {

		pupiltrackThreadReturn = (::WaitForMultipleObjects(3, pupiltrackTimingHandles, false, 1));
		
		switch (pupiltrackThreadReturn) {
			case WAIT_OBJECT_0:
				shouldExit = true;
				WriteToPipe('k');
				PupiltrackKillEvent.ResetEvent();
				break;
			case WAIT_OBJECT_0 + 1:
				isPaused = true;
				WriteToPipe('p');
				executionState = Stopped;
				PupiltrackPauseCompleteEvent.SetEvent();
				PupiltrackPauseEvent.ResetEvent();
				break;
			case WAIT_OBJECT_0 + 2:
				isPaused = false;
				WriteToPipe('r');
				executionState = Going;
				PupiltrackResumeCompleteEvent.SetEvent();
				PupiltrackResumeEvent.ResetEvent();
				break;
		}
		// read eye position from data pipe
		if (!isPaused) {
			ReadFromPipe();
		}
	}
}

// Pupil track data piping methods
/////////////////////////////////

void PupiltrackThreadManager::InitializePipe() {
	SECURITY_ATTRIBUTES saAttr;

	// Set the bInheritHandle flag so pipe handles are inherited.
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	// Create a pipe for the child process's STDOUT. 
	if ( ! CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0) ) 
		ErrorExit(TEXT("StdoutRd CreatePipe"));

	// Ensure the read handle to the pipe for STDOUT is not inherited.
	if ( ! SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0) )
		ErrorExit(TEXT("Stdout SetHandleInformation"));

	// Create a pipe for the child process's STDIN. 
	if (! CreatePipe(&g_hChildStd_IN_Rd, &g_hChildStd_IN_Wr, &saAttr, 0)) 
		ErrorExit(TEXT("Stdin CreatePipe"));

	// Ensure the write handle to the pipe for STDIN is not inherited. 
	if ( ! SetHandleInformation(g_hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0) )
		ErrorExit(TEXT("Stdin SetHandleInformation"));
}

void PupiltrackThreadManager::ErrorExit(PTSTR lpszFunction) {
// Format a readable error message, display a message box, 
// and exit from the application.
    LPVOID lpMsgBuf;
    LPVOID lpDisplayBuf;
    DWORD dw = GetLastError(); 

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );

    lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT, 
        (lstrlen((LPCTSTR)lpMsgBuf)+lstrlen((LPCTSTR)lpszFunction)+40)*sizeof(TCHAR)); 
    StringCchPrintf((LPTSTR)lpDisplayBuf, 
        LocalSize(lpDisplayBuf) / sizeof(TCHAR),
        TEXT("%s failed with error %d: %s"), 
        lpszFunction, dw, lpMsgBuf); 
    MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK); 

    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);
    ExitProcess(1);
}

void PupiltrackThreadManager::CreateChildProcess() {
	PROCESS_INFORMATION piProcInfo;
	STARTUPINFO siStartInfo;
	BOOL bSuccess = FALSE;
	// Set up members of the PROCESS_INFORMATION structure. 
	ZeroMemory( &piProcInfo, sizeof(PROCESS_INFORMATION) );

	// Set up members of the STARTUPINFO structure. 
	// This structure specifies the STDIN and STDOUT handles for redirection.
	ZeroMemory( &siStartInfo, sizeof(STARTUPINFO) );
	siStartInfo.cb = sizeof(STARTUPINFO); 
	siStartInfo.hStdError = g_hChildStd_OUT_Wr;
	siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
	siStartInfo.hStdInput = g_hChildStd_IN_Rd;
	siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

	// Create the child process. 
	bSuccess = CreateProcess("C:\\somlab\\exe\\Eyetrack\\PupilDetect.exe", 
	  NULL,     // command line 
	  NULL,          // process security attributes 
	  NULL,          // primary thread security attributes 
	  TRUE,          // handles are inherited 
	  0,             // creation flags 
	  NULL,          // use parent's environment 
	  NULL,          // use parent's current directory 
	  &siStartInfo,  // STARTUPINFO pointer 
	  &piProcInfo);  // receives PROCESS_INFORMATION 

	// If an error occurs, exit the application.
	if ( ! bSuccess )
		ErrorExit(TEXT("CreateProcess"));
	else {
		// Close handles to the child process and its primary thread. 
		CloseHandle(piProcInfo.hProcess);
		CloseHandle(piProcInfo.hThread);
	}
}

void PupiltrackThreadManager::ReadFromPipe() {
	// Read output from the child process's pipe for STDOUT
	DWORD bytesAvail = 0;
	DWORD dwRead;
	CHAR chBuf[20];
	BOOL bSuccess = FALSE;
	stringstream message;
	HANDLE hParentStdOut = GetStdHandle(STD_OUTPUT_HANDLE);

	PeekNamedPipe(g_hChildStd_OUT_Rd, NULL, 0, NULL, &bytesAvail, NULL);
	if (bytesAvail) {
		bSuccess = ReadFile(g_hChildStd_OUT_Rd, chBuf, 20, &dwRead, NULL);
		if (bSuccess && (dwRead > 0)) {
			char *nextT;
			cX = atof(strtok_s(chBuf, ",", &nextT));
			cY = atof(strtok_s(NULL, ",", &nextT));
			pDoc->setGazePoint((const float)cX, (const float)cY, (const float)pView->getViewWidth(), (const float)pView->getViewHeight());
		}
	}
}

void PupiltrackThreadManager::WriteToPipe(char toWrite) {
	DWORD dwWritten;
	CHAR chBuf[2];
	chBuf[0] = toWrite;
	// send into pipe
	WriteFile(g_hChildStd_IN_Wr, chBuf, 2, &dwWritten, NULL);
}
		

bool PupiltrackThreadManager::isThreadPaused() {
	return isPaused;
}
