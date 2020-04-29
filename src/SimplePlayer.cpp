#include "SimplePlayer.h"

Player::Player()
{
	m_formatCtx = avformat_alloc_context();
	m_videoIndex = -1;
	m_audioIndex = -1;
	m_video_clock = 0;
	m_audio_clock = 0;
	m_frame_last_pts = 0;
	m_frame_last_delay = 0;
	m_frame_timer = av_gettime() / 1000000.0;
}

Player::~Player()
{
	avformat_free_context(m_formatCtx);
	av_frame_free(&m_videoFrame);
	av_frame_free(&m_audioFrame);
	sws_freeContext(m_swsContext);
	swr_free(&m_swrContext);
}

void Player::openVideo(string file)
{
	avformat_open_input(&m_formatCtx, file.c_str(), NULL, NULL);

	avformat_find_stream_info(m_formatCtx, NULL);

	for (int i = 0; i < m_formatCtx->nb_streams; i++)
	{
		if (m_formatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			m_vCodecCtx = m_formatCtx->streams[i]->codec;
			m_videoIndex = i;

			int w = m_vCodecCtx->width;
			int h = m_vCodecCtx->height;
			m_swsContext = sws_getContext(w, h, m_vCodecCtx->pix_fmt, w, h, AV_PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL);

			AVCodec* codec = avcodec_find_decoder(m_vCodecCtx->codec_id);   // 查找解码器

			//"没有该类型的解码器"
			if (codec == nullptr)
			{
				cout << "Can't find video decoder." << endl;
				return;
			}

			if (avcodec_open2(m_vCodecCtx, codec, NULL) != 0)
				cout << "Can't open video decoder" << endl;
		}
		else if (m_formatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			m_aCodecCtx = m_formatCtx->streams[i]->codec;
			m_audioIndex = i;

			int64_t in_channel_layout = av_get_default_channel_layout(m_aCodecCtx->channels);
			m_swrContext = swr_alloc_set_opts(NULL, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, 44100,
				in_channel_layout, m_aCodecCtx->sample_fmt, m_aCodecCtx->sample_rate, 0, NULL);
			swr_init(m_swrContext);

			AVCodec* codec = avcodec_find_decoder(m_aCodecCtx->codec_id);   // 查找解码器

			//"没有该类型的解码器"
			if (codec == nullptr)
			{
				cout << "Can't find audio decoder." << endl;
				return;
			}
			
			if (avcodec_open2(m_aCodecCtx, codec, NULL) != 0)
				cout << "Can't open audio decoder" << endl;
		}
	}
}

void Player::readFrame()
{
	m_packet = (AVPacket*)av_malloc(sizeof(AVPacket));

	if (m_formatCtx == nullptr)
	{
		cout << "File not open" << endl;
		return;
	}

	while (av_read_frame(m_formatCtx, m_packet) >= 0)
	{
		if (m_packet->stream_index == m_videoIndex)
			m_videoPacket.push(*m_packet);
		else if (m_packet->stream_index == m_audioIndex)
			m_audioPacket.push(*m_packet);
	}
	cout << "read frame finish. video: " << m_videoPacket.size() << " audio: " << m_audioPacket.size() << endl;
	av_packet_free(&m_packet);
	m_packet = nullptr;
}

int Player::getWidth()
{
	return m_vCodecCtx->width;
}

int Player::getHeight()
{
	return m_vCodecCtx->height;
}

double Player::getFrameRate() {
	if (m_formatCtx != nullptr) {
		AVRational frame_rate =
			av_guess_frame_rate(m_formatCtx, m_formatCtx->streams[m_videoIndex], NULL);
		double fr = frame_rate.num && frame_rate.den ? av_q2d(frame_rate) : 0.0;
		return fr;
	}
	else {
		throw std::runtime_error("can not getFrameRate.");
	}
}

int Player::getPixelFormat() {
	if (m_vCodecCtx != nullptr) {
		return static_cast<int>(m_vCodecCtx->pix_fmt);
	}
	else {
		throw std::runtime_error("can not getPixelFormat.");
	}
}

int Player::getVideoFrame(AVFrame*& frame)
{
	while (true)
	{
		if (m_videoFrame == nullptr)
			m_videoFrame = av_frame_alloc();

		if (m_videoPacket.size() == 0)
			return 0;

		AVPacket packet = m_videoPacket.front();
		m_videoPacket.pop();
		int ret = avcodec_send_packet(m_vCodecCtx, &packet);
		if (ret != 0)
			return 0;

		ret = avcodec_receive_frame(m_vCodecCtx, m_videoFrame);
		if (ret == 0)
			break;
	}

	double pts;
	if ((pts = av_frame_get_best_effort_timestamp(m_videoFrame)) == AV_NOPTS_VALUE)
		pts = 0;
	pts *= av_q2d(m_formatCtx->streams[m_videoIndex]->time_base);
	pts = synchronize(m_videoFrame, pts);
	m_videoFrame->opaque = &pts;

	int w = m_vCodecCtx->width;
	int h = m_vCodecCtx->height;

	int numBytes = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, w, h, 32);
	frame = av_frame_alloc();
	uint8_t* buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
	av_image_fill_arrays(frame->data, frame->linesize, buffer, AV_PIX_FMT_YUV420P, w, h, 32);
	sws_scale(m_swsContext, (uint8_t const* const*)m_videoFrame->data, m_videoFrame->linesize, 0, h, frame->data, frame->linesize);
	frame->opaque = m_videoFrame->opaque;

	av_frame_free(&m_videoFrame);
	m_videoFrame = nullptr;

	return 1;
}

int Player::getAudioBuffer(uint8_t*& buffer)
{
	while (true)
	{
		if (m_audioFrame == nullptr)
			m_audioFrame = av_frame_alloc();

		if (m_audioPacket.size() == 0)
			return 0;

		AVPacket packet = m_audioPacket.front();
		m_audioPacket.pop();

		if (packet.pts != AV_NOPTS_VALUE)
		{
			m_audio_clock = av_q2d(m_formatCtx->streams[m_audioIndex]->time_base) * packet.pts;
		}

		int ret = avcodec_send_packet(m_aCodecCtx, &packet);
		if (ret != 0)
			return 0;

		ret = avcodec_receive_frame(m_aCodecCtx, m_audioFrame);
		if (ret == 0)
			break;
	}

	buffer = (uint8_t*)av_malloc(MAX_AUDIO_FRAME_SIZE * 2);
	int len = swr_convert(m_swrContext, &buffer, MAX_AUDIO_FRAME_SIZE, (const uint8_t**)m_audioFrame->data, m_audioFrame->nb_samples);
	int data_size = len * av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO) * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
	m_audio_clock += static_cast<double>(data_size) / (2 * 2 * 44100);

	av_frame_free(&m_audioFrame);
	m_audioFrame = nullptr;

	return len;
}

AVCodecContext* Player::getAudioCodecCtx()
{
	return m_aCodecCtx;
}

double Player::synchronize(AVFrame* frame, double pts)
{
	double frame_delay;

	if (pts != 0)
		m_video_clock = pts; // Get pts,then set video clock to it
	else
		pts = m_video_clock; // Don't get pts,set it to video clock

	frame_delay = av_q2d(m_vCodecCtx->time_base);
	frame_delay += frame->repeat_pict * (frame_delay * 0.5);

	m_video_clock += frame_delay;

	return pts;
}

double Player::getVideoClock()
{
	return m_video_clock;
}

double Player::getAudioClock()
{
	return m_audio_clock;
}

void Player::setLastPts(double last_pts)
{
	m_frame_last_pts = last_pts;
}

double Player::getLastPts()
{
	return m_frame_last_pts;
}

void Player::setLastDelay(double last_delay)
{
	m_frame_last_delay = last_delay;
}

double Player::getLastDelay()
{
	return m_frame_last_delay;
}

void Player::addTimer(double delay)
{
	m_frame_timer += delay;
}

double Player::getTimer()
{
	return m_frame_timer;
}
