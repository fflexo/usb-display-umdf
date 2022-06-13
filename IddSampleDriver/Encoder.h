#pragma once
#include <windows.h>
#include <bugcodes.h>
#include <wudfwdm.h>
#include <wdf.h>
#include <iddcx.h>

#include <dxgi1_5.h>
#include <d3d11_2.h>
#include <avrt.h>
#include <wrl.h>
#include <iosfwd>

#define FRAME_SIZE_MAX (1024*1024*32)

void *SaveToPipe(HANDLE pipe, ID3D11Texture2D* Texture, LPOVERLAPPED state, std::wostream& debug_log);