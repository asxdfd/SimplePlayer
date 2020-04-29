#include <thread>
#include "SimplePlayer.h"
extern "C"
{
#include "SDL.h"
};

#define REFRESH_EVENT (SDL_USEREVENT + 1)

#define BREAK_EVENT (SDL_USEREVENT + 2)

#define VIDEO_FINISH (SDL_USEREVENT + 3)

#undef main

void refreshPicture(int& timeInterval, bool& finish)
{
	while (!finish) {
		SDL_FlushEvent(REFRESH_EVENT);
		SDL_Event event;
		event.type = REFRESH_EVENT;
		SDL_PushEvent(&event);
		//this_thread::sleep_for(chrono::microseconds(timeInterval));
	}
	// Break
	SDL_Event event;
	event.type = BREAK_EVENT;
	SDL_PushEvent(&event);
}

int audio_len;
Uint8* audio_pos;
void  audioCallback(void* userdata, Uint8* stream, int len)
{
	SDL_memset(stream, 0, len);
	while (len > 0)
	{
		if (audio_len == 0)
			continue;
		int temp = (len > audio_len ? audio_len : len);
		SDL_MixAudio(stream, audio_pos, temp, SDL_MIX_MAXVOLUME);
		audio_pos += temp;
		audio_len -= temp;
		stream += temp;
		len -= temp;
	}
}

void playMediaVideo(Player& player)
{
	const int w = player.getWidth();
	const int h = player.getHeight();
	const auto fmt = AVPixelFormat(player.getPixelFormat());

	SDL_Window* screen;
	screen = SDL_CreateWindow("SimplePlayer", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

	if (!screen)
	{
		string errMsg = "SDL: could not create window - exiting:";
		errMsg += SDL_GetError();
		cout << errMsg << endl;
		throw std::runtime_error(errMsg);
	}

	SDL_Renderer* sdlRenderer = SDL_CreateRenderer(screen, -1, 0);

	Uint32 pixFmt = SDL_PIXELFORMAT_IYUV;

	SDL_Texture* sdlTexture = SDL_CreateTexture(sdlRenderer, pixFmt, SDL_TEXTUREACCESS_STREAMING, w, h);

	try
	{
		int fps = player.getFrameRate();
		cout << "fps: " << fps << endl;

		int ret;
		int timeInterval = 0;
		bool videoFinish = false;
		AVFrame* frame = nullptr;

		thread refreshThread(refreshPicture, ref(timeInterval), ref(videoFinish));
		SDL_Event event;

		while (true)
		{
			SDL_WaitEvent(&event);
			if (event.type == REFRESH_EVENT)
			{
				if (!videoFinish)
				{
					ret = player.getVideoFrame(frame);

					if (ret == 0)
					{  // no more frame.
						cout << "VIDEO FINISHED." << endl;
						videoFinish = true;
						SDL_Event finishEvent;
						finishEvent.type = VIDEO_FINISH;
						SDL_PushEvent(&finishEvent);
					}
				}
				else
					break;

				// 将视频同步到音频上，计算下一帧的延迟时间
				// 使用要播放的当前帧的PTS和上一帧的PTS差来估计播放下一帧的延迟时间，并根据video的播放速度来调整这个延迟时间
				double current_pts = *(double*)frame->opaque;
				double delay = current_pts - player.getLastPts();
				if (delay <= 0 || delay >= 1.0)
					delay = player.getLastDelay();

				player.setLastDelay(delay);
				player.setLastPts(current_pts);

				// 根据Audio clock来判断Video播放的快慢
				double video_clock = player.getVideoClock();
				double audio_clock = player.getAudioClock();

				double diff = video_clock - audio_clock;// diff < 0 => video slow,diff > 0 => video quick

				double threshold = (delay > SYNC_THRESHOLD) ? SYNC_THRESHOLD : delay;

				// 调整播放下一帧的延迟时间，以实现同步
				if (fabs(diff) < NOSYNC_THRESHOLD) // 不同步
				{
					if (diff <= -threshold) // 慢了，delay设为0
						delay = 0;
					else if (diff >= threshold) // 快了，加倍delay
						delay *= 2;
				}
				player.addTimer(delay);
				double actual_delay = player.getTimer() - static_cast<double>(av_gettime()) / 1000000.0;
				if (actual_delay <= 0.010)
					actual_delay = 0.010;

				// 设置一下帧播放的延迟
				timeInterval = static_cast<int>(actual_delay * 1000000.0);
				this_thread::sleep_for(chrono::microseconds(timeInterval));
				if (frame != nullptr)
				{
					SDL_UpdateYUVTexture(sdlTexture,
						NULL,
						frame->data[0],
						frame->linesize[0],
						frame->data[1],
						frame->linesize[1],
						frame->data[2],
						frame->linesize[2]);
					SDL_RenderClear(sdlRenderer);
					SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
					SDL_RenderPresent(sdlRenderer);
					
					av_frame_free(&frame);
					frame = nullptr;
				}
			}
			else if (event.type == SDL_QUIT)
				videoFinish = true;
			else if (event.type == BREAK_EVENT)
				break;
		}

		refreshThread.join();

	}
	catch (std::exception ex)
	{
		cout << "Exception in play video" << ex.what() << endl;
	}
	catch (...)
	{
		cout << "Unknown exception in play media" << endl;
	}
}

void playMediaAudio(Player& player)
{
	AVCodecContext* audioCodecCtx = player.getAudioCodecCtx();

	SDL_AudioSpec wanted_spec;
	wanted_spec.freq = 44100;
	wanted_spec.format = AUDIO_S16SYS;
	wanted_spec.channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
	wanted_spec.silence = 0;
	wanted_spec.samples = audioCodecCtx->frame_size;
	wanted_spec.callback = audioCallback;
	wanted_spec.userdata = audioCodecCtx;

	if (SDL_OpenAudio(&wanted_spec, NULL) < 0)
	{
		cout << "can't open audio." << endl;
		return;
	}

	SDL_PauseAudio(0);

	int len;
	uint8_t* buffer = nullptr;
	int out_size = 0;
	bool audioFinish = false;

	while (true)
	{
		if (!audioFinish)
		{
			if (buffer != nullptr)
			{
				av_free(buffer);
				buffer = nullptr;
			}
			len = player.getAudioBuffer(buffer);

			if (len == 0)
			{  // no more frame.
				cout << "AUDIO FINISHED." << endl;
				audioFinish = true;
			}
			else
				out_size = len * av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO) * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
		}
		else
			break;

		if (buffer != nullptr && out_size != 0)
		{
			while (audio_len > 0)
				SDL_Delay(1);
			audio_len = out_size;
			audio_pos = buffer;
		}
	}
}

void playMedia(const string& inputFile)
{
	Player player;
	player.openVideo(inputFile);
	player.readFrame();

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
	{
		string errMsg = "Could not init SDL";
		errMsg += SDL_GetError();
		cout << errMsg << endl;
		throw std::runtime_error(errMsg);
	}

	thread audio(playMediaAudio, ref(player));
	thread video(playMediaVideo, ref(player));

	audio.join();
	video.join();
}

int main()
{
	//playMediaVideo("D:/视频/5-第1章 (两级独立型).mp4");
	//playMediaVideo("D:/1/46_异常概述_1.avi");
	//playMediaAudio("D:/视频/5-第1章 (两级独立型).mp4");
	playMedia("D:/123.mp4");

	return 0;
}