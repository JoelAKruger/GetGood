#include <opencv2/opencv.hpp>
#include <Windows.h>
#include <onnxruntime_cxx_api.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "onnxruntime.lib")
#pragma comment(lib, "../../opencv/build/x64/vc16/lib/opencv_world4100.lib")

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

#define INPUT_DIMENSION 640

game_context CreateGameContext()
{
    game_context Context = {};
    
    Context.Window = GetForegroundWindow();
    /*
    if (!ValWindow)
    {
        MessageBoxA(0, "Could not find Valorant window", "Error", MB_OK|MB_ICONWARNING);
        return 0;
    }
*/
    
    Context.DC = GetDC(Context.Window);
    
    RECT WindowRect = {};
    GetClientRect(Context.Window, &WindowRect);
    
    Context.Width = WindowRect.right;
    Context.Height = WindowRect.bottom;
    
    Context.CompatibleDC = CreateCompatibleDC(Context.DC);
    Context.Bitmap = CreateCompatibleBitmap(Context.DC, INPUT_DIMENSION, INPUT_DIMENSION);
    
    SelectObject(Context.CompatibleDC, Context.Bitmap);
    StretchBlt(Context.CompatibleDC, 0, 0, INPUT_DIMENSION, INPUT_DIMENSION, Context.DC, 0, 0, Context.Width, Context.Height, SRCCOPY);
    
    Context.BitmapHeader.biSize = sizeof(BITMAPINFOHEADER);
    Context.BitmapHeader.biWidth = INPUT_DIMENSION;
    Context.BitmapHeader.biHeight = -INPUT_DIMENSION;
    Context.BitmapHeader.biPlanes = 1;
    Context.BitmapHeader.biBitCount = 32;
    Context.BitmapHeader.biCompression = BI_RGB;
    
    Context.Pixels = VirtualAlloc(0, INPUT_DIMENSION * INPUT_DIMENSION * 4, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
    
    GetDIBits(Context.CompatibleDC, Context.Bitmap, 0, INPUT_DIMENSION, Context.Pixels,  (BITMAPINFO*)&Context.BitmapHeader, DIB_RGB_COLORS);
    
    return Context;
}

int WINAPI wWinMain(HINSTANCE Instance, HINSTANCE, LPWSTR CommandLine, int ShowCode)
{
    wchar_t* ModelPath = L"../volov8s_32batch_300epoch.onnx";
    
    auto providers = Ort::GetAvailableProviders();
    for (auto provider : providers) {
        OutputDebugStringA(provider.c_str());
        OutputDebugStringA("\n");
    }
    
    Sleep(3000);
    game_context Game = CreateGameContext();
    
    cv::Mat Image(INPUT_DIMENSION, INPUT_DIMENSION, CV_8UC4, Game.Pixels);
    
    Ort::Env env = Ort::Env(OrtLoggingLevel::ORT_LOGGING_LEVEL_WARNING, "Default");
    Ort::SessionOptions sessionOptions;
    sessionOptions.SetInterOpNumThreads(1);
    sessionOptions.SetIntraOpNumThreads(1);
    sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_DISABLE_ALL);
    
    Ort::Session session = Ort::Session(env, ModelPath, sessionOptions);
    
    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    
    //std::vector<Ort::Value> inputTensor;        // Onnxruntime allowed input
    
    ///Ort::Value::CreateTensor<float>(memory_info, (float*)Image.data, input_tensor_size, input_node_dims[0].data(), input_node_dims[0].size());
    
    cv::imshow("Valorant Window", Image);
    cv::waitKey(0);
    
    return 0;
}