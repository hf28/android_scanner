
#include "detector.h"

Detector::Detector(std::string assetsDir, DetectionMethod dm, float conf, float nms)
{

    this->detectionMethod = dm;

    if (dm == DetectionMethod::YOLO_V3)
    {
        std::string model = "../../YOLOV3/cfg/yolo_v3.cfg";
        std::string config = "../../YOLOV3/weight/yolov3.weights";

        this->net = cv::dnn::readNetFromDarknet(model, config);
        this->net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        this->net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    }
    else if (dm == DetectionMethod::YOLO_TINY)
    {
        std::string model = "../../YOLOV3/cfg/yolov3-tiny.cfg";
        std::string config = "../../YOLOV3/weight/yolov3-tiny.weights";

        this->net = cv::dnn::readNetFromDarknet(model, config);
        this->net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        this->net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    }
    else if (dm == DetectionMethod::MN_SSD)
    {
        std::string model = assetsDir + "/MobileNetSSD_deploy.prototxt.txt";
        std::string config = assetsDir + "/MobileNetSSD_deploy.caffemodel";

        this->net = cv::dnn::readNetFromCaffe(model, config);
    }

    this->confidence = conf;
    this->nmsThreshold = nms;

}

void Detector::detect(cv::Mat &frame, std::vector<cv::Rect> &bboxes)
{
    cv::Mat blob;

    if (this->detectionMethod == YOLO_V3 || this->detectionMethod == YOLO_TINY)
    {
        cv::dnn::blobFromImage(frame, blob, 1/255.0, cv::Size(416, 416), cv::Scalar(0,0,0), true, false);
        this->net.setInput(blob);
        std::vector<cv::Mat> outs;
        this->net.forward(outs);
        yolov3PostProcess(frame, outs, bboxes);
    }
    else if (this->detectionMethod == MN_SSD)
    {
        cv::dnn::blobFromImage(frame, blob, 0.007843, cv::Size(300, 300), cv::Scalar(127.5, 127.5, 127.5), false);
        this->net.setInput(blob);
        cv::Mat prob = this->net.forward();
        ssdPostProcess(frame, prob, bboxes);
    }

}

void Detector::yolov3PostProcess(cv::Mat& frame, const std::vector<cv::Mat>& outs, std::vector<cv::Rect> &bboxes)
{
    std::vector<float> confidences;
    std::vector<cv::Rect> boxes;

    for (size_t i = 0; i < outs.size(); ++i)
    {
        float* data = (float*)outs[i].data;
        for (int j = 0; j < outs[i].rows; ++j, data += outs[i].cols)
        {
            cv::Mat scores = outs[i].row(j).colRange(5, outs[i].cols);
            cv::Point classIdPoint;
            double cnf;

            cv::minMaxLoc(scores, 0, &cnf, 0, &classIdPoint);
            if (cnf > this->confidence && classIdPoint.x == 0)
            {
                int centerX = (int)(data[0] * frame.cols);
                int centerY = (int)(data[1] * frame.rows);
                int width = (int)(data[2] * frame.cols);
                int height = (int)(data[3] * frame.rows);
                int left = centerX - width / 2;
                int top = centerY - height / 2;

                confidences.push_back((float)confidence);
                bboxes.push_back(cv::Rect(left, top, width, height));
            }
        }
    }

    // std::vector<int> indices;
    // cv::dnn::NMSBoxes(boxes, confidences, this->confidence, this->nmsThreshold, indices);
    // for (size_t i = 0; i < indices.size(); ++i)
    // {
    // 	int idx = indices[i];
    // 	cv::Rect box = boxes[idx];
    // 	bboxes.push_back(box);
    // }
}

void Detector::ssdPostProcess(cv::Mat& frame, cv::Mat &outs, std::vector<cv::Rect> &bboxes)
{
    cv::Mat detectionMat(outs.size[2], outs.size[3], CV_32F, outs.ptr<float>());

    for (int i = 0; i < detectionMat.rows; i++)
    {
        int idx = static_cast<int>(detectionMat.at<float>(i, 1));
        float cnf = detectionMat.at<float>(i, 2);

        if (cnf > this->confidence && idx == 15)
        {
            int xLeftBottom = static_cast<int>(detectionMat.at<float>(i, 3) * frame.cols);
            int yLeftBottom = static_cast<int>(detectionMat.at<float>(i, 4) * frame.rows);
            int xRightTop = static_cast<int>(detectionMat.at<float>(i, 5) * frame.cols);
            int yRightTop = static_cast<int>(detectionMat.at<float>(i, 6) * frame.rows);

            cv::Rect object((int)xLeftBottom, (int)yLeftBottom,
                            (int)(xRightTop - xLeftBottom),
                            (int)(yRightTop - yLeftBottom));

            bboxes.push_back(object);
        }
    }
}

void Detector::drawDetections(cv::Mat &dst, std::vector<cv::Rect> &bboxs){

    for(unsigned i=0; i<bboxs.size(); i++)
    {
        rectangle(dst, bboxs[i], cv::Scalar(0,0,255), 2, 1);
    }

}