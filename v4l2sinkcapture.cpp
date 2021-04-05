#include "v4l2sink.hpp"
#include "opencv2/opencv.hpp"
#include <stdio.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

//#include <omp.h>

void startCapture();
void copyBuffer(uint8_t *dstBuffer, uint32_t *size);
void stopCapture();
int saveFileBinary(const char* filename, uint8_t* data, int size);

int fd;
const int v4l2BufferNum = 1;
void *v4l2Buffer[v4l2BufferNum];
uint32_t v4l2BufferSize[v4l2BufferNum];

void startCapture()
{
	fd = open("/dev/video0", O_RDWR);

	/* 1. フォーマット指定。320 x 240のJPEG形式でキャプチャしてください */
	struct v4l2_format fmt;
	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = 320;
	fmt.fmt.pix.height = 240;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	ioctl(fd, VIDIOC_S_FMT, &fmt);

	/* 2. バッファリクエスト。バッファを2面確保してください */
	struct v4l2_requestbuffers req;
	memset(&req, 0, sizeof(req));
	req.count = v4l2BufferNum;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;
	ioctl(fd, VIDIOC_REQBUFS, &req);

	/* 3. 確保されたバッファをユーザプログラムからアクセスできるようにmmapする */
	struct v4l2_buffer buf;
	for (uint32_t i = 0; i < v4l2BufferNum; i++) {
		/* 3.1 確保したバッファ情報を教えてください */
		memset(&buf, 0, sizeof(buf));
		buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index  = i;
		ioctl(fd, VIDIOC_QUERYBUF, &buf);

		/* 3.2 取得したバッファ情報を基にmmapして、後でアクセスできるようにアドレスを保持っておく */
		v4l2Buffer[i] = mmap(NULL, buf.length, PROT_READ, MAP_SHARED, fd, buf.m.offset);
		v4l2BufferSize[i] = buf.length;
	}

	/* 4. バッファのエンキュー。指定するバッファをキャプチャするときに使ってください */
	for (uint32_t i = 0; i < v4l2BufferNum; i++) {
		memset(&buf, 0, sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;
		ioctl(fd, VIDIOC_QBUF, &buf);
	}

	/* 5. ストリーミング開始。キャプチャを開始してください */
	enum v4l2_buf_type type;
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ioctl(fd, VIDIOC_STREAMON, &type);

	/* この例だと2面しかないので、2フレームのキャプチャ(1/30*2秒?)が終わった後、新たにバッファがエンキューされるまでバッファへの書き込みは行われない */
}

void copyBuffer(uint8_t *dstBuffer, uint32_t *size)
{
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	/* 6. バッファに画データが書き込まれるまで待つ */
	while(select(fd + 1, &fds, NULL, NULL, NULL) < 0);

	if (FD_ISSET(fd, &fds)) {
		/* 7. バッファのデキュー。もっとも古くキャプチャされたバッファをデキューして、そのインデックス番号を教えてください */
		struct v4l2_buffer buf;
		memset(&buf, 0, sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		ioctl(fd, VIDIOC_DQBUF, &buf);

		/* 8. デキューされたバッファのインデックス(buf.index)と書き込まれたサイズ(buf.byteused)が返ってくる */
		*size = buf.bytesused;

		/* 9. ユーザプログラムで使いやすいように、別途バッファにコピーしておく */
		memcpy(dstBuffer, v4l2Buffer[buf.index], buf.bytesused);

		/* 10. 先ほどデキューしたバッファを、再度エンキューする。カメラデバイスはこのバッファに対して再びキャプチャした画を書き込む */
		ioctl(fd, VIDIOC_QBUF, &buf);
	}
}

void stopCapture()
{
	/* 11. ストリーミング終了。キャプチャを停止してください */
	enum v4l2_buf_type type;
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ioctl(fd, VIDIOC_STREAMOFF, &type);

	/* 12. リソース解放 */
	for (uint32_t i = 0; i < v4l2BufferNum; i++) munmap(v4l2Buffer[i], v4l2BufferSize[i]);

	/* 13. デバイスファイルを閉じる */
	close(fd);
}

int saveFileBinary(const char* filename, uint8_t* data, int size)
{
	FILE *fp;
	fp = fopen(filename, "wb");
	if(fp == NULL) {
		return -1;
	}
	fwrite(data, sizeof(uint8_t), size, fp);
	fclose(fp);
	return 0;
}



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

	cv::Mat img;
	int width = 640;
	int height = 480;
	cv::Mat yuv(width,height,CV_8SC3,cv::Scalar(0,0,0));

	cap >> img;
	char buffer[height][width*2];


	//int r = sink.init(img.cols,img.rows,img.channels() == 1 ? v4l2sink::GRAY: v4l2sink::RGB);
	int r = sink.init(img.cols,img.rows, v4l2sink::YUYV);
	printf("input %d %d %d\n",img.cols,img.rows,img.channels());

	//std::cout << "omp:" << omp_get_max_threads() << std::endl;

	while(true)
	{
		cap >> img;
		cv::cvtColor(img, yuv, cv::COLOR_RGB2YUV);

		for (int i=0;i<height;i++){

			cv::Vec3b *ptr = yuv.ptr<cv::Vec3b>(i);

			for (int j=0;j<width*2;j++){
				if (j % 2 == 0){
					buffer[i][j] = ptr[j/2][0];
				}
				else{
					if (j % 4 == 3)
						buffer[i][j] = ptr[j/2][1];
					if (j % 4 == 1)
						buffer[i][j] = ptr[j/2][2];
				}
			}

		}

		//std::cout << img << std::endl;
		/*
		for (int i=0;i<img.cols.;i++){
			for (int j=0;j<img.rows.;j++){

				float R = img[i][j][0];
				float G = img[i][j][1];
				float B = img[i][j][2];
					
				float Y = 0.299 * R + 0.587 * G + 0.114 * B;
				float U = 0.492 * (B - Y); 
				float V = 0.877 * (R - Y); 
				
				img[i][j] = [Y,U,V];	
			}
		}
		*/

		//printf("output %d %d %d\n",img.cols,img.rows,img.channels());
		if(img.rows != 0)
		{
	    		//r = sink.write((const char*)img.data,img.cols*img.rows*img.channels());
	    		r = sink.write((const char*)buffer,img.cols*img.rows*2);
		}
	}

	return 0;
}
