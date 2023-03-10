package com.grandstream.ffmpeg;

import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;

import com.grandstream.ffmpeg.databinding.ActivityVideoPlayBinding;

public class VideoPlayOld extends AppCompatActivity {
    private ActivityVideoPlayBinding binding;
    private SurfaceView mSurfaceView;
    private SurfaceHolder mSurfaceHolder;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityVideoPlayBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());
        mSurfaceView = binding.surfaceView;
        mSurfaceHolder = mSurfaceView.getHolder();
        binding.btnPlayVideo.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                MyUtils.playVideoOld("/sdcard/input.mp4", mSurfaceHolder.getSurface());
            }
        });
    }
}