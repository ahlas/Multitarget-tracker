#include <fstream>
#include "YoloDetector.h"
#include "nms.h"

///
/// \brief YoloDetector::YoloDetector
/// \param gray
///
YoloOCVDetector::YoloOCVDetector(const cv::UMat& colorFrame)
    :
      BaseDetector(colorFrame)
{
    m_classNames = { "background",
                     "aeroplane", "bicycle", "bird", "boat",
                     "bottle", "bus", "car", "cat", "chair",
                     "cow", "diningtable", "dog", "horse",
                     "motorbike", "person", "pottedplant",
                     "sheep", "sofa", "train", "tvmonitor" };
}

///
/// \brief YoloDetector::~YoloDetector
///
YoloOCVDetector::~YoloOCVDetector(void)
{
}

///
/// \brief YoloDetector::Init
/// \return
///
bool YoloOCVDetector::Init(const config_t& config)
{
#if (((CV_VERSION_MAJOR == 4) && (CV_VERSION_MINOR >= 2)) || (CV_VERSION_MAJOR > 4))
	std::map<cv::dnn::Target, std::string> dictTargets;
	dictTargets[cv::dnn::DNN_TARGET_CPU] = "DNN_TARGET_CPU";
	dictTargets[cv::dnn::DNN_TARGET_OPENCL] = "DNN_TARGET_OPENCL";
	dictTargets[cv::dnn::DNN_TARGET_OPENCL_FP16] = "DNN_TARGET_OPENCL_FP16";
	dictTargets[cv::dnn::DNN_TARGET_MYRIAD] = "DNN_TARGET_MYRIAD";
	dictTargets[cv::dnn::DNN_TARGET_CUDA] = "DNN_TARGET_CUDA";
	dictTargets[cv::dnn::DNN_TARGET_CUDA_FP16] = "DNN_TARGET_CUDA_FP16";

	std::map<int, std::string> dictBackends;
	dictBackends[cv::dnn::DNN_BACKEND_DEFAULT] = "DNN_BACKEND_DEFAULT";
	dictBackends[cv::dnn::DNN_BACKEND_HALIDE] = "DNN_BACKEND_HALIDE";
	dictBackends[cv::dnn::DNN_BACKEND_INFERENCE_ENGINE] = "DNN_BACKEND_INFERENCE_ENGINE";
	dictBackends[cv::dnn::DNN_BACKEND_OPENCV] = "DNN_BACKEND_OPENCV";
	dictBackends[cv::dnn::DNN_BACKEND_VKCOM] = "DNN_BACKEND_VKCOM";
	dictBackends[cv::dnn::DNN_BACKEND_CUDA] = "DNN_BACKEND_CUDA";
	dictBackends[1000000] = "DNN_BACKEND_INFERENCE_ENGINE_NGRAPH";
	dictBackends[1000000 + 1] = "DNN_BACKEND_INFERENCE_ENGINE_NN_BUILDER_2019";

	std::cout << "Avaible pairs for Target - backend:" << std::endl;
	std::vector<std::pair<cv::dnn::Backend, cv::dnn::Target>> pairs = cv::dnn::getAvailableBackends();
	for (auto p : pairs)
	{
		std::cout << dictBackends[p.first] << " (" << p.first << ") - " << dictTargets[p.second] << " (" << p.second << ")" << std::endl;
	}
#endif

    auto modelConfiguration = config.find("modelConfiguration");
    auto modelBinary = config.find("modelBinary");
    if (modelConfiguration != config.end() && modelBinary != config.end())
    {
        m_net = cv::dnn::readNetFromDarknet(modelConfiguration->second, modelBinary->second);
    }

    auto dnnTarget = config.find("dnnTarget");
    if (dnnTarget != config.end())
    {
        std::map<std::string, cv::dnn::Target> targets;
        targets["DNN_TARGET_CPU"] = cv::dnn::DNN_TARGET_CPU;
        targets["DNN_TARGET_OPENCL"] = cv::dnn::DNN_TARGET_OPENCL;
#if (CV_VERSION_MAJOR >= 4)
        targets["DNN_TARGET_OPENCL_FP16"] = cv::dnn::DNN_TARGET_OPENCL_FP16;
        targets["DNN_TARGET_MYRIAD"] = cv::dnn::DNN_TARGET_MYRIAD;
#endif
#if (((CV_VERSION_MAJOR == 4) && (CV_VERSION_MINOR >= 2)) || (CV_VERSION_MAJOR > 4))
		targets["DNN_TARGET_CUDA"] = cv::dnn::DNN_TARGET_CUDA;
		targets["DNN_TARGET_CUDA_FP16"] = cv::dnn::DNN_TARGET_CUDA_FP16;
#endif
		std::cout << "Trying to set target " << dnnTarget->second << "... ";
        auto target = targets.find(dnnTarget->second);
        if (target != std::end(targets))
        {
			std::cout << "Succeded!" << std::endl;
            m_net.setPreferableTarget(target->second);
        }
		else
		{
			std::cout << "Failed" << std::endl;
		}
    }

#if (CV_VERSION_MAJOR >= 4)
    auto dnnBackend = config.find("dnnBackend");
    if (dnnBackend != config.end())
    {
        std::map<std::string, cv::dnn::Backend> backends;
        backends["DNN_BACKEND_DEFAULT"] = cv::dnn::DNN_BACKEND_DEFAULT;
        backends["DNN_BACKEND_HALIDE"] = cv::dnn::DNN_BACKEND_HALIDE;
        backends["DNN_BACKEND_INFERENCE_ENGINE"] = cv::dnn::DNN_BACKEND_INFERENCE_ENGINE;
        backends["DNN_BACKEND_OPENCV"] = cv::dnn::DNN_BACKEND_OPENCV;
        backends["DNN_BACKEND_VKCOM"] = cv::dnn::DNN_BACKEND_VKCOM;
#if (((CV_VERSION_MAJOR == 4) && (CV_VERSION_MINOR >= 2)) || (CV_VERSION_MAJOR > 4))
		backends["DNN_BACKEND_CUDA"] = cv::dnn::DNN_BACKEND_CUDA;
#endif
		std::cout << "Trying to set backend " << dnnBackend->second << "... ";
        auto backend = backends.find(dnnBackend->second);
        if (backend != std::end(backends))
        {
			std::cout << "Succeded!" << std::endl;
            m_net.setPreferableBackend(backend->second);
        }
		else
		{
			std::cout << "Failed" << std::endl;
		}
    }
#endif

    auto classNames = config.find("classNames");
    if (classNames != config.end())
    {
        std::ifstream classNamesFile(classNames->second);
        if (classNamesFile.is_open())
        {
            m_classNames.clear();
            std::string className;
            for (; std::getline(classNamesFile, className); )
            {
                m_classNames.push_back(className);
            }
        }
    }

    auto confidenceThreshold = config.find("confidenceThreshold");
    if (confidenceThreshold != config.end())
    {
        m_confidenceThreshold = std::stof(confidenceThreshold->second);
    }

    auto maxCropRatio = config.find("maxCropRatio");
    if (maxCropRatio != config.end())
    {
        m_maxCropRatio = std::stof(maxCropRatio->second);
        if (m_maxCropRatio < 1.f)
        {
            m_maxCropRatio = 1.f;
        }
    }

    return !m_net.empty();
}

///
/// \brief YoloDetector::Detect
/// \param gray
///
void YoloOCVDetector::Detect(const cv::UMat& colorFrame)
{
    m_regions.clear();

	std::vector<cv::Rect> crops = GetCrops(m_maxCropRatio, cv::Size(m_inWidth, m_inHeight), colorFrame.size());
	regions_t tmpRegions;
	for (size_t i = 0; i < crops.size(); ++i)
	{
		const auto& crop = crops[i];
		//std::cout << "Crop " << i << ": " << crop << std::endl;
		DetectInCrop(colorFrame, crop, tmpRegions);
	}

	if (crops.size() > 1)
	{
		nms3<CRegion>(tmpRegions, m_regions, 0.4f,
			[](const CRegion& reg) { return reg.m_brect; },
			[](const CRegion& reg) { return reg.m_confidence; },
			[](const CRegion& reg) { return reg.m_type; },
			0, 0.f);
		//std::cout << "nms for " << tmpRegions.size() << " objects - result " << m_regions.size() << std::endl;
	}
}

///
/// \brief YoloDetector::DetectInCrop
/// \param colorFrame
/// \param crop
/// \param tmpRegions
///
void YoloOCVDetector::DetectInCrop(const cv::UMat& colorFrame, const cv::Rect& crop, regions_t& tmpRegions)
{
    //Convert Mat to batch of images
    cv::dnn::blobFromImage(cv::UMat(colorFrame, crop), m_inputBlob, m_inScaleFactor, cv::Size(m_inWidth, m_inHeight), m_meanVal, false, true);

    m_net.setInput(m_inputBlob, "data"); //set the network input

#if (CV_VERSION_MAJOR < 4)
    cv::String outputName = "detection_out";
#else
    cv::String outputName = cv::String();
#endif
    cv::Mat detectionMat = m_net.forward(outputName); //compute output


    for (int i = 0; i < detectionMat.rows; ++i)
    {
        const int probability_index = 5;
        const int probability_size = detectionMat.cols - probability_index;
        float* prob_array_ptr = &detectionMat.at<float>(i, probability_index);

        size_t objectClass = std::max_element(prob_array_ptr, prob_array_ptr + probability_size) - prob_array_ptr;
        float confidence = detectionMat.at<float>(i, (int)objectClass + probability_index);

        if (confidence > m_confidenceThreshold)
        {
            float x_center = detectionMat.at<float>(i, 0) * crop.width + crop.x;
            float y_center = detectionMat.at<float>(i, 1) * crop.height + crop.y;
            float width = detectionMat.at<float>(i, 2) * crop.width;
            float height = detectionMat.at<float>(i, 3) * crop.height;
            cv::Point p1(cvRound(x_center - width / 2), cvRound(y_center - height / 2));
            cv::Point p2(cvRound(x_center + width / 2), cvRound(y_center + height / 2));
            cv::Rect object(p1, p2);

            tmpRegions.emplace_back(object, (objectClass < m_classNames.size()) ? m_classNames[objectClass] : "", confidence);
        }
    }
}
