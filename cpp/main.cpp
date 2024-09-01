#include <opencv2/opencv.hpp>
#include <Windows.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "../../opencv/build/x64/vc16/lib/opencv_world4100.lib")

int WINAPI wWinMain(HINSTANCE Instance, HINSTANCE, LPWSTR CommandLine, int ShowCode)
{
    HWND ValWindow = FindWindowEx(0, 0, "Notepad", 0);
    
    if (!ValWindow)
    {
        MessageBoxA(0, "Could not find Valorant window", "Error", MB_OK|MB_ICONWARNING);
        return 0;
    }
    
    HDC ValWindowDC = GetDC(ValWindow);
    
    RECT WindowRect = {};
    GetClientRect(ValWindow, &WindowRect);
    
    int ValWidth = WindowRect.right;
    int ValHeight = WindowRect.bottom;
    
    HDC ValWindowCompatibleDC = CreateCompatibleDC(ValWindowDC);
    HBITMAP ValWindowBitmap = CreateCompatibleBitmap(ValWindowDC, ValWidth, ValHeight);
    
    SelectObject(ValWindowCompatibleDC, ValWindowBitmap);
    BitBlt(ValWindowCompatibleDC, 0, 0, ValWidth, ValHeight, ValWindowDC, 0, 0, SRCCOPY);
    
    BITMAPINFOHEADER BitmapHeader = {};
    BitmapHeader.biSize = sizeof(BITMAPINFOHEADER);
    BitmapHeader.biWidth = ValWidth;
    BitmapHeader.biHeight = -ValHeight;
    BitmapHeader.biPlanes = 1;
    BitmapHeader.biBitCount = 32;
    BitmapHeader.biCompression = BI_RGB;
    
    void* Pixels = VirtualAlloc(0, ValWidth * ValHeight * 4, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
    
    GetDIBits(ValWindowCompatibleDC, ValWindowBitmap, 0, ValHeight, Pixels, (BITMAPINFO*)&BitmapHeader, DIB_RGB_COLORS);
    
    cv::Mat Image(ValHeight, ValWidth, CV_8UC4, Pixels);
    cv::imshow("Valorant Window", Image);
    cv::waitKey(0);
    
    return 0;
}