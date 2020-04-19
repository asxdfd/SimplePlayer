# CMakeList.txt: SimplePlayer 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required (VERSION 3.8)

#指定要引用的dll的头文件所在路径
include_directories("E:/ffmpeg/ffmpeg-4.2.2-win64-dev/include")
include_directories("E:/SDL2-2.0.12/include")
include_directories("E:/OpenAL 1.1 SDK/include")
include_directories("./include")
#指定该dll的lib所在路径
link_directories("E:/ffmpeg/ffmpeg-4.2.2-win64-dev/lib")
link_directories("E:/OpenAL 1.1 SDK/libs/Win64")
link_directories("E:/SDL2-2.0.12/lib/x64")

# 将源代码添加到此项目的可执行文件。
add_executable (SimplePlayer  "src/SimplePlayer.cpp" "src/main.cpp")

# TODO: 如有需要，请添加测试并安装目标。

#指定链接库的名字
target_link_libraries(SimplePlayer
	avcodec		#音视频编码核心库 , 用于音视频或图片编解码
	avdevice	#硬件采集 , 加速 , 显示 库
	avfilter	#音视频滤波器
	avformat	#音视频封装格式生成或解析 
	avutil		#该库中封装了一些公共的工具函数
	postproc	#封装了同步, 时间计算的简单算法
	swresample	#音视频编解码格式预设
	swscale		#原始视频格式转换库
	SDL2
	OpenAL32
)

