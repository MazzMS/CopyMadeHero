#include <windows.h>
#include <stdint.h>
#include <math.h>
#include <GameInput.h>
#include <xaudio2.h>

// define static as it vary depends on local/global variable or function

#define internal static
#define local_persist static
#define global_variable static

// TODO(molivera): This is a global just for now.
global_variable bool GlobalRunning;

global_variable int XAdditional = 0;
global_variable int YAdditional = 0;
global_variable int radius = 100;

struct win32_offscreen_buffer {
  BITMAPINFO Info;
  void *Memory;
  int Width;
  int Height;
  int Pitch;
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
	win32_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset
) {
	uint8_t *Row = (uint8_t *)Buffer->Memory;
	for (int y = 0; y < Buffer->Height; ++y) {
		uint32_t *Pixel = (uint32_t *)Row;
		for (int x = 0; x < Buffer->Width; ++x) {
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
		Row += Buffer->Pitch;
	}
}

internal void
Win32InitAudio() {
	// NOTE(molivera): Audio constants
	WORD BitsPerSample = 16;		// 16 bits per sample.
	DWORD SamplesPerSec = 44100;	// 44,100 samples per second.
	double CyclesPerSec = 256.0;		// 220 cycles per second (frequency of the audible tone).
	double Volume = 0.2;			// 50% volume.
	WORD AudioBufferSizeInCycles = 10;	// 10 cycles per audio buffer.
	double PI = 3.14159265358979323846;

	// Calculated constants.
	DWORD  SamplesPerCycle = (DWORD)(SamplesPerSec / CyclesPerSec);                // 200 samples per cycle.
	DWORD  AudioBufferSizeInSamples = SamplesPerCycle * AudioBufferSizeInCycles;   // 2,000 samples per buffer.
	uint32_t BufferSizeInBytes = AudioBufferSizeInSamples * BitsPerSample / 8; // 4,000 bytes per buffer.

	BYTE *RawBuffer = new BYTE[BufferSizeInBytes];

	HRESULT Res = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(Res)) {
		// TODO(molivera): Diagnostic
		delete[] RawBuffer;
		return;
	}

	IXAudio2 *XAudio2{};
	Res = XAudio2Create(&XAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	if (FAILED(Res)) {
		// TODO(molivera): Diagnostic
		delete[] RawBuffer;
		return;
	}

	IXAudio2MasteringVoice* MasteringVoice{};
	Res = XAudio2->CreateMasteringVoice(&MasteringVoice);
	if (FAILED(Res)) {
		// TODO(molivera): Diagnostic
		XAudio2->Release();
		delete[] RawBuffer;
		return;
	}

	// Format
	WAVEFORMATEX WaveFormat{};
    WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
    WaveFormat.nChannels = 2; // Stereo
    WaveFormat.nSamplesPerSec = SamplesPerSec;
	WaveFormat.nBlockAlign = (WaveFormat.nChannels * BitsPerSample) / 8;
    WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
    WaveFormat.wBitsPerSample = BitsPerSample;
    WaveFormat.cbSize = 0;

	// Source Voice
	IXAudio2SourceVoice* SourceVoice{};
	if (FAILED(XAudio2->CreateSourceVoice(&SourceVoice, &WaveFormat))){
		XAudio2->Release();
		delete[] RawBuffer;
		return;
	}

	// Fill buffer
	double phase = 0;
	uint32_t Index = 0;

	while(Index < AudioBufferSizeInSamples){
		int16_t sample = (int16_t)(sin(phase) * INT16_MAX * Volume);

		// Left Channel
		RawBuffer[Index++] = (BYTE)sample;
		RawBuffer[Index++] = (BYTE)(sample >> 8);

		// Right Channel
		RawBuffer[Index++] = (BYTE)sample;
		RawBuffer[Index++] = (BYTE)(sample >> 8);

		// Advance phase
		phase += (2.0 * PI) / SamplesPerCycle;
		if (phase >= (2.0 * PI)) {
			phase -= (2.0 * PI); // Keep phase within 0 to 2π
		}
	}

	XAUDIO2_BUFFER Buffer{};
	Buffer.Flags = XAUDIO2_END_OF_STREAM;
	Buffer.AudioBytes = BufferSizeInBytes; // BufferSize
	Buffer.pAudioData = RawBuffer;
	Buffer.PlayBegin = 0;
	Buffer.PlayLength = 0;
	Buffer.LoopBegin = 0;
	Buffer.LoopLength = 0;
	Buffer.LoopCount = XAUDIO2_LOOP_INFINITE;

	// Submit buffer and start voice
	if (FAILED(SourceVoice->SubmitSourceBuffer(&Buffer))){
		XAudio2->Release();
		delete[] RawBuffer;
		return;
	}

	if (FAILED(SourceVoice->Start(0))){
		XAudio2->Release();
		delete[] RawBuffer;
		return;
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
	int BytesPerPixel = 4;

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
		(Buffer->Width * Buffer->Height) * BytesPerPixel;
	Buffer->Memory =
		VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

	Buffer->Pitch = Width * BytesPerPixel;
}

internal void
Win32DisplayBufferInWindow(
	HDC DeviceContext, int WindowWidth, int WindowHeight,
	win32_offscreen_buffer *Buffer
) {
	// TODO(molivera): Fix aspect ratio
	StretchDIBits(
		DeviceContext,
		0, 0, WindowWidth, WindowHeight,
		0, 0, Buffer->Width, Buffer->Height,
		Buffer->Memory, &Buffer->Info,
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

	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_KEYDOWN:
	case WM_KEYUP: {
		uint32_t VKcode = WParam;
		bool WasDown = ((LParam & (1 << 30)) == 0);
		bool IsDown = ((LParam & (1 << 31)) != 0);

		if(WasDown != IsDown) {
			if(VKcode == 'W'){
				YAdditional += 20;
			}
			if(VKcode == 'A'){
				XAdditional += 20;
			}
			if(VKcode == 'S'){
				YAdditional -= 20;
			}
			if(VKcode == 'D'){
				XAdditional -= 20;
			}
			if(VKcode == 'D'){
				XAdditional -= 20;
			}
			if(VKcode == VK_SPACE){
				radius = (radius == 100) ? 0 : 100;
			}
		}
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
				&GlobalBackbuffer
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
			double angle_increment = 0.01;
			double angle = 0;

			// Controller
			IGameInput *GameInput = nullptr;

			//
			Win32InitAudio();

			while (GlobalRunning) {
				MSG Message;

				while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE)) {
					if (Message.message == WM_QUIT) {
						GlobalRunning = false;
					}

					TranslateMessage(&Message);
					DispatchMessage(&Message);
				}

				// Gamepad input
//				IGameInputReading *reading;
//				if (SUCCEEDED(GameInput->GetCurrentReading(
//					GameInputKindGamepad,
//					nullptr,
//					&reading
//				))) {
//					GameInputGamepadState state;
//					reading->GetGamepadState(&state);
//
//					bool Menu = state.buttons & GameInputGamepadMenu;
//					bool A = state.buttons & GameInputGamepadA;
//					bool B = state.buttons & GameInputGamepadB;
//					bool X = state.buttons & GameInputGamepadX;
//					bool Y = state.buttons & GameInputGamepadY;
//					bool Up = state.buttons & GameInputGamepadDPadUp;
//					bool Down = state.buttons & GameInputGamepadDPadDown;
//					bool Left = state.buttons & GameInputGamepadDPadLeft;
//					bool Rigth = state.buttons & GameInputGamepadDPadRight;
//					bool LeftShoulder = state.buttons & GameInputGamepadLeftShoulder;
//					bool RightShoulder = state.buttons & GameInputGamepadRightShoulder;
//
//					float LeftThumbstickX = state.leftThumbstickX;
//					float LeftThumbstickY = state.leftThumbstickY;
//					float RightThumbstickX = state.rightThumbstickX;
//					float RightThumbstickY = state.rightThumbstickY;
//
//					reading->Release();
//				}

				RenderWeirdGradient(&GlobalBackbuffer, XOffset, YOffset);


				HDC DeviceContext = GetDC(Window);
				win32_window_dimension Dimension = Win32GetWindowDimension(Window);

				Win32DisplayBufferInWindow(
					DeviceContext, Dimension.Width, Dimension.Height,
					&GlobalBackbuffer
				);
				ReleaseDC(Window, DeviceContext);

				XOffset = (radius * cos(angle)) + XAdditional;
				YOffset = (radius * sin(angle)) + YAdditional;
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
