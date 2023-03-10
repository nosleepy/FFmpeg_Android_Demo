package com.grandstream.ffmpeg;

import android.os.Bundle;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;

import androidx.appcompat.app.AppCompatActivity;

import com.grandstream.ffmpeg.databinding.ActivityVideoPlayBinding;

public class VideoPlayNew extends AppCompatActivity {
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
                MyUtils.playVideo("/sdcard/input.mp4", mSurfaceHolder.getSurface());
            }
        });
    }
}