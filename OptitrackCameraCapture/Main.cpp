#include <opencv2/opencv.hpp>
#include <d3d11.h>
#include <thread>
#include <mutex>
#include <ctime>
#include "cameralibrary.h"
#include "IUnityInterface.h"
#include "IUnityGraphics.h"
#include "IUnityGraphicsD3D11.h"

class OptiCamera
{
public:
	OptiCamera(IUnityInterfaces* unity) : unity_(unity)
	{
		CameraLibrary_EnableDevelopment();
		CameraLibrary::CameraManager::X().WaitForInitialization();
		camera_ = CameraLibrary::CameraManager::X().GetCamera();

		try
		{
			isConnected_ = !(camera_->IsDisconnected());
			StartCapture();
		}
		catch (...)
		{
			isConnected_ = false;
			return;
		}
	}

	int GetWidth() const
	{
		if (camera_ == 0)
		{
			return 0;
		}
		else
		{
			return camera_->PhysicalPixelWidth();
		}
	}

	int GetHeight() const
	{
		if (camera_ == 0)
		{
			return 0;
		}
		else
		{
			return camera_->PhysicalPixelHeight();
		}
	}

	bool IsConnected()
	{
		return isConnected_;
	}

	void SetTexturePtr(void* ptr)
	{
		texture_ = static_cast<ID3D11Texture2D*>(ptr);
	}

	void SetExposure(int exposure)
	{
		camera_->SetExposure(exposure);
	}

	void SetIntensity(int intensity)
	{
		camera_->SetIntensity(intensity);
	}

	void SetGainLevel(int gain)
	{
		if (gain < 0 || gain >= 8)
		{
			return;
		}
		auto imagerGain = (CameraLibrary::eImagerGain)gain;
		camera_->SetImagerGain(imagerGain);
	}

	void Update()
	{
		if (unity_ == nullptr || texture_ == nullptr || image_.empty()) return;

		std::lock_guard<std::mutex> lock(mutex_);
		auto device = unity_->Get<IUnityGraphicsD3D11>()->GetDevice();
		ID3D11DeviceContext* context;
		device->GetImmediateContext(&context);

		if (showOriginal_)
		{
			cv::cvtColor(image_, exportImage_, CV_RGB2RGBA);
		}
		else
		{
			if (backgroundImage_.empty())
			{
				backgroundImage_ = image_;
			}
			cv::absdiff(backgroundImage_, image_, diffImage_);
			cv::bitwise_not(diffImage_, invertedImage_);
			cv::cvtColor(invertedImage_, exportImage_, CV_RGB2RGBA);
		}
		
		context->UpdateSubresource(texture_, 0, nullptr, exportImage_.data, 4 * exportImage_.cols, 4 * exportImage_.cols * exportImage_.rows);
	}

	std::string GetTimeStamp()
	{
		std::time_t rawTime;
		std::tm* timeInfo;
		std::time(&rawTime);
		timeInfo = std::localtime(&rawTime);

		char buffer[80];
		std::strftime(buffer, 80, "%Y-%m-%d-%H-%M-%S", timeInfo);
		std::string timeStamp(buffer);
		return timeStamp;
	}

	void SaveImage(int frameNumber)
	{
		std::string timeStamp = GetTimeStamp();
		std::string frameCount = std::to_string(frameNumber);

		if (showOriginal_)
		{
			cv::imwrite("CapturedImages/" + timeStamp + "_" + frameCount + ".jpg", image_);
		}
		else
		{
			cv::imwrite("CapturedImages/" + timeStamp + "_" + frameCount + ".jpg", invertedImage_);
		}
	}

	void SaveOriginalImage(int frameNumber)
	{
		std::string timeStamp = GetTimeStamp();
		std::string frameCount = std::to_string(frameNumber);
		cv::imwrite("CapturedImages/" + timeStamp + "_" + frameCount + ".jpg", image_);
	}

	void SaveSubtractedImage(int frameNumber)
	{
		std::string timeStamp = GetTimeStamp();
		std::string frameCount = std::to_string(frameNumber);
		cv::imwrite("CapturedImages/" + timeStamp + "_" + frameCount + ".jpg", invertedImage_);
	}

	void RecordBackground()
	{

		backgroundImage_ = image_.clone();
	}

	void ToggleImage()
	{
		showOriginal_ = !showOriginal_;
	}

	void StopCapture()
	{
		isRunning_ = false;
		if (thread_.joinable())
		{
			thread_.join();
		}
		camera_->Release();
	}

private:
	void StartCapture()
	{
		image_ = cv::Mat(camera_->PhysicalPixelHeight(), camera_->PhysicalPixelWidth(), CV_8UC3);
		camera_->SetVideoType(Core::MJPEGMode);
		camera_->SetExposure(1000);
		camera_->SetImagerGain(CameraLibrary::eImagerGain::Gain_Level0);
		camera_->SetThreshold(40);
		camera_->SetIntensity(15);
		camera_->SetShutterDelay(0);
		camera_->SetAEC(false);
		camera_->SetAGC(false);
		camera_->SetIRFilter(false);
		camera_->Start();

		thread_ = std::thread([this]
		{
			isRunning_ = true;

			while (isRunning_ && camera_->IsCameraRunning())
			{
				std::lock_guard<std::mutex> lock(mutex_);
				frame_ = camera_->GetFrame();
				if (frame_)
				{
					frame_->Rasterize(camera_->PhysicalPixelWidth(), camera_->PhysicalPixelHeight(), image_.step, 24, image_.data);
					frame_->Release();
				}
			}
		});
	}

	CameraLibrary::Camera *camera_;
	cv::Mat image_;
	cv::Mat backgroundImage_;
	cv::Mat diffImage_;
	cv::Mat invertedImage_;
	cv::Mat exportImage_;

	IUnityInterfaces* unity_;
	ID3D11Texture2D* texture_ = nullptr;
	CameraLibrary::Frame *frame_ = nullptr;

	std::thread thread_;
	std::mutex mutex_;
	bool isRunning_ = false;
	bool isConnected_ = false;
	bool showOriginal_ = false;
};

namespace
{
	IUnityInterfaces* g_unity = nullptr;
	OptiCamera* g_camera = nullptr;
}

extern "C"
{

	UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
	{
		g_unity = unityInterfaces;
	}

	UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginUnload()
	{
		// Do nothing
	}

	void UNITY_INTERFACE_API OnRenderEvent(int eventId)
	{
		if (g_camera)
			g_camera->Update();
	}

	UNITY_INTERFACE_EXPORT UnityRenderingEvent UNITY_INTERFACE_API GetRenderEventFunc()
	{
		return OnRenderEvent;
	}

	UNITY_INTERFACE_EXPORT void* UNITY_INTERFACE_API GetCamera()
	{
		g_camera = new OptiCamera(g_unity);
		return g_camera;
	}

	UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API ReleaseCamera(void* ptr)
	{
		auto camera = reinterpret_cast<OptiCamera*>(ptr);
		if (g_camera == camera)
		{
			g_camera->StopCapture();
			g_camera = nullptr;
		}
		delete g_camera;
	}

	UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API GetCameraWidth(void* ptr)
	{
		auto camera = reinterpret_cast<OptiCamera*>(ptr);
		return camera->GetWidth();
	}

	UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API GetCameraHeight(void* ptr)
	{
		auto camera = reinterpret_cast<OptiCamera*>(ptr);
		return camera->GetHeight();
	}

	UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API IsCameraConnected(void* ptr)
	{
		auto camera = reinterpret_cast<OptiCamera*>(ptr);
		return camera->IsConnected();
	}

	UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API SetCameraTexturePtr(void* ptr, void* texture)
	{
		auto camera = reinterpret_cast<OptiCamera*>(ptr);
		camera->SetTexturePtr(texture);
	}

	UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API SetCameraExposure(void* ptr, int exposure)
	{
		auto camera = reinterpret_cast<OptiCamera*>(ptr);
		camera->SetExposure(exposure);
	}

	UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API SetCameraIntensity(void* ptr, int intensity)
	{
		auto camera = reinterpret_cast<OptiCamera*>(ptr);
		camera->SetIntensity(intensity);
	}

	UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API SetCameraGainLevel(void* ptr, int gain)
	{
		auto camera = reinterpret_cast<OptiCamera*>(ptr);
		camera->SetGainLevel(gain);
	}

	UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API SaveImage(void* ptr, int frameNumber)
	{
		auto camera = reinterpret_cast<OptiCamera*>(ptr);
		camera->SaveImage(frameNumber);
	}

	UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API SaveOriginalImage(void* ptr, int frameNumber)
	{
		auto camera = reinterpret_cast<OptiCamera*>(ptr);
		camera->SaveOriginalImage(frameNumber);
	}

	UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API SaveSubtractedImage(void* ptr, int frameNumber)
	{
		auto camera = reinterpret_cast<OptiCamera*>(ptr);
		camera->SaveSubtractedImage(frameNumber);
	}

	UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API RecordBackground(void* ptr)
	{
		auto camera = reinterpret_cast<OptiCamera*>(ptr);
		camera->RecordBackground();
	}

	UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API ToggleImage(void* ptr)
	{
		auto camera = reinterpret_cast<OptiCamera*>(ptr);
		camera->ToggleImage();
	}
}