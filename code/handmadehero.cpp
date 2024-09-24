#include <windows.h>
#include <stdint.h>
#include <math.h>

// define static as it vary depends on local/global variable or function

#define internal static
#define local_persist static
#define global_variable static

// TODO(molivera): This is a global just for now.
global_variable bool GlobalRunning;

struct win32_offscreen_buffer {
  BITMAPINFO Info;
  void *Memory;
  int Width;
  int Height;
  int Pitch;
  int BytesPerPixel = 4;
};

struct win32_window_dimension {
	int Width;
	int Height;
};

global_variable win32_offscreen_buffer GlobalBackbuffer;

win32_window_dimension
Win32GetWindowDimension(HWND Window) {
	win32_window_dimension Result;

	RECT ClientRect;
	GetClientRect(Window, &ClientRect);
	Result.Width = ClientRect.right - ClientRect.left;
	Result.Height = ClientRect.bottom - ClientRect.top;

	return Result;
}

internal void
RenderWeirdGradient(
	win32_offscreen_buffer Buffer, int BlueOffset, int GreenOffset
) {
	uint8_t *Row = (uint8_t *)Buffer.Memory;
	for (int y = 0; y < Buffer.Height; ++y) {
		uint32_t *Pixel = (uint32_t *)Row;
		for (int x = 0; x < Buffer.Width; ++x) {
			/*
				NOTE(molivera): This is a little endian architecture

				but people at Microsoft wanted that the memory look like
				0xppRRGGBB
				(pp = Padding, RR = Red, GG = Green, BB = Blue)

				As it is little endian, in memory is:

				Pixel in memory = BB GG RR pp
			  */
		uint8_t Blue = (x + BlueOffset);
		uint8_t Green = (y + GreenOffset);
		// uint8_t Red = (x + y) % 2 == 0 ? (x + BlueOffset) : (y + GreenOffset);

		*Pixel++ = ((Green << 8) | Blue);
		}
		Row += Buffer.Pitch;
	}
}

internal void
Win32ResizeDIBSection(
	win32_offscreen_buffer *Buffer, int Width, int Height
) {
	// TODO(molivera): Bullerproof this.
	// Free after, then free first if that fails?

	if (Buffer->Memory) {
		VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
	}

	Buffer->Width = Width;
	Buffer->Height = Height;
	Buffer->BytesPerPixel = 4;

	// NOTE(casey): When the biHeight fieldi s negative, this is the clue to
	// Windwos to treat this bitmap as top-down, not bottom-up, meaning that
	// the first three bytes of memory ar the color for the top left pixel
	// in the bitmap, not the bottom left!
	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Buffer->Width;
	Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;

	// NOTE(casey): thanks to Chris Hecker
	int BitmapMemorySize =
		(Buffer->Width * Buffer->Height) * Buffer->BytesPerPixel;
	Buffer->Memory =
		VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

	Buffer->Pitch = Width * Buffer->BytesPerPixel;
}

internal void
Win32DisplayBufferInWindow(
	HDC DeviceContext, int WindowWidth, int WindowHeight,
	win32_offscreen_buffer Buffer,
	int X, int Y, int Width, int Height
) {
	// TODO(molivera): Fix aspect ratio
	StretchDIBits(
		DeviceContext,
		0, 0, WindowWidth, WindowHeight,
		0, 0, Buffer.Width, Buffer.Height,
		Buffer.Memory, &Buffer.Info,
        DIB_RGB_COLORS, SRCCOPY
	);
}

LRESULT CALLBACK
Win32MainWindowCallback(
	HWND Window, UINT Message, WPARAM WParam, LPARAM LParam) {
	LRESULT Result = 0;

	switch (Message) {

	case WM_SIZE: {
	} break;

	case WM_CLOSE: {
		// TODO(molivera): Handle this with a message to the user?
		GlobalRunning = false;
	} break;

	case WM_DESTROY: {
		// TODO(molivera): Handle this as an error - recreate window?
		GlobalRunning = false;
	} break;

	case WM_ACTIVATEAPP: {
		OutputDebugStringA("WM_ACTIVATEAPP\n");
	} break;

	case WM_PAINT: {
		PAINTSTRUCT Paint;
		HDC DeviceContext = BeginPaint(Window, &Paint);
		int X = Paint.rcPaint.left;
		int Y = Paint.rcPaint.top;
		int Width = Paint.rcPaint.right - Paint.rcPaint.left;
		int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;

		win32_window_dimension Dimension = Win32GetWindowDimension(Window);
		Win32DisplayBufferInWindow(
				DeviceContext, Dimension.Width, Dimension.Height,
				GlobalBackbuffer,
				X, Y, Width, Height
		);

		EndPaint(Window, &Paint);
	} break;

	default: {
		// OutputDebugStringA("default\n");
		Result = DefWindowProc(Window, Message, WParam, LParam);
	}
	}

	return (Result);
}

int CALLBACK
WinMain(
	HINSTANCE Instance, HINSTANCE PrevInstance,
	LPSTR CommandLine, int ShowCode
) {
	WNDCLASSA WindowClass = {};

	Win32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);

	WindowClass.style = CS_HREDRAW | CS_VREDRAW;
	WindowClass.lpfnWndProc = Win32MainWindowCallback;
	WindowClass.hInstance = Instance;
	WindowClass.lpszClassName = "HandmadeHeroWindowClass";

	if (RegisterClassA(&WindowClass)) {
		HWND Window = CreateWindowExA(
			0,
			WindowClass.lpszClassName,
			"Handmade Hero",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			0, 0, Instance, 0
		);

		if (Window) {
			GlobalRunning = true;

			int XOffset = 0;
			int YOffset = 0;
			int radius = 100;
			double angle_increment = 0.01;
			double angle = 0;

			while (GlobalRunning) {
				MSG Message;

				while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE)) {
					if (Message.message == WM_QUIT) {
						GlobalRunning = false;
					}

					TranslateMessage(&Message);
					DispatchMessage(&Message);
				}

				RenderWeirdGradient(GlobalBackbuffer, XOffset, YOffset);

				HDC DeviceContext = GetDC(Window);
				win32_window_dimension Dimension = Win32GetWindowDimension(Window);

				Win32DisplayBufferInWindow(
					DeviceContext, Dimension.Width, Dimension.Height,
					GlobalBackbuffer,
					0, 0, Dimension.Width, Dimension.Height
				);
				ReleaseDC(Window, DeviceContext);

				XOffset = (radius * cos(angle));
				YOffset = (radius * sin(angle));
				angle += angle_increment;
			}
		} else {
			// TODO(molivera): Logging
		}
	} else {
		// TODO(molivera): Logging
	}

	return (0);
}
