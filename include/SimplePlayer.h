// SimplePlayer.h: 标准系统包含文件的包含文件
// 或项目特定的包含文件。

#ifndef SIMPLE_PLAYER_H
#define SIMPLE_PLAYER_H

// TODO: 在此处引用程序需要的其他标头。
#include <iostream>
#include <string>
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libswresample/swresample.h"
};

using namespace std;

class Player
{
public:
	Player();
	~Player();
	void openVideo(string);
    void readFrame();
    int decodeFrame();
    bool yuv2RGB(char*);
    int getWidth();
    int getHeight();
    double getFrameRate();
    int getPixelFormat();
    int getFrame(AVFrame*&);

private:
    string m_videoCodecName;
    AVFormatContext* m_formatCtx = nullptr;
    AVCodecContext* m_vCodecCtx = nullptr;
    SwsContext* m_swsContext = nullptr;
    AVPacket* m_packet = nullptr;
    AVFrame* m_frame = nullptr;
    int m_videoIndex;
    bool isEnd = false;
};


#endif
