#include "SimplePlayer.h"

Player::Player()
{
	m_videoCodecName = "unknown";
	m_formatCtx = avformat_alloc_context();
	m_videoIndex = -1;
}

Player::~Player()
{
	avformat_free_context(m_formatCtx);
	av_frame_free(&m_frame);
	sws_freeContext(m_swsContext);
}

void Player::openVideo(string file)
{
	avformat_open_input(&m_formatCtx, file.c_str(), NULL, NULL);

	avformat_find_stream_info(m_formatCtx, NULL);

	for (int i = 0; i < m_formatCtx->nb_streams; i++)
	{
		if (m_formatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			m_vCodecCtx = m_formatCtx->streams[i]->codec;
			m_videoIndex = i;

			int w = m_vCodecCtx->width;
			int h = m_vCodecCtx->height;
			m_swsContext = sws_getContext(w, h, m_vCodecCtx->pix_fmt, w, h, AV_PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL);

			AVCodec* codec = avcodec_find_decoder(m_vCodecCtx->codec_id);   // 查找解码器

			//"没有该类型的解码器"
			if (codec == nullptr)
			{
				cout << "Can't find decoder." << endl;
				return;
			}

			if (avcodec_open2(m_vCodecCtx, codec, NULL) != 0)
				cout << "Can't open decoder" << endl;
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

	if (av_read_frame(m_formatCtx, m_packet) != 0)
	{
		cout << "Can't read frame" << endl;
		return;
	}
}

int Player::decodeFrame()
{
	if (m_formatCtx == nullptr)
	{
		cout << "Media file is not opened yet" << endl;
		return 0;
	}
	if (m_packet->stream_index != m_videoIndex)
		return -1;

	if (m_frame == nullptr)
		m_frame = av_frame_alloc();

	int ret = avcodec_send_packet(m_vCodecCtx, m_packet);
	if (ret == 0)
	{
		av_packet_free(&m_packet);
		m_packet = nullptr;
	}
	else
		return 0;
	
	ret = avcodec_receive_frame(m_vCodecCtx, m_frame);
	if (ret == 0)
		return 1;
	else if (ret == AVERROR(EAGAIN))
		return -2;
	else
		return 0;
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

int Player::getFrame(AVFrame*& frame)
{
	int ret = -1;
	while (ret <= 0)
	{
		readFrame();
		ret = decodeFrame();
	}

	int w = m_vCodecCtx->width;
	int h = m_vCodecCtx->height;

	int numBytes = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, w, h, 32);
	frame = av_frame_alloc();
	uint8_t* buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
	av_image_fill_arrays(frame->data, frame->linesize, buffer, AV_PIX_FMT_YUV420P, w, h, 32);
	sws_scale(m_swsContext, (uint8_t const* const*)m_frame->data, m_frame->linesize, 0, h, frame->data, frame->linesize);

	return ret;
}
