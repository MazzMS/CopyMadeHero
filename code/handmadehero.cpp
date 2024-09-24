#include <windows.h>
#include <stdint.h>

// define static as it vary depends on local/global variable or function

#define internal static
#define local_persist static
#define global_variable static

typdef int8_t int8;
typdef int16_t int16;
typdef int32_t int32;
typdef int64_t int64;

typdef uint8_t uint8;
typdef uint16_t uint16;
typdef uint32_t uint32;
typdef uint64_t uint64;

// TODO(molivera): This is a global just for now.
global_variable bool Running;

global_variable BITMAPINFO BitmapInfo;
global_variable void *BitmapMemory;
global_variable int BitmapWidth;
global_variable int BitmapHeight;
global_variable int BytesPerPixel = 4;

internal void RenderWeirdGradient(int XOffset, int YOffset) {
	int Width = BitmapWidth;
	int Height = BitmapHeight;

	int Pitch = Width*BytesPerPixel;
	uint8 *Row = (uint8 *)BitmapMemory;
	for(int y = 0; y < BitmapHeight; ++y) {
		uint32 *Pixel = (uint32 *)Row;
		for(int x = 0; x < BitmapWidth; ++x) {
			/*
				NOTE(molivera): This is a little endian architecture

				but people at Microsoft wanted that the memory look like
				0xppRRGGBB
				(pp = Padding, RR = Red, GG = Green, BB = Blue)

				As it is little endian, in memory is:

				Pixel in memory = BB GG RR pp
			*/

			// Blue
			*Pixel = (unit8)(X + XOffset);
			++Pixel;

			// Green
			*Pixel = (unit8)(Y + YOffset);
			++Pixel;

			// Red
			*Pixel = 0;
			++Pixel;

			// Padding
			*Pixel = 0;
			++Pixel;
		}
		Row += Pitch;
	}
}

internal void Win32ResizeDIBSection(int Width, int Height) {
	// TODO(molivera): Bullerproof this.
	// Free after, then free first if that fails?

	if(BitmapMemory) {
		VirtualFree(BitmapMemory, 0, MEM_RELEASE);
	}

	BitmapWidth = Width;
	BitmapHeight = Height;

	BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
	BitmapInfo.bmiHeader.biWidth = BitmapWidth;
	BitmapInfo.bmiHeader.biHeight = -BitmapHeight;
	BitmapInfo.bmiHeader.biPlanes = 1;
	BitmapInfo.bmiHeader.biBitCount = 32;
	BitmapInfo.bmiHeader.biCompression = BI_RGB;

	// NOTE(casey): thanks to Chris Hecker
	int BitmapMemorySize = (Width * Height) * BytesPerPixel;
	BitmapMemory = VirtualAlloc(0, BitmapMemory, MEM_COMMIT, PAGE_READWRITE);

	RenderWeirdGradient(0, 0);
}

internal void Win32UpdateWindow(
	HDC DeviceContext, RECT *WindowRect, int X, int Y, int Width, int Height
) {
	int WindowWidth = WindowRect->right - WindowRect->left;
	int WindowHeight = WindowRect->bottom - WindowRect->top;

	StretchDIBits(
		DeviceContext,
		0, 0, BitmapWidth, BitmapHeight,
		0, 0, WindowWidth, WindowHeight,
		BitmapMemory,
		&BitmapInfo,
		DIB_RGB_COLORS,
		SRCCOPY
	);
}

LRESULT CALLBACK Win32MainWindowCallback(
	HWND Window, UINT MEssage, WPARAM WParam, LPARAM LParam
) {
	LRESULT Result = 0;

	switch (message){

		case WM_SIZE:
		{
			RECT ClientRect;
			GetClientRect(Window, &ClientRect);
			int Width = ClientRect.right - ClientRect.left;
			int Height = ClientRect.bottom - ClientRect.top;
			Win32ResizeDIBSection(Width, Height);
		} break;

		case WM_CLOSE:
		{
			// TODO(molivera): Handle this with a message to the user?
			Running = false;
		} break;

		case WM_DESTROY:
		{
			// TODO(molivera): Handle this as an error - recreate window?
			Running = false;
		} break;

		case WM_ACTIVATEAPP:
		{
			OutputDebugStringA("WM_ACTIVATEAPP\n");
		} break;

		case WM_PAINT:
		{
			PAINSTRUCT Paint;
			HDC DeviceContext = BeginPain(Window, &Paint);
			int X = Paint.rcPaint.left;
			int Y = Paint.rcPaint.top;
			int Width = Paint.rcPaint.right - Paint.rcPaint.left;
			int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;

			RECT ClientRect;
			GetClientRect(Window, &ClientRect);

			Win32UpdateWindow(DeviceContext, &ClientRect, X, Y, Width, Height);
			EndPaint(Window, &Paint);
		} break;

		default:
		{
			// OutputDebugStringA("default\n");
			Result = DefWindowProc(Window, Message, WParam, LParam);
		}
	}
}

int CALLBACK WinMain(
	HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode
) {
	WNDCLASSA WindowClass = {};

	// TODO(molivera): Check if HREDRAW/VREDRAW/OWNDC still matter
	WindwowClass.lpfnWndProc = Win32MainWindowCallback;
	WindwowClass.hInstance = Instance;
	WindwowClass.lpszClassName = "HandmadeHeroWindowClass";

	if(RegisterClassA(&WindowClass)){
		HWND WindowHandle = CreateWindowExA(
			0, WindowClass.lpszClassName, "Handmade Hero",
			WS_OVERLAPPEDWINDOW|WS_VISIBLE,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			0, 0,
			Instance,
			0);

		if(WindowHandle){
			Running = true;

			while(Running){
				MSG Message;

				while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE)){
					if(Message.message == WM_QUIT) {
						Running = false;
					}

					TranslateMessage(&Message);
					DispatchMessage(&Message);
				}
			}
		} else {
			// TODO(molivera): Logging
		}
	} else {
		// TODO(molivera): Logging
	}

	return(0);
}
