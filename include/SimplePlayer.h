// SimplePlayer.h: 标准系统包含文件的包含文件
// 或项目特定的包含文件。

#ifndef SIMPLE_PLAYER_H
#define SIMPLE_PLAYER_H

// TODO: 在此处引用程序需要的其他标头。
#include <iostream>
#include <string>
#include <queue>
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libswresample/swresample.h"
#include "libavutil/time.h"
};

#define MAX_AUDIO_FRAME_SIZE 192000
#define SYNC_THRESHOLD 0.01
#define NOSYNC_THRESHOLD 10.0

using namespace std;

class Player
{
public:
	Player();
	~Player();
	void openVideo(string);
	void readFrame();
	int getWidth();
	int getHeight();
	double getFrameRate();
	int getPixelFormat();
	int getVideoFrame(AVFrame*&);
	int getAudioBuffer(uint8_t*&);
	AVCodecContext* getAudioCodecCtx();
	double getVideoClock();
	double getAudioClock();
	void setLastPts(double);
	double getLastPts();
	void setLastDelay(double);
	double getLastDelay();
	void addTimer(double);
	double getTimer();

private:
	AVFormatContext* m_formatCtx = nullptr;
	AVCodecContext* m_vCodecCtx = nullptr;
	AVCodecContext* m_aCodecCtx = nullptr;
	SwsContext* m_swsContext = nullptr;
	SwrContext* m_swrContext = nullptr;
	AVPacket* m_packet = nullptr;
	AVFrame* m_videoFrame = nullptr;
	AVFrame* m_audioFrame = nullptr;
	int m_videoIndex;
	int m_audioIndex;
	queue<AVPacket> m_videoPacket;
	queue<AVPacket> m_audioPacket;
	double m_video_clock;
	double m_audio_clock;
	double m_frame_last_pts;
	double m_frame_last_delay;
	double m_frame_timer;
	double synchronize(AVFrame*, double);
};


#endif
