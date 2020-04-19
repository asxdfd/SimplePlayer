#include <thread>
#include "SimplePlayer.h"
extern "C" {
#include "SDL.h"
};

#define REFRESH_EVENT (SDL_USEREVENT + 1)

#define BREAK_EVENT (SDL_USEREVENT + 2)

#define VIDEO_FINISH (SDL_USEREVENT + 3)

#undef main

void refreshPicture(int timeInterval, bool& finish) {
    cout << "timeInterval: " << timeInterval << endl;
    while (!finish) {
        SDL_Event event;
        event.type = REFRESH_EVENT;
        SDL_PushEvent(&event);
        this_thread::sleep_for(std::chrono::milliseconds(timeInterval));
    }
    // Break
    SDL_Event event;
    event.type = BREAK_EVENT;
    SDL_PushEvent(&event);
}

void playMediaVideo(const string& inputFile) {
    Player player;
    player.openVideo(inputFile);

    const int w = player.getWidth();
    const int h = player.getHeight();
    const auto fmt = AVPixelFormat(player.getPixelFormat());

    if (SDL_Init(SDL_INIT_VIDEO)) {
        string errMsg = "Could not init SDL";
        errMsg += SDL_GetError();
        cout << errMsg << endl;
        throw std::runtime_error(errMsg);
    }

    SDL_Window* screen;
    screen = SDL_CreateWindow("SimplePlayer", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    if (!screen) {
        string errMsg = "SDL: could not create window - exiting:";
        errMsg += SDL_GetError();
        cout << errMsg << endl;
        throw std::runtime_error(errMsg);
    }

    SDL_Renderer* sdlRenderer = SDL_CreateRenderer(screen, -1, 0);

    Uint32 pixFmt = SDL_PIXELFORMAT_IYUV;

    SDL_Texture* sdlTexture = SDL_CreateTexture(sdlRenderer, pixFmt, SDL_TEXTUREACCESS_STREAMING, w, h);

    try {
        int fps = player.getFrameRate();
        cout << "fps: " << fps << endl;

        int ret;
        bool videoFinish = false;
        AVFrame* frame = nullptr;

        thread refreshThread(refreshPicture, fps == 0 ? 100 : (int)(1000 / fps), ref(videoFinish));
        SDL_Event event;

        while (true) {
            if (!videoFinish) {
                ret = player.getFrame(frame);
                
                if (ret == 0) {  // no more frame.
                    cout << "VIDEO FINISHED." << endl;
                    videoFinish = true;
                    SDL_Event finishEvent;
                    finishEvent.type = VIDEO_FINISH;
                    SDL_PushEvent(&finishEvent);
                }
            }
            else {
                break;
            }
            SDL_WaitEvent(&event);
            if (frame != nullptr && event.type == REFRESH_EVENT) {
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
            }
            else if (event.type == SDL_QUIT) {
                videoFinish = true;
            }
            else if (event.type == BREAK_EVENT) {
                break;
            }

        }

        refreshThread.join();
        av_frame_free(&frame);
        frame = nullptr;

    }
    catch (std::exception ex) {
        cout << "Exception in play video" << ex.what() << endl;
    }
    catch (...) {
        cout << "Unknown exception in play media" << endl;
    }
}

int main()
{
    playMediaVideo("D:/视频/5-第1章 (两级独立型).mp4");
    //playMediaVideo("D:/1/46_异常概述_1.avi");

    return 0;
}