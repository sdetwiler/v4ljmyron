// V4L2 Camera interface for webcamxtra/JMyron.
// Copyright, 2008 Stephen Detwiler

#ifndef __V4L2CAMERA_H__
#define __V4L2CAMERA_H___

#define MAX_VIDEODEVICE_COUNT 4

class V4L2Camera
{
    public:
        V4L2Camera();
        ~V4L2Camera();
        // Find the first camera attached to the system.
        bool findCamera();

        // Find a specific camera attached to the system. Zero based
        // index.
        bool findCamera(unsigned int cameraId);

        // Initialize the previously found camera for a specific width
        // and height. The color paramater is ignored in the
        // implementation.
        bool initCamera(int width, int height, bool color);

        // Start capture for the previously initialized camera.
        bool startCamera();

        // Returns the initialized width of the camera.
        int getWidth();

        // Returns the initialized height of the camera.
        int getHeight();
        
        // Get a frame from the previously started camera.
        unsigned char* getFrame();

        // Not implemented.
        void showSettingsDialog();

        // Stop a camera that was previously started.
        bool stopCamera();

        // Close a previously initialized camera.
        bool closeCamera();

        // Count the number of cameras attached to the system.
        static unsigned int countCameras();

        void getCameraCapabilities();
        
    private:
        struct VideoDevice
        {
            char mPath[512]; // File path to device.
            int mMaxWidth;   // Maximum width.
            int mMaxHeight;  // Maximum height.
            int mMinWidth;   // Minimum width.
            int mMinHeight;  // Minimum height.
            int mBitDepth;   // Set bit depth.
            int mWidth;      // Configured width.
            int mHeight;     // Configured height.
        };

        struct VideoBuffer
        {
            void* mStart;
            unsigned int mLength;
        };

        unsigned char* convertYUYVtoRGB(unsigned char* yuyv);

        // Gets locations of attached video devices and resets
        // their configuration settings.
        unsigned int getVideoDevices();

        unsigned int   mFrameBufSize;
        VideoBuffer* mBuffers;
        unsigned int mNumBuffers;

        unsigned char* mRgb;//[640*480*3];
        unsigned int mRgbLen;
        
        VideoDevice*   mCamera;
        unsigned int   mCameraId;
        int            mFd;

        bool           mCountedDevices;
        
        VideoDevice mVideoDevices[MAX_VIDEODEVICE_COUNT];
        unsigned int mVideoDeviceCount;
};



#endif // __V4LCAMERA_H__
