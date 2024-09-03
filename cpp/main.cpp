#include <Windows.h>

#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>

#include <assert.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "onnxruntime.lib")
#pragma comment(lib, "opencv_world4100.lib")

#include "Types.h"
#include "Timer.cpp"
#include "Profiler.cpp"

struct game_context
{
    HWND Window;
    int Width, Height;
    HDC DC;
    HDC CompatibleDC;
    HBITMAP Bitmap;
    BITMAPINFOHEADER BitmapHeader;
    
    void* Pixels;
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

static game_context
CreateGameContext()
{
    game_context Context = {};
    
    Context.Window = GetForegroundWindow();
    
    if (!Context.Window)
    {
        MessageBoxA(0, "Failed to get the window", "Error", MB_OK|MB_ICONWARNING);
        return {};
    }
    
    
    Context.DC = GetDC(0);
    
    RECT WindowRect = {};
    GetWindowRect(Context.Window, &WindowRect);
    
    Context.Width = (WindowRect.right - WindowRect.left) / 4 * 4;
    Context.Height = (WindowRect.bottom - WindowRect.top) / 4 * 4;
    
    Context.CompatibleDC = CreateCompatibleDC(Context.DC);
    Context.Bitmap = CreateCompatibleBitmap(Context.DC, Context.Width, Context.Height);
    
    SelectObject(Context.CompatibleDC, Context.Bitmap);
    
    Context.BitmapHeader.biSize = sizeof(BITMAPINFOHEADER);
    Context.BitmapHeader.biWidth = Context.Width;
    Context.BitmapHeader.biHeight = -Context.Height;
    Context.BitmapHeader.biPlanes = 1;
    Context.BitmapHeader.biBitCount = 24;
    Context.BitmapHeader.biCompression = BI_RGB;
    
    Context.Pixels = VirtualAlloc(0, Context.Width * Context.Height * 3, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
    
    return Context;
}

static void
GetFrame(game_context* Context)
{
    TimeFunction;
    
    RECT WindowRect = {};
    GetWindowRect(Context->Window, &WindowRect);
    BitBlt(Context->CompatibleDC, 0, 0, Context->Width, Context->Height, Context->DC, WindowRect.left, WindowRect.top, SRCCOPY);
    GetDIBits(Context->CompatibleDC, Context->Bitmap, 0, Context->Height, Context->Pixels,  (BITMAPINFO*)&Context->BitmapHeader, DIB_RGB_COLORS);
}

static std::vector<inference_detection>
RunInference(cv::dnn::Net& Network, cv::Size ModelShape, const cv::Mat& Frame)
{
    TimeFunction;
    
    cv::Mat Blob;
    cv::dnn::blobFromImage(Frame, Blob, 1.0f / 255.0f, ModelShape, cv::Scalar(), true, false);
    
    Network.setInput(Blob);
    
    std::vector<cv::Mat> Outputs;
    Network.forward(Outputs, Network.getUnconnectedOutLayersNames());
    
    int BatchSize = Outputs[0].size[0];
    int Rows = Outputs[0].size[2];
    int Dimensions = Outputs[0].size[1];
    
    float XScale = (float)Frame.cols / ModelShape.width;
    float YScale = (float)Frame.rows / ModelShape.height;
    
    Outputs[0] = Outputs[0].reshape(1, Dimensions);
    cv::transpose(Outputs[0], Outputs[0]);
    float* Data = (float*)Outputs[0].data;
    
    std::vector<inference_detection> Detections;
    
    std::vector<f32> Confidences;
    std::vector<cv::Rect> Boxes;
    
    float Threshold = 0.3f;
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
    
    f32 NMSThreshold = 0.4f;
    
    //Run non-maximum suppression to eliminate duplicate detections
    std::vector<int> NMSResult;
    cv::dnn::NMSBoxes(Boxes, Confidences, Threshold, NMSThreshold, NMSResult);
    
    std::vector<inference_detection> Result;
    
    for (int Index : NMSResult)
    {
        Result.push_back(Detections[Index]);
    }
    
    return Result;
}

int WINAPI wWinMain(HINSTANCE Instance, HINSTANCE, LPWSTR CommandLine, int ShowCode)
{
    GetCPUFrequency(100);
    
    char* ModelPath = "../volov8s_10batch_1280size.onnx";
    
    //Wait for user to switch to the game window
    Sleep(3000);
    
    game_context Game = CreateGameContext();
    
    
    HANDLE ArduinoCOM = CreateFileA("\\\\.\\COM3", GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
    if (ArduinoCOM == INVALID_HANDLE_VALUE)
    {
        MessageBoxA(0, "Failed to get connect to the arduino", "Error", MB_OK|MB_ICONWARNING);
    }
    
    
    bool UseCUDA = true;
    
    cv::dnn::Net ONNXNetwork = cv::dnn::readNetFromONNX(ModelPath);
    cv::Size ModelShape = {1280, 1280};
    
    if (UseCUDA)
    {
        ONNXNetwork.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
        ONNXNetwork.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
    }
    
    char* ClassName = "enemy";
    
    while (true)
    {
        BeginProfile();
        
        GetFrame(&Game);
        
        {
            cv::Mat Image(Game.Height, Game.Width, CV_8UC3, Game.Pixels);
            
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
                    float X = Inference.X / Game.Width;
                    float Y = Inference.Y / Game.Height;
                    
                    float dX = X - 0.5f;
                    float dY = Y - 0.5f;
                    
                    float Speed = 100.0f;
                    
                    PCMouseUpdate Update = {};
                    Update.dX = Speed * dX;
                    Update.dY = Speed * dY;
                    
                    char MessageBuffer[256];
                    sprintf_s(MessageBuffer, "dX: %d, dY: %d", Update.dX, Update.dY);
                    OutputDebugStringA(MessageBuffer);
                    
                    WriteFile(ArduinoCOM, &Update, sizeof(Update), 0, 0);
                }
                
                
                cv::waitKey(1);
            }
        }
        
        OutputProfileReadout();
    }
    
    return 0;
}