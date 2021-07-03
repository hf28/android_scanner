
#include "Logger.h"

void Logger::setImage(Mat image, float time)
{
    img.image = image;
    img.time = time;

//    setImageSet(img);
}

void Logger::setLocation(double lat, double lng, double alt, float time)
{
    loc.lat = lat;
    loc.lng = lng;
    loc.alt = alt;
    loc.time = time;

    bufferLocation(loc);

    if (refLoc.time == 0)
    {
        refLoc.lat = lat;
        refLoc.lng = lng;
        refLoc.alt = alt;
        refLoc.time = time;
    }
}

void Logger::setOrientation(double roll, double pitch, double azimuth, float time)
{
    orn.roll = roll*PI/180;
    orn.pitch = pitch*PI/180;
    orn.azimuth = azimuth*PI/180;
    orn.time = time;
//    __android_log_print(ANDROID_LOG_ERROR, "TRACKERS", "%s", "Str--------------------------------------------------------------");

    bufferOrientation(orn);
}

void Logger::bufferLocation(Location loc)
{
    if (locationBuffer.size() < locBufLen)
    {
        locationBuffer.push_back(loc);
    }
    else
    {
        locationBuffer.erase(locationBuffer.begin());
        locationBuffer.push_back(loc);
    }
}

void Logger::bufferOrientation(Orientation orn)
{
    if (orientationBuffer.size() < ornBufLen)
    {
        orientationBuffer.push_back(orn);
    }
    else
    {
        orientationBuffer.erase(orientationBuffer.begin());
        orientationBuffer.push_back(orn);
    }
}

ImageSet Logger::getImageSet()
{
    Location location;
    Orientation orientation;
    float dist = 0, minDist = 1e7;

    for(int k=0; k<locBufLen; k++)
    {
        dist = fabs(locationBuffer[k].time - img.time);
        if (dist < minDist)
        {
            location = locationBuffer[k];
            minDist = dist;
        }
    }

    dist = 0;
    minDist = 1e7;
    for(int k=0; k<ornBufLen; k++)
    {
        dist = fabs(orientationBuffer[k].time - img.time);
        if (dist < minDist)
        {
            orientation = orientationBuffer[k];
            minDist = dist;
        }
    }

    ImageSet imgSet;

    imgSet.image = img.image;
    imgSet.lat = location.lat;
    imgSet.lng = location.lng;
    imgSet.alt = location.alt;
    imgSet.roll = orientation.roll;
    imgSet.pitch = orientation.pitch;
    imgSet.azimuth = orientation.azimuth;
    imgSet.time = img.time;

    return imgSet;
}

ImuSet Logger::getImuSet()
{
    Location location;
    float dist = 0, minDist = 1e7;

    for(int k=0; k<locBufLen; k++)
    {
        dist = fabs(locationBuffer[k].time - orn.time);
        if (dist < minDist)
        {
            location = locationBuffer[k];
            minDist = dist;
        }
    }

    imuSet.roll = orn.roll;
    imuSet.pitch = orn.pitch;
    imuSet.azimuth = orn.azimuth;
    imuSet.lat = location.lat;
    imuSet.lng = location.lng;
    imuSet.alt = location.alt;
    imuSet.time = orn.time;

    return imuSet;
}