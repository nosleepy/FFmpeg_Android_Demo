#include <jni.h>
#include <string>
#include <android/log.h>

extern "C"
{
//编码
#include "libavcodec/avcodec.h"
//封装格式处理
#include "libavformat/avformat.h"
//像素处理
#include "libswscale/swscale.h"
//工具类
#include "libavutil/imgutils.h"
//音频处理
#include "libswresample/swresample.h"
//android
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <unistd.h>
}

#define LOGE(FORMAT, ...) __android_log_print(ANDROID_LOG_ERROR, "wlzhou", FORMAT, ##__VA_ARGS__);

extern "C"
JNIEXPORT jstring JNICALL
Java_com_grandstream_ffmpeg_MyUtils_getAvcodecConfiguration(JNIEnv *env, jobject thiz) {
    std::string config = avcodec_configuration();
    return env->NewStringUTF(config.c_str());
}
extern "C"
JNIEXPORT void JNICALL
Java_com_grandstream_ffmpeg_MyUtils_playVideoOld(JNIEnv *env, jclass clazz, jstring inputStr, jobject surface) {
    const char *inputPath = env->GetStringUTFChars(inputStr, JNI_FALSE);
    LOGE("inputPath = %s", inputPath)
    //注册各大组件
    av_register_all();
    AVFormatContext *avFormatContext = avformat_alloc_context(); //获取上下文
    int error;
    char buf[] = "";
    //打开视频地址并获取里面的内容(解封装)
    error = avformat_open_input(&avFormatContext, inputPath, NULL, NULL);
    if (error < 0) {
        av_strerror(error, buf, 1024);
        LOGE("can't open file %s: %d(%s)", inputPath, error, buf)
        LOGE("打开视频失败")
        return;
    }
    if (avformat_find_stream_info(avFormatContext, NULL) < 0) {
        LOGE("获取内容失败")
        return;
    }
    //获取到整个内容过后找到里面的视频流
    int videoIndex = -1;
    for (int i = 0; i < avFormatContext->nb_streams; i++) {
        if (avFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            //如果是视频流,标记
            videoIndex = i;
        }
    }
    LOGE("成功找到视频流")
    //对视频流进行解码
    //获取解码器上下文
    AVCodecContext *avCodecContext = avFormatContext->streams[videoIndex]->codec;
    //获取解码器
    AVCodec *avCodec = avcodec_find_decoder(avCodecContext->codec_id);
    //打开解码器
    if (avcodec_open2(avCodecContext, avCodec, NULL) < 0) {
        LOGE("打开失败")
        return;
    }
    //申请AVPacket
    AVPacket *packet = (AVPacket*) av_malloc(sizeof(AVPacket));
    av_init_packet(packet);
    //申请AVFrame
    AVFrame *frame = av_frame_alloc(); //分配一个AVFrame结构体,AVFrame结构体一般用于存储原始数据,指向解码后的原始帧
    AVFrame *rgbFrame = av_frame_alloc(); //分配一个AVFrame结构体,指向存放转换成rgb后的帧
    //缓存区
    uint8_t *outBuffer = (uint8_t*) av_malloc(avpicture_get_size(
            AV_PIX_FMT_RGBA, avCodecContext->width, avCodecContext->height));
    //与缓存区相关联,设置rgbFrame缓存区
    avpicture_fill((AVPicture*) rgbFrame, outBuffer,
                   AV_PIX_FMT_RGBA, avCodecContext->width, avCodecContext->height);
    SwsContext *swsContext = sws_getContext(avCodecContext->width, avCodecContext->height, avCodecContext->pix_fmt,
                                            avCodecContext->width, avCodecContext->height, AV_PIX_FMT_RGBA,
                                            SWS_BICUBIC, NULL, NULL, NULL);
    //取到NativeWindow
    ANativeWindow *nativeWindow = ANativeWindow_fromSurface(env, surface);
    if (nativeWindow == 0) {
        LOGE("NativeWindow取到失败")
        return;
    }
    //视频缓冲区
    ANativeWindow_Buffer nativeOutBuffer;
    int frameCount;
    while (av_read_frame(avFormatContext, packet) >= 0) {
        if (packet->stream_index == videoIndex) {
            //如果是视频流解码
            avcodec_decode_video2(avCodecContext, frame, &frameCount, packet);
            LOGE("解码中...%d", frameCount)
            if (frameCount) {
                //说明有内容,绘制之前配置NativeWindow
                ANativeWindow_setBuffersGeometry(nativeWindow, avCodecContext->width, avCodecContext->height, WINDOW_FORMAT_RGBA_8888);
                //上锁
                ANativeWindow_lock(nativeWindow, &nativeOutBuffer, NULL);
                //转换为rgb格式
                sws_scale(swsContext, (const uint8_t *const *) frame->data, frame->linesize, 0, frame->height, rgbFrame->data, rgbFrame->linesize);
                //rgbFrame是有画面数据
                uint8_t *dst = (uint8_t *) nativeOutBuffer.bits;
                //拿到一行有多少个字节RGBA
                int destStride = nativeOutBuffer.stride * 4;
                //像素数据的首地址
                uint8_t *src = rgbFrame->data[0];
                //实际内存一行数量
                int srcStride = rgbFrame->linesize[0];
                //int i = 0;
                for (int i = 0; i < avCodecContext->height; i++) {
                    //将rgb_frame中每一行的数据复制给NativeWindow
                    memcpy(dst + i * destStride, src + i * srcStride, srcStride);
                }
                //解锁
                ANativeWindow_unlockAndPost(nativeWindow);
                usleep(1000 * 16);
            }
        }
        av_free_packet(packet);
    }
    //释放
    ANativeWindow_release(nativeWindow);
    av_frame_free(&frame);
    av_frame_free(&rgbFrame);
    avcodec_close(avCodecContext);
    avformat_free_context(avFormatContext);
    env->ReleaseStringUTFChars(inputStr, inputPath);
}
extern "C"
JNIEXPORT void JNICALL
Java_com_grandstream_ffmpeg_MyUtils_playVideoNew(JNIEnv *env, jclass clazz, jstring input_str,jobject surface) {
    const char *inputPath = env->GetStringUTFChars(input_str, JNI_FALSE);
    AVFormatContext *avFormatContext = avformat_alloc_context();//获取上下文
    int error;
    char buf[] = "";
    //打开视频地址并获取里面的内容(解封装)
    error = avformat_open_input(&avFormatContext, inputPath, NULL, NULL);
    if (error < 0) {
        av_strerror(error, buf, 1024);
        LOGE("Couldn't open file %s: %d(%s)", inputPath, error, buf)
        LOGE("打开视频失败")
        return;
    }
    if (avformat_find_stream_info(avFormatContext, NULL) < 0) {
        LOGE("获取内容失败")
        return;
    }
    av_dump_format(avFormatContext, 0, inputPath, 0);
    //获取视频的编码信息
    AVCodecParameters *origin_par = NULL;
    int mVideoStreamIdx = -1;
    mVideoStreamIdx = av_find_best_stream(avFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (mVideoStreamIdx < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't find video stream in input file\n");
        return;
    }
    LOGE("成功找到视频流")
    origin_par = avFormatContext->streams[mVideoStreamIdx]->codecpar;
    // 寻找解码器 {start
    AVCodec *mVcodec = NULL;
    AVCodecContext *mAvContext = NULL;
    mVcodec = avcodec_find_decoder(origin_par->codec_id);
    mAvContext = avcodec_alloc_context3(mVcodec);
    if (!mVcodec || !mAvContext) {
        return;
    }
    //不初始化解码器context会导致MP4封装的mpeg4码流解码失败
    int ret = avcodec_parameters_to_context(mAvContext, origin_par);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error initializing the decoder context.\n");
    }
    // 打开解码器
    if (avcodec_open2(mAvContext, mVcodec, NULL) != 0){
        LOGE("打开失败")
        return;
    }
    LOGE("解码器打开成功")
    // 寻找解码器 end
    //申请AVPacket
    AVPacket *packet = (AVPacket *) av_malloc(sizeof(AVPacket));
    av_init_packet(packet);
    //申请AVFrame
    AVFrame *frame = av_frame_alloc();//分配一个AVFrame结构体,AVFrame结构体一般用于存储原始数据，指向解码后的原始帧
    AVFrame *rgb_frame = av_frame_alloc();//分配一个AVFrame结构体，指向存放转换成rgb后的帧

    //缓存区
    uint8_t  *out_buffer= (uint8_t *)av_malloc(avpicture_get_size(AV_PIX_FMT_RGBA,
                                                                  mAvContext->width,mAvContext->height));
    //与缓存区相关联，设置rgb_frame缓存区
    avpicture_fill((AVPicture *)rgb_frame,out_buffer,AV_PIX_FMT_RGBA,mAvContext->width,mAvContext->height);
    SwsContext* swsContext = sws_getContext(mAvContext->width,mAvContext->height,mAvContext->pix_fmt,
                                            mAvContext->width,mAvContext->height,AV_PIX_FMT_RGBA,
                                            SWS_BICUBIC,NULL,NULL,NULL);
    //取到nativewindow
    ANativeWindow *nativeWindow=ANativeWindow_fromSurface(env,surface);
    if(nativeWindow==0){
        LOGE("nativewindow取到失败")
        return;
    }
    //视频缓冲区
    ANativeWindow_Buffer native_outBuffer;

    while(1) {
        int ret = av_read_frame(avFormatContext, packet);
        if (ret != 0) {
            av_strerror(ret, buf, sizeof(buf));
            LOGE("--%s--\n", buf);
            av_packet_unref(packet);
            break;
        }
        if (ret >= 0 && packet->stream_index != mVideoStreamIdx) {
            av_packet_unref(packet);
            continue;
        }
        // 发送待解码包
        int result = avcodec_send_packet(mAvContext, packet);
        av_packet_unref(packet);
        if (result < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error submitting a packet for decoding\n");
            continue;
        }
        // 接收解码数据
        while (result >= 0) {
            result = avcodec_receive_frame(mAvContext, frame);
            if (result == AVERROR_EOF)
                break;
            else if (result == AVERROR(EAGAIN)) {
                result = 0;
                break;
            } else if (result < 0) {
                av_log(NULL, AV_LOG_ERROR, "Error decoding frame\n");
                av_frame_unref(frame);
                break;
            }
            LOGE("转换并绘制")
            //绘制之前配置nativewindow
            ANativeWindow_setBuffersGeometry(nativeWindow, mAvContext->width,
                                             mAvContext->height, WINDOW_FORMAT_RGBA_8888);
            //上锁
            ANativeWindow_lock(nativeWindow, &native_outBuffer, NULL);
            //转换为rgb格式
            sws_scale(swsContext, (const uint8_t *const *) frame->data, frame->linesize, 0,
                      frame->height, rgb_frame->data,
                      rgb_frame->linesize);
            //  rgb_frame是有画面数据
            uint8_t *dst = (uint8_t *) native_outBuffer.bits;
            //拿到一行有多少个字节 RGBA
            int destStride = native_outBuffer.stride * 4;
            //像素数据的首地址
            uint8_t *src = rgb_frame->data[0];
            //实际内存一行数量
            int srcStride = rgb_frame->linesize[0];
            //int i=0;
            for (int i = 0; i < mAvContext->height; ++i) {
                //将rgb_frame中每一行的数据复制给nativewindow
                memcpy(dst + i * destStride, src + i * srcStride, srcStride);
            }
            //解锁
            ANativeWindow_unlockAndPost(nativeWindow);
            av_frame_unref(frame);
        }
    }
    //释放
    ANativeWindow_release(nativeWindow);
    av_frame_free(&frame);
    av_frame_free(&rgb_frame);
    avcodec_close(mAvContext);
    avformat_free_context(avFormatContext);
    env->ReleaseStringUTFChars(input_str, inputPath);
}
/**
 * 播放视频流
 * R# 代表申请内存 需要释放或关闭
 */
extern "C"
JNIEXPORT void JNICALL
Java_com_grandstream_ffmpeg_MyUtils_playVideo(JNIEnv *env, jclass clazz, jstring input_str, jobject surface) {
    // 记录结果
    int result;
    // R1 Java String -> C String
    const char *path = env->GetStringUTFChars(input_str, 0);
    // 注册 FFmpeg 组件
    av_register_all();
    // R2 初始化 AVFormatContext 上下文
    AVFormatContext *format_context = avformat_alloc_context();
    // 打开视频文件
    result = avformat_open_input(&format_context, path, NULL, NULL);
    if (result < 0) {
        LOGE("Player Error : Can not open video file");
        return;
    }
    // 查找视频文件的流信息
    result = avformat_find_stream_info(format_context, NULL);
    if (result < 0) {
        LOGE("Player Error : Can not find video file stream info");
        return;
    }
    // 查找视频编码器
    int video_stream_index = -1;
    for (int i = 0; i < format_context->nb_streams; i++) {
        // 匹配视频流
        if (format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
        }
    }
    // 没找到视频流
    if (video_stream_index == -1) {
        LOGE("Player Error : Can not find video stream");
        return;
    }
    // 初始化视频编码器上下文
    AVCodecContext *video_codec_context = avcodec_alloc_context3(NULL);
    avcodec_parameters_to_context(video_codec_context, format_context->streams[video_stream_index]->codecpar);
    // 初始化视频编码器
    AVCodec *video_codec = avcodec_find_decoder(video_codec_context->codec_id);
    if (video_codec == NULL) {
        LOGE("Player Error : Can not find video codec");
        return;
    }
    // R3 打开视频解码器
    result  = avcodec_open2(video_codec_context, video_codec, NULL);
    if (result < 0) {
        LOGE("Player Error : Can not find video stream");
        return;
    }
    // 获取视频的宽高
    int videoWidth = video_codec_context->width;
    int videoHeight = video_codec_context->height;
    // R4 初始化 Native Window 用于播放视频
    ANativeWindow *native_window = ANativeWindow_fromSurface(env, surface);
    if (native_window == NULL) {
        LOGE("Player Error : Can not create native window");
        return;
    }
    // 通过设置宽高限制缓冲区中的像素数量，而非屏幕的物理显示尺寸。
    // 如果缓冲区与物理屏幕的显示尺寸不相符，则实际显示可能会是拉伸，或者被压缩的图像
    result = ANativeWindow_setBuffersGeometry(native_window, videoWidth, videoHeight,WINDOW_FORMAT_RGBA_8888);
    if (result < 0){
        LOGE("Player Error : Can not set native window buffer");
        ANativeWindow_release(native_window);
        return;
    }
    // 定义绘图缓冲区
    ANativeWindow_Buffer window_buffer;
    // 声明数据容器 有3个
    // R5 解码前数据容器 Packet 编码数据
    AVPacket *packet = av_packet_alloc();
    // R6 解码后数据容器 Frame 像素数据 不能直接播放像素数据 还要转换
    AVFrame *frame = av_frame_alloc();
    // R7 转换后数据容器 这里面的数据可以用于播放
    AVFrame *rgba_frame = av_frame_alloc();
    // 数据格式转换准备
    // 输出 Buffer
    int buffer_size = av_image_get_buffer_size(AV_PIX_FMT_RGBA, videoWidth, videoHeight, 1);
    // R8 申请 Buffer 内存
    uint8_t *out_buffer = (uint8_t *) av_malloc(buffer_size * sizeof(uint8_t));
    av_image_fill_arrays(rgba_frame->data, rgba_frame->linesize, out_buffer, AV_PIX_FMT_RGBA, videoWidth, videoHeight, 1);
    // R9 数据格式转换上下文
    struct SwsContext *data_convert_context = sws_getContext(
            videoWidth, videoHeight, video_codec_context->pix_fmt,
            videoWidth, videoHeight, AV_PIX_FMT_RGBA,
            SWS_BICUBIC, NULL, NULL, NULL);
    // 开始读取帧
    while (av_read_frame(format_context, packet) >= 0) {
        // 匹配视频流
        if (packet->stream_index == video_stream_index) {
            // 解码
            result = avcodec_send_packet(video_codec_context, packet);
            if (result < 0 && result != AVERROR(EAGAIN) && result != AVERROR_EOF) {
                LOGE("Player Error : codec step 1 fail");
                return;
            }
            result = avcodec_receive_frame(video_codec_context, frame);
            if (result < 0 && result != AVERROR_EOF) {
                LOGE("Player Error : codec step 2 fail");
                return;
            }
            // 数据格式转换
            result = sws_scale(
                    data_convert_context,
                    (const uint8_t* const*) frame->data, frame->linesize,
                    0, videoHeight,
                    rgba_frame->data, rgba_frame->linesize);
            if (result < 0) {
                LOGE("Player Error : data convert fail");
                return;
            }
            // 播放
            result = ANativeWindow_lock(native_window, &window_buffer, NULL);
            if (result < 0) {
                LOGE("Player Error : Can not lock native window");
            } else {
                // 将图像绘制到界面上
                // 注意 : 这里 rgba_frame 一行的像素和 window_buffer 一行的像素长度可能不一致
                // 需要转换好 否则可能花屏
                uint8_t *bits = (uint8_t *) window_buffer.bits;
                for (int h = 0; h < videoHeight; h++) {
                    memcpy(bits + h * window_buffer.stride * 4,
                           out_buffer + h * rgba_frame->linesize[0],
                           rgba_frame->linesize[0]);
                }
                ANativeWindow_unlockAndPost(native_window);
            }
        }
        // 释放 packet 引用
        av_packet_unref(packet);
    }
    // 释放 R9
    sws_freeContext(data_convert_context);
    // 释放 R8
    av_free(out_buffer);
    // 释放 R7
    av_frame_free(&rgba_frame);
    // 释放 R6
    av_frame_free(&frame);
    // 释放 R5
    av_packet_free(&packet);
    // 释放 R4
    ANativeWindow_release(native_window);
    // 关闭 R3
    avcodec_close(video_codec_context);
    // 释放 R2
    avformat_close_input(&format_context);
    // 释放 R1
    env->ReleaseStringUTFChars(input_str, path);
}
extern "C"
JNIEXPORT void JNICALL
Java_com_grandstream_ffmpeg_MyUtils_playAudio(JNIEnv *env, jclass clazz, jobject instance, jstring input_str) {
    // 记录结果
    int result;
    // R1 Java String -> C String
    const char *path = env->GetStringUTFChars(input_str, 0);
    // 注册组件
    av_register_all();
    // R2 创建 AVFormatContext 上下文
    AVFormatContext *format_context = avformat_alloc_context();
    // R3 打开视频文件
    avformat_open_input(&format_context, path, NULL, NULL);
    // 查找视频文件的流信息
    result = avformat_find_stream_info(format_context, NULL);
    if (result < 0) {
        LOGE("Player Error : Can not find video file stream info");
        return;
    }
    // 查找音频编码器
    int audio_stream_index = -1;
    for (int i = 0; i < format_context->nb_streams; i++) {
        // 匹配音频流
        if (format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_index = i;
        }
    }
    // 没找到音频流
    if (audio_stream_index == -1) {
        LOGE("Player Error : Can not find audio stream");
        return;
    }
    // 初始化音频编码器上下文
    AVCodecContext *audio_codec_context = avcodec_alloc_context3(NULL);
    avcodec_parameters_to_context(audio_codec_context, format_context->streams[audio_stream_index]->codecpar);
    // 初始化音频编码器
    AVCodec *audio_codec = avcodec_find_decoder(audio_codec_context->codec_id);
    if (audio_codec == NULL) {
        LOGE("Player Error : Can not find audio codec");
        return;
    }
    // R4 打开视频解码器
    result  = avcodec_open2(audio_codec_context, audio_codec, NULL);
    if (result < 0) {
        LOGE("Player Error : Can not open audio codec");
        return;
    }
    // 音频重采样准备
    // R5 重采样上下文
    struct SwrContext *swr_context = swr_alloc();
    // 缓冲区
    uint8_t *out_buffer = (uint8_t *) av_malloc(44100 * 2);
    // 输出的声道布局 (双通道 立体音)
    uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO;
    // 输出采样位数 16位
    enum AVSampleFormat out_format = AV_SAMPLE_FMT_S16;
    // 输出的采样率必须与输入相同
    int out_sample_rate = audio_codec_context->sample_rate;
    //swr_alloc_set_opts 将PCM源文件的采样格式转换为自己希望的采样格式
    swr_alloc_set_opts(swr_context,
                       out_channel_layout, out_format, out_sample_rate,
                       audio_codec_context->channel_layout, audio_codec_context->sample_fmt, audio_codec_context->sample_rate,
                       0, NULL);
    swr_init(swr_context);
    // 调用 Java 层创建 AudioTrack
    int out_channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
    jclass player_class = env->GetObjectClass(instance);
    jmethodID create_audio_track_method_id = env->GetMethodID(player_class, "createAudioTrack", "(II)V");
    env->CallVoidMethod(instance, create_audio_track_method_id, 44100, out_channels);
    // 播放音频准备
    jmethodID play_audio_track_method_id = env->GetMethodID(player_class, "playAudioTrack", "([BI)V");
    // 声明数据容器 有2个
    // R6 解码前数据容器 Packet 编码数据
    AVPacket *packet = av_packet_alloc();
    // R7 解码后数据容器 Frame MPC数据 还不能直接播放 还要进行重采样
    AVFrame *frame = av_frame_alloc();
    // 开始读取帧
    while (av_read_frame(format_context, packet) >= 0) {
        // 匹配音频流
        if (packet->stream_index == audio_stream_index) {
            // 解码
            result = avcodec_send_packet(audio_codec_context, packet);
            if (result < 0 && result != AVERROR(EAGAIN) && result != AVERROR_EOF) {
                LOGE("Player Error : codec step 1 fail");
                return;
            }
            result = avcodec_receive_frame(audio_codec_context, frame);
            if (result < 0 && result != AVERROR_EOF) {
                LOGE("Player Error : codec step 2 fail");
                return;
            }
            LOGE("--")
            // 重采样
            swr_convert(swr_context, &out_buffer, 44100 * 2, (const uint8_t **) frame->data, frame->nb_samples);
            // 播放音频
            // 调用 Java 层播放 AudioTrack
            int size = av_samples_get_buffer_size(NULL, out_channels, frame->nb_samples, AV_SAMPLE_FMT_S16, 1);
            jbyteArray audio_sample_array = env->NewByteArray(size);
            env->SetByteArrayRegion(audio_sample_array, 0, size, (const jbyte *) out_buffer);
            env->CallVoidMethod(instance, play_audio_track_method_id, audio_sample_array, size);
            env->DeleteLocalRef(audio_sample_array);
        }
        // 释放 packet 引用
        av_packet_unref(packet);
    }
    // 调用 Java 层释放 AudioTrack
    jmethodID release_audio_track_method_id = env->GetMethodID(player_class, "releaseAudioTrack", "()V");
    env->CallVoidMethod(instance, release_audio_track_method_id);
    // 释放 R7
    av_frame_free(&frame);
    // 释放 R6
    av_packet_free(&packet);
    // 释放 R5
    swr_free(&swr_context);
    // 关闭 R4
    avcodec_close(audio_codec_context);
    // 关闭 R3
    avformat_close_input(&format_context);
    // 释放 R2
    avformat_free_context(format_context);
    // 释放 R1
    env->ReleaseStringUTFChars(input_str, path);
}