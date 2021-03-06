#include "libppp.h"
#include "CanvasDefinition.h"
#include "CommonHelpers.h"
#include "LandMarks.h"
#include "PhotoStandard.h"
#include "PppEngine.h"
#include "Utilities.h"

#include <regex>

#include <opencv2/imgcodecs.hpp>

#ifdef EMSCRIPTEN
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

using namespace std;

PublicPppEngine g_c_pppInstance;
string g_last_error;

cv::Point fromJson(rapidjson::Value & v)
{
    return cv::Point(v["x"].GetInt(), v["y"].GetInt());
}

PublicPppEngine::PublicPppEngine()
: m_pPppEngine(new PppEngine)
{
}

PublicPppEngine::~PublicPppEngine()
{
    delete m_pPppEngine;
}

bool PublicPppEngine::configure(const char * jsonConfig) const
{

    return m_pPppEngine->configure(jsonConfig);
}

std::string PublicPppEngine::setImage(const char * bufferData, size_t bufferLength) const
{
    cv::Mat inputImage;
    if (bufferLength <= 0)
    {
        // Find out if this is a data url
        auto offset = 0;
        auto dataLen = strlen(bufferData);

        regex re("^data:([a-z]+\\/[a-z]+(;[a-z\\-]+\\=[a-z\\-]+)?)?(;base64)?,");
        std::cmatch cm; // same as std::match_results<const char*> cm;
        if (std::regex_search(bufferData, cm, re))
        {
            offset = cm[0].length();
            dataLen -= offset;
        }

        auto decodedBytes = Utilities::base64Decode(bufferData + offset, dataLen);
        const cv::_InputArray inputArray(decodedBytes.data(), static_cast<int>(decodedBytes.size()));
        inputImage = imdecode(inputArray, cv::IMREAD_COLOR);
    }
    else
    {
        const cv::_InputArray inputArray(bufferData, static_cast<int>(bufferLength));
        inputImage = imdecode(inputArray, cv::IMREAD_COLOR);
    }
    return m_pPppEngine->setInputImage(inputImage);
}

std::string PublicPppEngine::detectLandmarks(const std::string & imageId) const
{
    LandMarks landMarks;
    m_pPppEngine->detectLandMarks(imageId, landMarks);
    return landMarks.toJson();
}

std::string PublicPppEngine::createTiledPrint(const std::string & imageId, const std::string & request) const
{
    rapidjson::Document d;
    d.Parse(request.c_str());

    const auto ps = PhotoStandard::fromJson(d["standard"]);
    const auto canvas = CanvasDefinition::fromJson(d["canvas"]);
    auto cronwPoint = fromJson(d["crownPoint"]);
    auto chinPoint = fromJson(d["chinPoint"]);
    auto asBase64Encode = false;

    if (d.HasMember("asBase64"))
    {
        asBase64Encode = d["asBase64"].GetBool();
    }

    const auto result = m_pPppEngine->createTiledPrint(imageId, *ps, *canvas, cronwPoint, chinPoint);

    std::vector<BYTE> pictureData;
    imencode(".png", result, pictureData);

    // Add image resolution to output
    setPngResolutionDpi(pictureData, canvas->resolution_ppmm());

    if (asBase64Encode)
    {
        return Utilities::base64Encode(pictureData);
    }
    return std::string(pictureData.begin(), pictureData.end());
}

template <typename T>
std::vector<BYTE> toBytes(const T & x)
{
    vector<BYTE> v(static_cast<const BYTE *>(static_cast<const void *>(&x)),
                   static_cast<const BYTE *>(static_cast<const void *>(&x)) + sizeof(x));
    reverse(v.begin(), v.end()); // Little endian notation
    return v;
}

/*  The pHYs chunk specifies the intended pixel size or aspect ratio for display of the image. It contains:
    Pixels per unit, X axis: 4 bytes (unsigned integer)
    Pixels per unit, Y axis: 4 bytes (unsigned integer)
    Unit specifier:          1 byte
The following values are defined for the unit specifier:
    0: unit is unknown
    1: unit is the meter
pHYs has to go before IDAT chunk
*/
void PublicPppEngine::setPngResolutionDpi(std::vector<BYTE> & imageStream, double resolution_ppmm)
{
    const auto chunkLenBytes = toBytes(9);
    auto resolBytes = toBytes(ROUND_INT(resolution_ppmm * 1000));
    const string physStr = "pHYs";

    auto pHYsChunk(chunkLenBytes);
    pHYsChunk.insert(pHYsChunk.end(), physStr.begin(), physStr.end());

    pHYsChunk.insert(pHYsChunk.end(), resolBytes.begin(), resolBytes.end());
    pHYsChunk.insert(pHYsChunk.end(), resolBytes.begin(), resolBytes.end());
    pHYsChunk.push_back(1); // Unit is the meter

    auto crcBytes = toBytes(Utilities::crc32(0, &pHYsChunk[4], &pHYsChunk[4] + pHYsChunk.size() - 4));
    pHYsChunk.insert(pHYsChunk.end(), crcBytes.begin(), crcBytes.end());

    string idat = "IDAT";
    const auto it = search(imageStream.begin(), imageStream.end(), idat.begin(), idat.end());
    if (it != imageStream.end())
    {
        // Insert the chunk in the stream
        imageStream.insert(it - 4, pHYsChunk.begin(), pHYsChunk.end());
    }
}

#pragma region C Interface
#define TRYRUN(statements)                                                                                             \
    try                                                                                                                \
    {                                                                                                                  \
        statements;                                                                                                    \
        std::cout << "Method '" << __FUNCTION__ << "' called successfully" << std::endl;                               \
        return true;                                                                                                   \
    }                                                                                                                  \
    catch (const std::exception & ex)                                                                                  \
    {                                                                                                                  \
        std::cout << "Method '" << __FUNCTION__ << "' failed: " << ex.what() << std::endl;                             \
        g_last_error = ex.what();                                                                                      \
        return false;                                                                                                  \
    }

EMSCRIPTEN_KEEPALIVE
bool set_image(const char * img_buf, int img_buf_size, char * img_id)
{
    TRYRUN(auto imgId = g_c_pppInstance.setImage(img_buf, img_buf_size); strcpy(img_id, imgId.c_str()););
}

EMSCRIPTEN_KEEPALIVE
bool configure(const char * config_json)
{
    TRYRUN(g_c_pppInstance.configure(config_json););
}

EMSCRIPTEN_KEEPALIVE
bool detect_landmarks(const char * img_id, char * landmarks)
{
    TRYRUN(auto landmarksStr = g_c_pppInstance.detectLandmarks(img_id); strcpy(landmarks, landmarksStr.c_str()););
}

EMSCRIPTEN_KEEPALIVE
int create_tiled_print(const char * img_id, const char * request, char * out_buf)
{
    try
    {
        auto output = g_c_pppInstance.createTiledPrint(img_id, request);
        const auto out_size = static_cast<int>(output.size());
        copy(output.begin(), output.end(), out_buf);
        return out_size;
    }
    catch (const std::exception & ex)
    {
        g_last_error = ex.what();
        return 0;
    }
}

#pragma endregion
