package com.grandstream.ffmpeg;

import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.view.Surface;

public class MyUtils {
    static {
        System.loadLibrary("ffmpeg_android_demo");
    }

    public static native String getAvcodecConfiguration();

    public static native void playVideoOld(String inputStr, Surface surface);

    public static native void playVideoNew(String inputStr, Surface surface);

    public static native void playVideo(String inputStr, Surface surface);

    public static native void playAudio(Object instance, String inputStr);

    private AudioTrack audioTrack;

    /**
     * 创建 AudioTrack
     * 由 C 反射调用
     * @param sampleRate  采样率
     * @param channels     通道数
     */
    public void createAudioTrack(int sampleRate, int channels) {
        int channelConfig;
        if (channels == 1) {
            channelConfig = AudioFormat.CHANNEL_OUT_MONO;
        } else if (channels == 2) {
            channelConfig = AudioFormat.CHANNEL_OUT_STEREO;
        }else {
            channelConfig = AudioFormat.CHANNEL_OUT_MONO;
        }
        int bufferSize = AudioTrack.getMinBufferSize(sampleRate, channelConfig, AudioFormat.ENCODING_PCM_16BIT);
        audioTrack = new AudioTrack(AudioManager.STREAM_MUSIC, sampleRate, channelConfig,
                AudioFormat.ENCODING_PCM_16BIT, bufferSize, AudioTrack.MODE_STREAM);
        audioTrack.play();
    }

    /**
     * 播放 AudioTrack
     * 由 C 反射调用
     * @param data
     * @param length
     */
    public void playAudioTrack(byte[] data, int length) {
        if (audioTrack != null && audioTrack.getPlayState() == AudioTrack.PLAYSTATE_PLAYING) {
            audioTrack.write(data, 0, length);
        }
    }

    /**
     * 释放 AudioTrack
     * 由 C 反射调用
     */
    public void releaseAudioTrack() {
        if (audioTrack != null) {
            if (audioTrack.getPlayState() == AudioTrack.PLAYSTATE_PLAYING) {
                audioTrack.stop();
            }
            audioTrack.release();
            audioTrack = null;
        }
    }
}
