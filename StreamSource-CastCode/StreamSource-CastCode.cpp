// StreamSource-CastCode.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include <iostream>
#include <memory>
#include <tchar.h>

AVFormatContext *inputContext;
AVFormatContext *outputContext;
int64_t lastReadPacketTime;

void Init() {
	cout << "初始化" << endl;
	av_register_all();
	avcodec_register_all();
	avfilter_register_all();
	avformat_network_init();
	av_log_set_level(AV_LOG_ERROR);
}

//open_input回调函数
static int interrupt_cb(void *ctx) {
	int timeout = 10;
	if (av_gettime() - lastReadPacketTime > timeout * 1000 * 1000) {
		//返回负值，avformat_open_input就会返回
		return -1;
	}
	return 0;
}

int OpenInput(string inputUrl) {
	cout << "开始打开输入流" << endl;
	inputContext = avformat_alloc_context();
	lastReadPacketTime = av_gettime();
	inputContext->interrupt_callback.callback = interrupt_cb;
	int ret = avformat_open_input(&inputContext, inputUrl.c_str(), nullptr, nullptr);
	if (ret < 0) {
		char *errStr = new char[1024];
		av_strerror(ret, errStr, 1024);
		av_log(NULL, AV_LOG_ERROR, "open input file failed!err msg:%s\n", errStr);
		return ret;
	}
	else {
		av_log(NULL, AV_LOG_FATAL, "open input file success!\n");
	}
	ret = avformat_find_stream_info(inputContext, nullptr);
	if (ret < 0) {
		char *errStr = new char[1024];
		av_strerror(ret, errStr, 1024);
		av_log(NULL, AV_LOG_ERROR, "find input stream failed!,err msg:%s\n", errStr);
		return ret;
	}
	else {
		av_log(NULL, AV_LOG_FATAL, "find input stream success!\n");
	}
	return ret;
}

int OpenOutput(string outputUrl) {
	cout << "开始打开输出流" << endl;
	int ret = avformat_alloc_output_context2(&outputContext, nullptr, "flv", outputUrl.c_str());
	if (ret < 0) {
		char *errStr = new char[1024];
		av_strerror(ret, errStr, 1024);
		av_log(NULL, AV_LOG_ERROR, "open output failed!err msg:%s\n", errStr);
		goto Error;
	}
	else {
		av_log(NULL, AV_LOG_FATAL, "open output success!\n");
	}
	ret = avio_open2(&outputContext->pb, outputUrl.c_str(), AVIO_FLAG_WRITE, nullptr, nullptr);
	if (ret < 0) {
		char *errStr = new char[1024];
		av_strerror(ret, errStr, 1024);
		av_log(NULL, AV_LOG_ERROR, "avio open failed!err msg:%s\n", errStr);
		goto Error;
	}
	for (int i = 0; i < inputContext->nb_streams; i++) {
		AVStream *stream = avformat_new_stream(outputContext, inputContext->streams[i]->codec->codec);
		ret = avcodec_copy_context(stream->codec, inputContext->streams[i]->codec);
		if (ret < 0) {
			char *errStr = new char[1024];
			av_strerror(ret, errStr, 1024);
			av_log(NULL, AV_LOG_ERROR, "copy codec context failed!err msg:%s\n", errStr);
			goto Error;
		}
	}
	ret = avformat_write_header(outputContext, nullptr);
	if (ret < 0) {
		char *errStr = new char[1024];
		av_strerror(ret, errStr, 1024);
		av_log(NULL, AV_LOG_ERROR, "write header failed!%s\n", errStr);
		goto Error;
	}
	av_log(NULL, AV_LOG_FATAL, "open output file success!\n");
	return ret;
Error:
	if (outputContext) {
		for (int i = 0; i < outputContext->nb_streams; i++) {
			avcodec_close(outputContext->streams[i]->codec);
		}
		avformat_close_input(&outputContext);
	}
	return ret;
}

shared_ptr<AVPacket> ReadPacketFromSource() {
	//shared_ptr的用法，构造AVPacket
	shared_ptr<AVPacket> packet(static_cast<AVPacket*>(av_malloc(sizeof(AVPacket))),
		[&](AVPacket *p) {av_packet_free(&p); av_freep(&p); });
	lastReadPacketTime = av_gettime();//重新初始化时间戳
	av_init_packet(packet.get());
	int ret = av_read_frame(inputContext, packet.get());
	if (ret >= 0) {
		return packet;
	}
	else {
		return nullptr;
	}
}

int WritePacket(shared_ptr<AVPacket> packet) {
	auto inputStream = inputContext->streams[packet->stream_index];
	auto outputStream = outputContext->streams[packet->stream_index];
	av_packet_rescale_ts(packet.get(), inputStream->time_base, outputStream->time_base);
	return av_interleaved_write_frame(outputContext, packet.get());
}


void CloseInput() {
	if (inputContext != nullptr) {
		avformat_close_input(&inputContext);
	}
}

void CloseOutput() {
	if (outputContext != nullptr) {
		for (int i = 0; i < outputContext->nb_streams; i++) {
			AVCodecContext * codecContext = outputContext->streams[i]->codec;
			avcodec_close(codecContext);
		}
		avformat_close_input(&outputContext);
	}
}


int main()
{
	Init();
	int ret = OpenInput("rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov");	//打开rtsp流
	if (ret >= 0) {
		//输出为rtmp流
		OpenOutput("rtmp://127.0.0.1:1935/live/stream0");		//打开输出流
	}
	else {
		cout << "open input failed! ret=" << ret << endl;
		goto Error;
	}
	if (ret < 0) {
		goto Error;
	}
	while (true) {
		auto packet = ReadPacketFromSource();
		if (packet) {
			ret = WritePacket(packet);
			if (ret >= 0) {
				cout << "WritePacket successs!" << endl;
			}
			else {
				cout << "Write Packet failed!" << endl;
			}
		}
		else {
			break;
		}
	}
Error:
	CloseInput();
	CloseOutput();
	while (true) {
		this_thread::sleep_for(chrono::seconds(100));
	}
	return 0;
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门提示: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
