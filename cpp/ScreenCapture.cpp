#include <d3d11.h>
#include <dxgi1_2.h>
#include <vector>
#include <iostream>
#include <opencv2/opencv.hpp>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "opencv_world4100.lib")

#include "Types.h"
#include "Timer.cpp"
#include "Profiler.cpp"

// Structure to hold RGBA data
struct rgb {
    unsigned char R;
    unsigned char G;
    unsigned char B;
};

struct directx_capture_context
{
    ID3D11Device* Device;
    ID3D11DeviceContext* DeviceContext;
    IDXGIOutputDuplication* DesktopDup;
    
    HWND Window;
    int Width;
    int Height;
    rgb* Pixels;
};

struct inference_detection
{
    float Confidence;
    float X, Y, W, H; //X and Y are the middle of the rectangle
};

struct PCMouseUpdate
{
    int32_t dX;
    int32_t dY;
    uint32_t Click;
};

static directx_capture_context
CreateDirectXContext()
{
    directx_capture_context Context = {};
    
    // Initialize D3D11 device and context
    D3D_FEATURE_LEVEL FeatureLevel;
    D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_DEBUG, nullptr, 0, D3D11_SDK_VERSION, &Context.Device, &FeatureLevel, &Context.DeviceContext);
    
    // Get DXGI device and adapter
    IDXGIDevice* DXGIDevice;
    Context.Device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&DXGIDevice));
    
    IDXGIAdapter* DXGIAdapter;
    DXGIDevice->GetAdapter(&DXGIAdapter);
    
    // Get DXGI output (monitor)
    IDXGIOutput* DXGIOutput;
    DXGIAdapter->EnumOutputs(0, &DXGIOutput);
    
    IDXGIOutput1* DXGIOutput1;
    DXGIOutput->QueryInterface(__uuidof(IDXGIOutput1), reinterpret_cast<void**>(&DXGIOutput1));
    
    // Create desktop duplication
    HRESULT Result = DXGIOutput1->DuplicateOutput(Context.Device, &Context.DesktopDup);
    
    Sleep(3000);
    
    Context.Window = GetForegroundWindow();
    
    RECT WindowRect = {};
    GetClientRect(Context.Window, &WindowRect);
    
    Context.Width = (WindowRect.right - WindowRect.left) / 4 * 4;
    Context.Height = (WindowRect.bottom - WindowRect.top) / 4 * 4;
    
    Context.Pixels = (rgb*)VirtualAlloc(0, Context.Width * Context.Height * sizeof(rgb), MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
    
    return Context;
}

static void
GetFrame(directx_capture_context* Context)
{
    TimeFunction;
    
    POINT TopLeftOfWindow = {0,0};
    ClientToScreen(Context->Window, &TopLeftOfWindow);
    
    if (TopLeftOfWindow.x < 0)
    {
        TopLeftOfWindow.x = 0;
    }
    if (TopLeftOfWindow.y < 0)
    {
        TopLeftOfWindow.y = 0;
    }
    
    // Acquire next frame
    DXGI_OUTDUPL_FRAME_INFO FrameInfo = {};
    IDXGIResource* DesktopResource = 0;
    HRESULT AcquireResult = Context->DesktopDup->AcquireNextFrame(INFINITE, &FrameInfo, &DesktopResource);
    
    // Query for ID3D11Texture2D
    ID3D11Texture2D* AcquiredDesktopImage;
    DesktopResource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&AcquiredDesktopImage));
    
    // Get texture description
    D3D11_TEXTURE2D_DESC Desc;
    AcquiredDesktopImage->GetDesc(&Desc);
    UINT Width = Desc.Width;
    UINT Height = Desc.Height;
    
    // Modify texture description for staging texture
    D3D11_TEXTURE2D_DESC StagingDesc = {};
    StagingDesc.Width = Width;
    StagingDesc.Height = Height;
    StagingDesc.MipLevels = 1;
    StagingDesc.ArraySize = 1;
    StagingDesc.Format = Desc.Format;  // Use the same format as the source texture
    StagingDesc.SampleDesc.Count = 1;
    StagingDesc.SampleDesc.Quality = 0;
    StagingDesc.Usage = D3D11_USAGE_STAGING;
    StagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    StagingDesc.BindFlags = 0;
    StagingDesc.MiscFlags = 0;
    
    ID3D11Texture2D* StagingTexture;
    HRESULT Result = Context->Device->CreateTexture2D(&StagingDesc, nullptr, &StagingTexture);
    
    // Copy acquired frame to staging texture
    Context->DeviceContext->CopyResource(StagingTexture, AcquiredDesktopImage);
    
    // Map the staging texture for CPU read
    D3D11_MAPPED_SUBRESOURCE Mapped;
    Result = Context->DeviceContext->Map(StagingTexture, 0, D3D11_MAP_READ, 0, &Mapped);
    
    {
        TimeBlock("Copy from GPU");
        
        int XCount = MIN(Context->Width, Width - TopLeftOfWindow.x);
        int YCount = MIN(Context->Height, Height - TopLeftOfWindow.y);
        
        for (int Y = 0; Y < YCount; Y++) {
            for (int X = 0; X < XCount; X++) {
                unsigned char* Pixel = (unsigned char*)Mapped.pData + (TopLeftOfWindow.y + Y) * Mapped.RowPitch + (TopLeftOfWindow.x + X) * 4;
                Context->Pixels[Y * Context->Width + X] = {Pixel[0], Pixel[1], Pixel[2]};
            }
        }
    }
    
    
    Context->DeviceContext->Unmap(StagingTexture, 0);
    
    DesktopResource->Release();
    AcquiredDesktopImage->Release();
    StagingTexture->Release();
    Context->DesktopDup->ReleaseFrame();
}

static std::vector<inference_detection>
RunInference(cv::dnn::Net& Network, cv::Size ModelShape, const cv::Mat& Frame)
{
    cv::Mat Blob;
    {
        TimeBlock("blobFromImage");
        cv::dnn::blobFromImage(Frame, Blob, 1.0f / 255.0f, ModelShape, cv::Scalar(), true, false);
    }
    {
        TimeBlock("setInput");
        Network.setInput(Blob);
    }
    
    std::vector<cv::Mat> Outputs;
    {
        TimeBlock("forward");
        Network.forward(Outputs, Network.getUnconnectedOutLayersNames());
    }
    int BatchSize = Outputs[0].size[0];
    int Rows = Outputs[0].size[2];
    int Dimensions = Outputs[0].size[1];
    
    float XScale = (float)Frame.cols / ModelShape.width;
    float YScale = (float)Frame.rows / ModelShape.height;
    
    {
        TimeBlock("Reshape and transpose");
        Outputs[0] = Outputs[0].reshape(1, Dimensions);
        cv::transpose(Outputs[0], Outputs[0]);
    }
    float* Data = (float*)Outputs[0].data;
    
    std::vector<inference_detection> Detections;
    
    std::vector<f32> Confidences;
    std::vector<cv::Rect> Boxes;
    
    float Threshold = 0.3f;
    {
        TimeBlock("Check confidences");
        for (int Row = 0; Row < Rows; Row++)
        {
            float Score = Data[4];
            
            if (Score > Threshold)
            {
                float X = Data[0] * XScale;
                float Y = Data[1] * YScale;
                float W = Data[2] * XScale;
                float H = Data[3] * YScale;
                
                inference_detection Inference = {Score, X, Y, W, H};
                Detections.push_back(Inference);
                
                Boxes.emplace_back((int)(X - 0.5f * W), (int)(Y - 0.5f * H), (int)W, (int)H);
                Confidences.push_back(Score);
            }
            
            Data += Dimensions;
        }
    }
    f32 NMSThreshold = 0.4f;
    
    //Run non-maximum suppression to eliminate duplicate detections
    std::vector<inference_detection> Result;
    {
        TimeBlock("NMS");
        std::vector<int> NMSResult;
        cv::dnn::NMSBoxes(Boxes, Confidences, Threshold, NMSThreshold, NMSResult);
        
        for (int Index : NMSResult)
        {
            Result.push_back(Detections[Index]);
        }
    }
    return Result;
}

static void
SendUpdate(HANDLE Arduino, int8_t dX, int8_t dY)
{
    PCMouseUpdate Update = {};
    Update.dX = dX;
    Update.dY = dY;
    
    WriteFile(Arduino, &Update, sizeof(Update), 0, 0);
}

int main() {
    GetCPUFrequency(100);
    Sleep(3000);
    directx_capture_context Context = CreateDirectXContext();
    
    HANDLE ArduinoCOM = CreateFileA("\\\\.\\COM3", GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
    if (ArduinoCOM == INVALID_HANDLE_VALUE)
    {
        MessageBoxA(0, "Failed to get connect to the arduino", "Error", MB_OK|MB_ICONWARNING);
    }
    
    char* ModelPath = "../volov8s_10batch_1280size.onnx";
    //char* ModelPath = "../yolov8n_1280size.onnx";
    cv::Size ModelShape = {1280, 1280};
    //char* ModelPath = "../volov8s_32batch_300epoch.onnx";
    //cv::Size ModelShape = {640, 640};
    
    bool UseCUDA = true;
    
    cv::dnn::Net ONNXNetwork = cv::dnn::readNetFromONNX(ModelPath);
    
    
    if (UseCUDA)
    {
        ONNXNetwork.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
        ONNXNetwork.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
    }
    
    while (true)
    {
        BeginProfile();
        
        GetFrame(&Context);
        
        {
            cv::Mat Image(Context.Height, Context.Width, CV_8UC3, Context.Pixels);
            
            std::vector<inference_detection> Inferences = RunInference(ONNXNetwork, ModelShape, Image);
            {
                TimeBlock("Render");
                for (inference_detection Inference : Inferences)
                {
                    cv::Rect Rect = cv::Rect((int)(Inference.X - 0.5f * Inference.W), (int)(Inference.Y - 0.5f * Inference.H), (int)Inference.W, (int)Inference.H);
                    cv::rectangle(Image, Rect, cv::Scalar(1.0f), 2);
                }
                
                cv::imshow("Valorant Window", Image);
                
                
                if (Inferences.size() > 0)
                {
                    inference_detection Inference = Inferences[0];
                    
                    //Get normalised coordinates
                    float X = Inference.X / Context.Width;
                    float Y = Inference.Y / Context.Height;
                    
                    float CenterX = 0.5f;
                    float CenterY = 0.52f;
                    
                    float dX = X - CenterX;
                    float dY = Y - CenterY;
                    
                    float Speed = 10000.0f;
                    
                    int MoveX = Speed * dX;
                    int MoveY = Speed * dY;
                    
                    char MessageBuffer[256];
                    sprintf_s(MessageBuffer, "dX: %d, dY: %d\n", MoveX, MoveY);
                    printf(MessageBuffer);
                    OutputDebugStringA(MessageBuffer);
                    
                    int MaxMove = 50;
                    
                    while (MoveX != 0 || MoveY != 0)
                    {
                        int SendX = 0;
                        int SendY = 0;
                        
                        if (MoveX > 0)
                        {
                            SendX = MIN(MoveX, MaxMove);
                        }
                        if (MoveX < 0)
                        {
                            SendX = MAX(MoveX, -MaxMove);
                        }
                        if (MoveY > 0)
                        {
                            SendY = MIN(MoveY, MaxMove);
                        }
                        if (MoveY < 0)
                        {
                            SendY = MAX(MoveY, -MaxMove);
                        }
                        
                        MoveX -= SendX;
                        MoveY -= SendY;
                        
                        SendUpdate(ArduinoCOM, (int8_t)SendX, (int8_t)SendY);
                    }
                }
                
                
                if (cv::waitKey(1) == 'q')
                {
                    break;
                }
            }
        }
        
        OutputProfileReadout();
    }
    
    
    return 0;
}