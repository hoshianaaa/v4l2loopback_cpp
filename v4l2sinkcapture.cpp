#include "v4l2sink.hpp"
#include "opencv2/opencv.hpp"
#include <stdio.h>

int main(int argc, char**argv)
{
	if(argc < 3) 
	{
		printf("test needs: output_device input_device\n");
		return -1;
	}

	v4l2sink sink;

	if(!sink.open(argv[1]))
	{
		printf("Failed opening\n");
		return -1;
	}

	int id = atoi(argv[2]);
	cv::VideoCapture cap(id);
	cap.set(8, ('M', 'J', 'P', 'G'));
	int frameFormat = cap.get(CV_CAP_PROP_FORMAT);
	printf("format %d\n",frameFormat);


	if(!cap.isOpened())
	{
		printf("not opened %d\n",id);
		return -1;
	}
	else
		printf("opened %d\n",id);

	while(true)
	{
		cv::Mat img;
		cap >> img;
		//printf("loaded %d %d %d\n",img.cols,img.rows,img.channels());
		if(img.rows != 0)
		{
	    int r = sink.init(img.cols,img.rows,img.channels() == 1 ? v4l2sink::GRAY: v4l2sink::RGB);
	    r = sink.write((const char*)img.data,img.cols*img.rows*img.channels());
		}
	}

	return 0;
}
