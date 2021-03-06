#pragma once

#include <sstream>
#include <opencv2/core/core.hpp>

//template <class T>
inline std::ostream & operator<<(std::ostream &stream, const cv::Point &p)
{
    return stream << p.x << " " << p.y << std::endl;
}

//template <class T>
inline std::ostream & operator<<(std::ostream &stream, const cv::Rect &r)
{
    return stream << r.x << " " << r.y << " " << r.width << " " << r.height << std::endl;
}

struct LandMarks
{
    // Eye Land marks
    cv::Point eyeLeftPupil;  ///<- Position of the left eye pupil
    cv::Point eyeRightPupil; ///<- Position of the right eye pupil
    cv::Rect  vjLeftEyeRect;  ///<- Rectangle where the left eye was detected using Viola Jones algorithm
    cv::Rect  vjRightEyeRect; ///<- Rectangle where the left eye was detected using Viola Jones algorithm

    int imageRotation;  ///<- Possible values are 0, 90, -90, 180

    // Mouth marks
    cv::Point lipUpperCenter;
    cv::Point lipLowerCenter;
    cv::Point lipLeftCorner;
    cv::Point lipRightCorner;
    cv::Rect vjMouthRect;

    cv::Rect  vjFaceRect;
    cv::Point crownPoint;
    cv::Point chinPoint;
    std::vector<cv::Point> lipContour1st;
    std::vector<cv::Point> lipContour2nd;

    std::vector<cv::Point> allLandmarks;

    std::string toString() const;

    std::string toJson() const;
};