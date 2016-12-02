# Pupil Tracker

In behavioral neurophysiology the behavior of the animal is generally monitored by tracking eye movements. Eye positions indicate the area of a screen which the animal is directing it's attention. Commercial eyetracking systems can be very costly so I developed an in-house system. This application leans heavily on methods provided in the OpenCV library.

The source code is compact due to the power of the OpenCV image processing methods. There is a single class, FrameProcessing, which handles frame acquisition and image analysis. Eye position is determined by tracking the center of the pupil. The image is first converted to grey scale, then smoothed with a gaussian filter, thresholded at a user defined level, eroded and dilated, and run through a contour boundary detector. 

The pupil coordinates are communicated to an external application through an anonymous pipe. In our setup we have a separate application which runs the behavioral task on the screen and depends on a constant stream of eye coordinates which it receives from the pupil-tracker. This behavioral application is considered the parent process and it executes the pupil-tracker from within it's own process thread. Directory "parent_example" contains a section of example code which manages the parent communication for the pupiltrack application.

If compiling on VS 2008, three static libraries from OpenCV version 2.3.1 (9/12/2011) are required:
(http://opencv.org/downloads.html)

	* opencv_core231.lib
	* opencv_highgui231.lib
	* opencv_imgproc231.lib
	
If compiling on VS 2015, you can use a later version of opencv (I used 3.1.0) and statically link the world library:

	* opencv_world310.lib
	

Erik Schluter,
March 28, 2016