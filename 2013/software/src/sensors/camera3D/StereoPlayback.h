#ifndef STEREOPLAYBACK_H
#define STEREOPLAYBACK_H

#include "sensors/camera3D/StereoSource.hpp"

class StereoPlayback : public StereoSource
{
    public:
        StereoPlayback(std::string leftVideo, std::string rightVideo, int fps);
        void Run();
        virtual ~StereoPlayback();

    private:
        VideoCapture _leftVid;
        VideoCapture _rightVid;
        int _framesPerSecond;
        StereoPair _images;
        //bool _running;
};

#endif // STEREOPLAYBACK_H