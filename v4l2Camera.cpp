// V4L2 Camera interface for webcamxtra/JMyron.
// Copyright, 2008 Stephen Detwiler

#include "v4l2Camera.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <memory.h>
#include <sys/mman.h>

#include <dirent.h>
#include <linux/videodev2.h>

V4L2Camera::V4L2Camera()
{
    mFd = 0;
    mBuffers = 0;
    mNumBuffers = 0;
   
    mVideoDeviceCount = 0;
    mCountedDevices = false;
    mRgb = 0;
   
}

V4L2Camera::~V4L2Camera()
{
    closeCamera();
}

bool V4L2Camera::findCamera()
{
    return findCamera(0);
}

bool V4L2Camera::findCamera(unsigned int cameraId)
{
    getVideoDevices();
    
    if(cameraId >= mVideoDeviceCount)
    {
        return false;
    }

    mCameraId = cameraId;
    mCamera = mVideoDevices+mCameraId;

    int rc;
    struct stat deviceStat;
    
    // Check if device exists.
    rc = stat(mCamera->mPath, &deviceStat);
    if(rc < 0)
    {
        printf("Failed to stat %s (%d).\n", mCamera->mPath, rc);
        return false;
    }

    // Check if is symlink.
    if(S_ISLNK(deviceStat.st_mode))
    {
        printf("%s is a symlink. Symlinks not supported\n", mCamera->mPath);
        return false;
    }

    // Open camera.
    mFd = open(mCamera->mPath, O_RDWR|O_NONBLOCK, 0);
    if(mFd < 0)
    {
        printf("Failed to open %s.\n", mCamera->mPath);
        return false;
    }
 
    return true;
}

void V4L2Camera::getCameraCapabilities()
{
    if(!mCamera)
        return;

    int rc;
    struct v4l2_capability cap;
    struct v4l2_format format;

    rc = ioctl(mFd, VIDIOC_QUERYCAP, &cap);
    if(rc < 0)
    {
        printf("VIDIOC_QUERYCAP failed for %s\n", mCamera->mPath);
        return;
    }
    printf("Device\t%s\nDriver\t%s\n", cap.card, cap.driver);
    printf ("Version\t%u.%u.%u\n",
        (cap.version >> 16) & 0xFF,
        (cap.version >> 8) & 0xFF,
         cap.version & 0xFF);
    
    printf("\n");
    
    printf("Device Capture Formats\n-----------------------------------\n");
    
    for(unsigned int i=0; true; ++i)
    {
        v4l2_fmtdesc desc;
        memset(&desc, 0, sizeof(desc));
        desc.index = i;
        desc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        rc = ioctl(mFd, VIDIOC_ENUM_FMT, &desc);
        if(rc < 0)
            break;

        char a,b,c,d;
        a = desc.pixelformat >> 0;
        b = desc.pixelformat >> 8; 
        c = desc.pixelformat >> 16;
        d = desc.pixelformat >> 24;
       
        printf("%d\t%c%c%c%c\t%s\n", i,
               a, b, c, d,
               desc.description);
    
    }

    printf("\n");
}

        

bool V4L2Camera::initCamera(int width, int height, bool color)
{
    int rc;

    if(!mCamera)
        return false;

    getCameraCapabilities();
    
    struct v4l2_capability cap;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    struct v4l2_format format;

    rc = ioctl(mFd, VIDIOC_QUERYCAP, &cap);
    if(rc < 0)
    {
        printf("VIDIOC_QUERYCAP failed for %s\n", mCamera->mPath);
        return false;
    }
    

    if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        printf("%s cannot capture video.\n", mCamera->mPath);
        return false;
    }
    
    if(!(cap.capabilities & V4L2_CAP_STREAMING))
    {
        printf("%s does not support streaming I/O.\n", mCamera->mPath);
        return false;
    }

    memset(&cropcap, 0, sizeof(cropcap));
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    rc = ioctl(mFd, VIDIOC_CROPCAP, &cropcap);
    if(rc < 0)
    {
        printf("VIDIOC_CROPCAP failed for %s\n", mCamera->mPath);
        //        return false;
    }
    else
    {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect; // resetting to default mode.
        rc = ioctl(mFd, VIDIOC_S_CROP, &crop);
    }

    memset(&format, 0, sizeof(format));

    format.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width       = width; 
    format.fmt.pix.height      = height;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV; // SCD... duh select from available options.
    format.fmt.pix.field       = V4L2_FIELD_INTERLACED;
        
    rc = ioctl(mFd, VIDIOC_S_FMT, &format);
    if(rc < 0)
    {
        printf("Failed to set format for %s\n", mCamera->mPath);
        return false;
    }

    struct v4l2_streamparm parm;
    memset(&parm, 0, sizeof(parm));
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    rc = ioctl(mFd, VIDIOC_G_PARM, &parm);
    if(rc < 0)
    {
        printf("Failed to get stream parameters for %s\n", mCamera->mPath);
        return false;
    }
//    printf("before %u/%u\n", 
//           parm.parm.capture.timeperframe.numerator,
//           parm.parm.capture.timeperframe.denominator);
    
    
    parm.parm.capture.timeperframe.numerator=1;
    parm.parm.capture.timeperframe.denominator=30;
    
    
    rc = ioctl(mFd, VIDIOC_S_PARM, &parm);
    if(rc < 0)
    {
        printf("Failed to get stream parameters for %s\n", mCamera->mPath);
        return false;
    }

//    rc = ioctl(mFd, VIDIOC_G_PARM, &parm);
//    if(rc < 0)
//    {
//        printf("Failed to get stream parameters for %s\n", mCamera->mPath);
//        return false;
//    }
//    printf("after %u/%u\n", 
//           parm.parm.capture.timeperframe.numerator,
//           parm.parm.capture.timeperframe.denominator);
    

    // Calculate minimum buffer size and don't trust what the driver tells us.
    // All assuming YUYV.
    unsigned int min;
    min = format.fmt.pix.width * 2;
	if (format.fmt.pix.bytesperline < min)
        format.fmt.pix.bytesperline = min;

    min = format.fmt.pix.bytesperline * format.fmt.pix.height;
    if (format.fmt.pix.sizeimage < min)
        format.fmt.pix.sizeimage = min;    // Set camera to requested width and height.

    mCamera->mWidth = format.fmt.pix.width;
    mCamera->mHeight = format.fmt.pix.height;

    mFrameBufSize = format.fmt.pix.sizeimage;


    // Set up mmap streaming.
    v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    rc = ioctl(mFd, VIDIOC_REQBUFS, &req);
    if(rc < 0)
    {
        printf("Failed to get mmap buffer for %s\n", mCamera->mPath);
        return false;
    }

    if(req.count < 2)
    {
        printf("Only got %d buffers, need at least 2.", req.count);
        return false;
    }

    mNumBuffers = req.count;
    mBuffers = new VideoBuffer[mNumBuffers];

    for(int i=0; i<mNumBuffers; ++i)
    {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        rc = ioctl(mFd, VIDIOC_QUERYBUF, &buf);
        if(rc < 0)
        {
            printf("QUERYBUF failed\n");
            return false;
        }

        mBuffers[i].mLength = buf.length;
        mBuffers[i].mStart = mmap(NULL,
                                  buf.length,
                                  PROT_READ | PROT_WRITE,
                                  MAP_SHARED,
                                  mFd,
                                  buf.m.offset);

        if(mBuffers[i].mStart == MAP_FAILED)
        {
            printf("mmap failed\n");
            return false;
        }
    }
    
        
        
    


    //    mFrameBuf = new unsigned char[mFrameBufSize];

    mRgbLen = mCamera->mWidth * mCamera->mHeight * 3;
    mRgb = new unsigned char[mRgbLen];
    
    //printf("%s initialized at %d x %d.\n", mCamera->mPath, mCamera->mWidth, mCamera->mHeight);
    
    return true;
}

bool V4L2Camera::startCamera()
{
    int rc;
    if(!mCamera)
        return false;


    for(int i=0; i<mNumBuffers; ++i)
    {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = i;

        rc = ioctl(mFd, VIDIOC_QBUF, &buf);
        if(rc < 0)
        {
            printf("Failed to attach buffer.\n");
            return false;
        }
    }

	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    rc = ioctl(mFd, VIDIOC_STREAMON, &type);
    if(rc < 0)
    {
        printf("Failed to enable streaming.\n");
        return false;
    }

    return true;
}

int V4L2Camera::getWidth()
{
    if(!mCamera)
        return 0;
    
    // printf("Requested width and height: %d x %d\n", mCamera->mWidth, mCamera->mHeight);
    return mCamera->mWidth;
}

int V4L2Camera::getHeight()
{
    if(!mCamera)
        return 0;

    // printf("Requested width and height: %d x %d\n", mCamera->mWidth, mCamera->mHeight);
    return mCamera->mHeight;
}

#define max(a, b) a>b?a:b
#define min(a, b) a<b?a:b

unsigned char* V4L2Camera::convertYUYVtoRGB(unsigned char* yuyv)
{
    for(int i=0, j=0; i<mRgbLen; i+=6, j+=4)
    {

//        float A;
//        float B;
//        float C;
//        float D;
//        float E;
//
//        unsigned char vMinus128;
//        unsigned char uMinus128;
//        unsigned char yMinus16;
//        
//        vMinus128 = min(max((int)yuyv[j+3] - 128, 0), 240);
//        uMinus128 = min(max((int)yuyv[j+1] - 128, 0), 240);
//        yMinus16  = min(max((int)yuyv[j+0] - 16, 0), 240);
//
//        A = 1.164f * (yMinus16);
//        B = 1.596f * (vMinus128);
//        C = 0.391f * (uMinus128);
//        D = 0.813f * (vMinus128);    
//        E = 2.018f * (uMinus128);
//        
//        mRgb[i+0] = (unsigned char) min((A + B), 240.0f);
//        mRgb[i+1] = (unsigned char) min((A - D - C), 240.0f);
//        mRgb[i+2] = (unsigned char) min((A +                 E), 240.0f);
//       
//        yMinus16  = min(max((int)yuyv[j+2] - 16, 0), 240.0f);
//        A = 1.164f * (yMinus16);
//
//        mRgb[i+3] = (unsigned char) min((A + B), 240.0f);
//        mRgb[i+4] = (unsigned char) min((A - D - C), 240.0f);
//        mRgb[i+5] = (unsigned char) min((A +                  E), 240.0f);

        // value, min, max
#define MINMAX(a, b, c) a<b?b: a>c?c:a
        
        float y  = yuyv[j+0];
        float u  = yuyv[j+1];
        float y2 = yuyv[j+2];
        float v  = yuyv[j+3];
        float r,g,b;

        float A;
        float B;
        float C;
        float D;
        float E;
        A = 1.164f * (y-16);
        B = 1.596f * (v-128);
        C = 0.391f * (u-128);
        D = 0.813f * (v-128);    
        E = 2.018f * (u-128);

        r = A  + B;
        g = A  - D - C;
        b = A  +                 E;
       
        mRgb[i+0] = (unsigned char)(MINMAX(r, 0, 255));
        mRgb[i+1] = (unsigned char)(MINMAX(g, 0, 255));
        mRgb[i+2] = (unsigned char)(MINMAX(b, 0, 255));

        A = 1.164f * (y2-16);

        r = A  + B;
        g = A  - D - C;
        b = A  +                 E;
        
        mRgb[i+3] = (unsigned char)(MINMAX(r, 0, 255));
        mRgb[i+4] = (unsigned char)(MINMAX(g, 0, 255));
        mRgb[i+5] = (unsigned char)(MINMAX(b, 0, 255));
         

///        r = 1.164f*(y-16)  + 1.596f*(v-128);
///        g = 1.164f*(y-16)  - 0.813f*(v-128) - 0.391f*(u-128);
///        b = 1.164f*(y-16)  +                 2.018f*(u-128);
///       
///        mRgb[i+0] = (unsigned char)(MINMAX(r, 0, 255));
///        mRgb[i+1] = (unsigned char)(MINMAX(g, 0, 255));
///        mRgb[i+2] = (unsigned char)(MINMAX(b, 0, 255));
///
///        r = 1.164*(y2-16) + 1.596*(v-128);
///        g = 1.164*(y2-16) - 0.813*(v-128) - 0.391*(u-128);
///        b = 1.164*(y2-16) +                 2.018*(u-128);
///        
///        mRgb[i+3] = (unsigned char)(MINMAX(r, 0, 255));
///        mRgb[i+4] = (unsigned char)(MINMAX(g, 0, 255));
///        mRgb[i+5] = (unsigned char)(MINMAX(b, 0, 255));

        
//        unsigned char y  = yuyv[j+0];
//        unsigned char u  = yuyv[j+1];
//        unsigned char y2 = yuyv[j+2];
//        unsigned char v  = yuyv[j+3];
//        mRgb[i+0] = 1.164*(y-16)  + 1.596*(v-128);
//        mRgb[i+1] = 1.164*(y-16)  - 0.813*(v-128) - 0.391*(u-128);
//        mRgb[i+2] = 1.164*(y-16)  +                 2.018*(u-128);
//       
//        mRgb[i+3] = 1.164*(y2-16) + 1.596*(v-128);
//        mRgb[i+4] = 1.164*(y2-16) - 0.813*(v-128) - 0.391*(u-128);
//        mRgb[i+5] = 1.164*(y2-16) +                 2.018*(u-128);

    }

    return mRgb;
}



unsigned char* V4L2Camera::getFrame()
{
    int rc;
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    rc = ioctl(mFd, VIDIOC_DQBUF, &buf);
    if(rc < 0)
    {
        if(errno == EAGAIN)
        {
            //            printf("EAGAIN\n");
            return mRgb;
        }
        
        //        printf("DQBUF failed: %d\n", errno);
        return mRgb;
    }

    convertYUYVtoRGB((unsigned char*)mBuffers[buf.index].mStart);

    rc = ioctl(mFd, VIDIOC_QBUF, &buf);
    if(rc < 0)
    {
        printf("QBUF failed\n");
    }

    return mRgb;
}



void V4L2Camera::showSettingsDialog()
{
    
}


bool V4L2Camera::stopCamera()
{
    int rc;
    
    if(!mCamera)
        return true;

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    rc = ioctl(mFd, VIDIOC_STREAMOFF, &type);
    if(rc < 0)
    {
        printf("Failed to turn off streaming.\n");
    }
    
    return true;
}


bool V4L2Camera::closeCamera()
{
    if(mFd)
    {
        close(mFd);
        mFd = 0;
    }

    for(int i=0; i<mNumBuffers; ++i)
        munmap(mBuffers[i].mStart, mBuffers[i].mLength);

    delete[] mBuffers;
    

    if(mRgb)
        delete[] mRgb;
    
    mCamera = NULL;
    
    return true;
}


unsigned int V4L2Camera::countCameras()
{
    unsigned int videoDeviceCount = 0;
    
    // Iterate over /dev and look for all entries named video*.
    
    DIR* dir = opendir("/dev");
    if(dir == 0)
        return 0;

    while(true)
    {
        struct dirent* de = readdir(dir);
        if(!de)
            break;

        if(strstr(de->d_name, "video"))
        {
            ++videoDeviceCount;
        }
    }

    closedir(dir);
    
    return videoDeviceCount;
}


unsigned int V4L2Camera::getVideoDevices()
{
    if(mCountedDevices)
        return mVideoDeviceCount;

    mCountedDevices = true;
    
    mVideoDeviceCount = 0;
    
    memset(mVideoDevices, 0, sizeof(mVideoDevices));

    // Iterate over /dev and look for all entries named video*.


    DIR* dir = opendir("/dev");
    if(dir == 0)
        return 0;

    while(true)
    {
        struct dirent* de = readdir(dir);
        if(!de)
            break;

        if(strstr(de->d_name, "video"))
        {
            snprintf(mVideoDevices[mVideoDeviceCount].mPath, sizeof(mVideoDevices[mVideoDeviceCount].mPath), "/dev/%s", de->d_name);
            ++mVideoDeviceCount;
        }
    }

    closedir(dir);
    
    return mVideoDeviceCount;
}
