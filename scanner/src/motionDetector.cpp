//
// Created by a on 6/7/2021.
//

#include "motionDetector.h"


/** \brief Constructor; sets the required initial parameter(s)
*
* \param [in]   hva_    Camera horizontal view angle
*/
MotionDetector::MotionDetector(float hva_)
{
    hva = hva_;
}

/** \brief Calculates and sets the camera focal length
*
* \param [out]  w   Image width in pixels
*
* This function sets the focal length (f) simply assuming that the camera focal point is exactly placed
* in front of image center. Usually this assumption is not true. However the little error is negligible
*/
void MotionDetector::setFocalLength(int w)
{
    fl = (float) (0.5 * w * (1.0 / tan((hva/2.0)*PI/180)));
    focalLengthSet = true;
}

/** \brief The main function which detects the moving objects within image
*
* \param [in]   imgSt   The ImageSet structure instance containing camera image in which moving objects
*                       must be detected, along with corresponding GPS location
* \param [out]  output  A gray image with detected moving objects highlighted with respect to their speed
* \param [out]  objects A list of Object instances each including obtained information about a moving object
* \param [in]   fov     A list of four Object instances each including a GPS location corresponding to one
* 				        of the camera view corners
*
* This function can be called whenever the camera image is available AND camera is in a fixed position.
* It is called with an image synchronized with GPS and IMU data previously. Besides, the camera FOV points
* must be mapped into the online map. The visual motion detection method is based on dense optical flow
*/
void MotionDetector::detect(ImageSet &imgSt, cv::Mat &output, std::vector<Object> &objects, const std::vector<Object> &fov)
{
    if (old_frame.empty())
    {
        cv::cvtColor(imgSt.image, old_frame, cv::COLOR_RGB2GRAY);
        output = cv::Mat::zeros(imgSt.image.rows, imgSt.image.cols, CV_8UC3);

        if (!focalLengthSet){
            setFocalLength(imgSt.image.rows);
        }

        return;
    }

    cv::Mat frame = imgSt.image.clone();

    cv::Mat new_frame, flow(old_frame.size(), CV_32FC2);
    cv::cvtColor(frame, new_frame, cv::COLOR_BGR2GRAY);

    cv::calcOpticalFlowFarneback(old_frame, new_frame, flow,  0.5, 3, 15, 3, 5, 1.2, 0);
    cv::cvtColor(frame, old_frame, cv::COLOR_RGB2GRAY);

    cv::Mat xNormalizationCoeff(old_frame.size(), CV_64FC1), yNormalizationCoeff(old_frame.size(), CV_64FC1);
    // TODO: Using aircraft velocity data, this function must be called once every time the camera is fixed to detect motions, not real-time!
    calcNormCoeffMat(fov, imgSt.lat, imgSt.lng, imgSt.alt, xNormalizationCoeff, yNormalizationCoeff);

    visualize(flow, xNormalizationCoeff, yNormalizationCoeff, output);

    generateMovingRects(imgSt.image, output, objects);
}

/** \brief Visualizes the metric speeds in a gray image
*
* \param [in]   flow                An two-channel image with each pixel representative for horizontal or
*                                   vertical speed within image
* \param [in]   xNormalizationCoeff The matrix with each pixel containing the proper coefficient to transform
*                                   the horizontal pixel speed into a metric speed in the same direction
* \param [in]   yNormalizationCoeff The matrix with each pixel containing the proper coefficient to transform
*                                   the vertical pixel speed into a metric speed in the same direction
* \param [in]   output              The output image with visualized moving objects
*
* This function is called with two generated normalization coefficient matrices for both horizontal and
* vertical directions. In the output image, the brighter a pixel is, the faster the corresponding point moves
*/
void MotionDetector::visualize(const cv::Mat &flow, const cv::Mat &xNormalizationCoeff, const cv::Mat &yNormalizationCoeff, cv::Mat &output)
{
    cv::Mat flow_parts[2], flow_parts_d[2], /*magnitude,*/ angle;
    cv::split(flow, flow_parts);
    flow_parts[0].convertTo(flow_parts_d[0], CV_64F);
    flow_parts[1].convertTo(flow_parts_d[1], CV_64F);

    cv::Mat metricFlowX(old_frame.size(), CV_64FC1), metricFlowY(old_frame.size(), CV_64FC1);

    metricFlowX = flow_parts_d[0].mul(xNormalizationCoeff);
    metricFlowY = flow_parts_d[1].mul(yNormalizationCoeff);

    cv::cartToPolar(metricFlowX, metricFlowY, output, angle, true);

    metricNormalize(output);
}

/** \brief Calculates the the required coefficient to convert pixel speed into metric speed for each pixel
*
* \param [in]   fov                     A list of four "Object" structure instances each including a GPS
* 				                        location corresponding to one of the camera view corners
* \param [in]   lat                     Camera location latitude at the moment in which image is captured
* \param [in]   lng                     Camera location longitude at the moment in which image is captured
* \param [in]   alt                     Camera location altitude at the moment in which image is captured
* \param [out]  xNormalizationCoeff     The matrix with each pixel containing the proper coefficient to transform
*                                       the horizontal pixel speed into a metric speed in the same direction
* \param [out]  yNormalizationCoeff     The matrix with each pixel containing the proper coefficient to transform
*                                       the vertical pixel speed into a metric speed in the same direction
*
* This function is called when the corresponding location for each camera FOV point is determined. It
* calculates the normalization coefficient matrix, the matrix in which each pixel contains the required
* value to multiply by the corresponding pixel speed, thus providing the metric speed of that point
*/
void MotionDetector::calcNormCoeffMat(const std::vector<Object> &fov, double lat, double lng, double alt, cv::Mat &xNormalizationCoeff, cv::Mat &yNormalizationCoeff)
{
    double x, y;
    LatLonToUTMXY(lat, lng, 0, x, y);

    double v1_1 = fov[0].location.x-fov[1].location.x, v1_2 = fov[0].location.y-fov[1].location.y, v2_1 = fov[2].location.x-fov[1].location.x, v2_2 = fov[2].location.y-fov[1].location.y;
    double v1dotv2 = v1_1*v2_1 + v1_2*v2_2;
    double alpha = (PI/2) - acos(v1dotv2/(sqrt(pow(v1_1,2)+pow(v1_2,2))*sqrt(pow(v2_1,2)+pow(v2_2,2))));
    int rows = old_frame.rows, cols = old_frame.cols;
    for (int i=0; i<rows; i++) {
        double rowFirstX = fov[0].location.x + i*(fov[3].location.x - fov[0].location.x)/rows;
        double rowFirstY = fov[0].location.y + i*(fov[3].location.y - fov[0].location.y)/rows;
        double rowLastX = fov[1].location.x + i*(fov[2].location.x - fov[1].location.x)/rows;
        double rowLastY = fov[1].location.y + i*(fov[2].location.y - fov[1].location.y)/rows;
        for (int j=0; j<cols; j++) {
            double X = rowFirstX + j*(rowLastX - rowFirstX)/cols;
            double Y = rowFirstY + j*(rowLastY - rowFirstY)/cols;
            double h = sqrt(pow(((double)j-((double)cols/2)),2) + pow(((double)i-((double)rows/2)),2) + pow(fl,2));
            double xCoeff = sqrt(pow(x-X, 1)+pow(y-Y, 2)+pow(alt,2))/h;
            xNormalizationCoeff.at<double>(i,j) = xCoeff;
            yNormalizationCoeff.at<double>(i,j) = xCoeff/cos(((double)j/((double)cols/2))*alpha);
        }
    }
}

/** \brief Extracts a list of Object instances from an image of moving objects and highlights each object
*
* \param [in]   input       The input image with visualized moving objects
* \param [out]  output      The output image with highlighted moving objects within
* \param [out]  objects     A list containing data for each detected moving object
*
* This function is called when the gray image of moving objects is generated.
*/
void MotionDetector::generateMovingRects(cv::Mat &input, cv::Mat &output, std::vector<Object> &objects)
{
    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;
    Mat otpt;

    cv::threshold(output, otpt, minimumDetectionSpeed, 255, THRESH_BINARY);
    output.convertTo(output, CV_8U);
    otpt.convertTo(otpt, CV_8U);
    findContours(otpt, contours, hierarchy, cv::RETR_LIST, cv::CHAIN_APPROX_TC89_KCOS, cv::Point(0, 0) );

    objects.clear();
    double area, areaRatio, imArea = output.rows * output.cols;
    for (auto & contour : contours) {
        area = cv::contourArea(contour);
        areaRatio = area / imArea;
        if (areaRatio < objectSizeUpLimit && areaRatio > objectSizeLowLimit)
        {
            Object obj;
            obj.box = cv::boundingRect(contour);
            obj.picture = input(obj.box);
            obj.type = Object::MOVING;
            objects.push_back(obj);
        }
    }
}

/** \brief Generates a speed map from a metric speed image
*
* \param [in]   in  The input image with metric speed of the corresponding point
*
* This function is called when an image with metric speed in each pixel is generated. It changes the picture
* in such a way that the fully white pixels (value: 255) indicate points with predetermined maximum object
* speed or more. The fully black pixels (value: 0) indicate points with zero speed
*/
void MotionDetector::metricNormalize(Mat &in)
{
    int rows = in.rows, cols = in.cols;

    for (int i=0; i<rows; i++) {
        for (int j=0; j<cols; j++) {
            if (in.at<double>(i,j) > objMaxSpeed)
                in.at<double>(i,j) = objMaxSpeed;
        }
    }
//    cv::normalize(in, in, 0, 255, NORM_MINMAX, CV_8UC1);
    in = (255.0/objMaxSpeed) * in;
}

