#include <fstream>
#include <regex>

#include <gtest/gtest.h>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/objdetect/objdetect.hpp>

#include "TestHelpers.h"
#include <map>

#if _MSC_VER >= 1910 // VS 2017
#include <filesystem>
namespace fs = std::experimental::filesystem;
#elif _MSC_VER >= 1900 // VS 2015
#include <filesystem>
namespace fs = std::tr2::sys;
// #elif __GNUC__ &&  __GNUC__ >= 7
// #include <experimental/filesystem>
// namespace fs = std::experimental::filesystem::v1;
#else
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;
#endif

#define LANDMARK_POINT(mat, row) cv::Point(ROUND_INT((mat).at<float>((row), 0)), ROUND_INT((mat).at<float>((row), 1)))

std::string resolvePath(const std::string & relPath)
{
    auto baseDir = fs::current_path();
    while (baseDir.has_parent_path())
    {
        auto combinePath = baseDir / relPath;
        if (exists(combinePath))
        {
            return combinePath.string();
        }
        baseDir = baseDir.parent_path();
    }
    return std::string();
}

std::string pathCombine(const std::string & prefix, const std::string & suffix)
{
    return (fs::path(prefix) / fs::path(suffix)).string();
}

void getImageFiles(const std::string & testImagesDir, std::vector<std::string> & imageFilenames)
{
    std::vector<std::string> supportedImageExtensions = { ".jpg", /*".png",*/ ".bmp" };
    fs::directory_iterator endIter;
    if (exists(fs::path(testImagesDir)) && is_directory(fs::path(testImagesDir)))
    {
        for (fs::directory_iterator itr(testImagesDir); itr != endIter; ++itr)
        {
            if (!is_regular_file(itr->status()))
            {
                continue;
            }
            auto filePath = itr->path();
            auto fileExt = filePath.extension().string();
            std::transform(fileExt.begin(), fileExt.end(), fileExt.begin(), tolower);
            if (std::find(supportedImageExtensions.begin(), supportedImageExtensions.end(), fileExt)
                != supportedImageExtensions.end())
            {
                imageFilenames.push_back(filePath.string());
            }
        }
    }
}

std::string getFileName(const std::string & filePath)
{
    return fs::path(filePath).filename().string();
}

std::string getDirectory(const std::string & fullPath)
{
    return fs::path(fullPath).parent_path().string();
}

/**
 * \brief Loads the landmarks manually annotated from a CSV file
 * \param csvFilePath Path to the CSV file containing the annotation in VIA format
 * \param landMarksMap image name to landmarks map
 * \return true if the  method succeeeds importing the data, false otherwise.
 */
void importLandMarks(const std::string & csvFilePath, std::map<std::string, LandMarks> & landMarksMap)
{
    const std::string pattern
        = "(.*\\.(jpg|JPG|png|PNG)),\\d+,\"\\{\\}\",6,(\\d),\".*\"\"cx\"\":(\\d+),\"\"cy\"\":(\\d+)\\}\",\"\\{\\}\"";
    std::regex e(pattern);

    std::ifstream t(csvFilePath);
    std::string csv_content((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
    const auto imageDir = getDirectory(csvFilePath);

    for (auto it = std::sregex_iterator(csv_content.begin(), csv_content.end(), e); it != std::sregex_iterator(); ++it)
    {
        const auto & m = *it;
        for (auto x : m)
        {
            std::string imageName = m[1];
            auto fullImagePath = (fs::path(imageDir) / fs::path(imageName)).string();
            auto landmarkIdx = stoi(m[3]);
            cv::Point coord(stoi(m[4]), stoi(m[5]));
            switch (landmarkIdx)
            {
                case 0:
                    landMarksMap[fullImagePath].crownPoint = coord;
                    break;
                case 1:
                    landMarksMap[fullImagePath].chinPoint = coord;
                    break;
                case 2:
                    landMarksMap[fullImagePath].eyeLeftPupil = coord;
                    break;
                case 3:
                    landMarksMap[fullImagePath].eyeRightPupil = coord;
                    break;
                case 4:
                    landMarksMap[fullImagePath].lipLeftCorner = coord;
                    break;
                case 5:
                    landMarksMap[fullImagePath].lipRightCorner = coord;
                    break;
                default:
                    throw std::runtime_error("Invalid landmark index when reading from CSV file");
            }
        }
    }
}

bool importSCFaceLandMarks(const std::string & txtFileName, cv::Mat & output)
{
    std::ifstream textFile(txtFileName);
    std::string str;
    auto numRows = 0;
    auto numCols = 0;
    std::vector<float> data;

    while (getline(textFile, str))
    {
        std::stringstream stream(str);
        auto numValuesInRow = 0;
        while (true)
        {
            float value;
            stream >> value;
            if (!stream)
            {
                break;
            }
            data.push_back(value);
            numValuesInRow++;
        }
        if (numRows == 0)
        {
            numCols = numValuesInRow;
        }
        else if (numValuesInRow == 0)
        {
            break; // Empty line
        }
        else if (numCols != numValuesInRow)
        {
            textFile.close();
            return false;
        }
        numRows++;
    }
    const auto dataPtr = data.data();
    output = cv::Mat(numRows, numCols, CV_32F, dataPtr, cv::Mat::AUTO_STEP).clone();
    textFile.close();
    return true;
}

void verifyEqualImages(const cv::Mat & expected, const cv::Mat & actual)
{
    ASSERT_EQ(expected.size, actual.size) << "Images have different sizes";
    ASSERT_EQ(0, cv::countNonZero(cv::abs(expected - actual))) << "Images are not the same pixel by pixel";
}

void readConfigFromFile(const std::string & configFile, std::string & configString)
{
    const auto configFilePath = configFile.empty() ? resolvePath("libppp/share/config.bundle.json") : configFile;
    std::ifstream fs(configFilePath, std::ios_base::in);
    configString.assign((std::istreambuf_iterator<char>(fs)), std::istreambuf_iterator<char>());
}

void processDatabase(const DetectionCallback & callback,
                     const std::vector<std::string> & ignoredImages,
                     const std::string & landmarksPath,
                     std::vector<ResultData> & rd)
{
#ifdef _DEBUG
    auto annotateResults = true;
#else
    auto annotateResults = false;
#endif

    auto annotationFile = resolvePath(landmarksPath);
    std::map<std::string, LandMarks> landMarksSet;
    importLandMarks(annotationFile, landMarksSet);
    for (auto & annotatedImage : landMarksSet)
    {
        auto & imageFileName = annotatedImage.first;
        auto & annotations = annotatedImage.second;

        if (imageFileName.find("049_frontal") == std::string::npos)
        {
            // continue;
        }

        LandMarks results;
        if (find_if(ignoredImages.begin(),
                    ignoredImages.end(),
                    [&imageFileName](const std::string & ignoreImageFile) {
                        return imageFileName.find(ignoreImageFile) != std::string::npos;
                    })
            != ignoredImages.end())
        {
            continue; // Skip processing this image
        }

        auto inputImage = cv::imread(imageFileName);
        cv::Mat grayImage;
        cvtColor(inputImage, grayImage, cv::COLOR_BGR2GRAY);

        auto imagePrefix = imageFileName.substr(0, imageFileName.find_last_of('.'));
        auto annotatedLandMarkFiles = imagePrefix + ".pos";
        cv::Mat landMarksAnn;

        auto imageName = getFileName(imageFileName);

        auto isSuccess = callback(imageName, inputImage, grayImage, annotations, results);

        if (annotateResults)
        {
            cv::Scalar annnotationColor(0, 30, 255);
            cv::Scalar detectionColor(250, 30, 0);

            circle(inputImage, annotations.eyeLeftPupil, 5, annnotationColor, 2);
            circle(inputImage, annotations.eyeRightPupil, 5, annnotationColor, 2);
            circle(inputImage, annotations.lipLeftCorner, 5, annnotationColor, 2);
            circle(inputImage, annotations.lipRightCorner, 5, annnotationColor, 2);
            circle(inputImage, annotations.crownPoint, 5, annnotationColor, 2);
            circle(inputImage, annotations.chinPoint, 5, annnotationColor, 2);

            rectangle(inputImage, results.vjFaceRect, cv::Scalar(0, 128, 0), 2);
            rectangle(inputImage, results.vjLeftEyeRect, cv::Scalar(0xA0, 0x52, 0x2D), 3);
            rectangle(inputImage, results.vjRightEyeRect, cv::Scalar(0xA0, 0x52, 0x2D), 3);

            polylines(inputImage,
                      std::vector<std::vector<cv::Point>> { results.lipContour1st, results.lipContour2nd },
                      true,
                      detectionColor);
            rectangle(inputImage, results.vjMouthRect, cv::Scalar(0xA0, 0x52, 0x2D), 3);

            circle(inputImage, results.eyeLeftPupil, 5, detectionColor, 2);
            circle(inputImage, results.eyeRightPupil, 5, detectionColor, 2);

            circle(inputImage, results.lipLeftCorner, 5, detectionColor, 2);
            circle(inputImage, results.lipRightCorner, 5, detectionColor, 2);

            circle(inputImage, results.crownPoint, 5, detectionColor, 2);
            circle(inputImage, results.chinPoint, 5, detectionColor, 2);
        }

        rd.push_back(ResultData(imageFileName, annotations, results, isSuccess));
    }
}

void adjustCrownChinCoeffs(const std::vector<LandMarks> & groundTruthAnnotations)
{
    std::vector<double> c1, c2;
    for (const auto & lm : groundTruthAnnotations)
    {
        auto frown = (lm.eyeLeftPupil + lm.eyeRightPupil) / 2.0;
        auto mouthCenter = (lm.lipLeftCorner + lm.lipRightCorner) / 2.0;

        const auto refDist = norm(lm.eyeLeftPupil - lm.eyeRightPupil) + norm(frown - mouthCenter);

        const auto chinCrown = norm(lm.crownPoint - lm.chinPoint);
        const auto chinFrown = norm(frown - lm.chinPoint);

        c1.push_back(chinCrown / refDist);
        c2.push_back(chinFrown / refDist);
    }

    std::cout << "Chin-crown normalization: " << median(c1) << std::endl;
    std::cout << "Chin-frown normalization: " << median(c2) << std::endl;
}

TEST(Research, ModelCoefficientsCalculation)
{
    const auto annCsvFile = resolvePath("research/mugshot_frontal_original_all/via_region_data_dpd.csv");
    std::map<std::string, LandMarks> landMarksMap;
    importLandMarks(annCsvFile, landMarksMap);

    std::vector<LandMarks> annotations;
    for (const auto & kv : landMarksMap)
    {
        annotations.push_back(kv.second);
    }
    adjustCrownChinCoeffs(annotations);
}
