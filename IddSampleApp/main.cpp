#include <iostream>
#include <vector>

#include <windows.h>
#include <swdevice.h>
#include <conio.h>
#include <wrl.h>
#include <sddl.h>


VOID WINAPI
CreationCallback(
    _In_ HSWDEVICE hSwDevice,
    _In_ HRESULT hrCreateResult,
    _In_opt_ PVOID pContext,
    _In_opt_ PCWSTR pszDeviceInstanceId
    )
{
    HANDLE hEvent = *(HANDLE*) pContext;

    SetEvent(hEvent);
    UNREFERENCED_PARAMETER(hSwDevice);
    UNREFERENCED_PARAMETER(hrCreateResult);
    UNREFERENCED_PARAMETER(pszDeviceInstanceId);
}

#define pipename L"\\\\.\\pipe\\UsbDisplay"

static HANDLE CreateFramePipe(void) {
    SECURITY_ATTRIBUTES sa = {};

    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = FALSE;

    // TODO: ACL IS DEFINITELY WRONG
    BOOL ok = ConvertStringSecurityDescriptorToSecurityDescriptor(L"D:P(A;; GRGW;;; UD)",
            SDDL_REVISION_1,
            &sa.lpSecurityDescriptor,
            NULL);
    if (!ok) {
        printf("SDDL wrong\n");
        exit(EXIT_FAILURE);
    }
    else {
        printf("SDDL Ok\n");
    }
    HANDLE pipe = CreateNamedPipe(pipename, PIPE_ACCESS_DUPLEX, PIPE_WAIT|PIPE_TYPE_MESSAGE|PIPE_REJECT_REMOTE_CLIENTS, 1, 64*1024, 32*1024*1024, 120 * 1000, &sa);
    if (INVALID_HANDLE_VALUE == pipe) {
        printf("Failed to create frame pipe\n");
        exit(EXIT_FAILURE);
    }
    LocalFree(sa.lpSecurityDescriptor);
    return pipe;
}

static void RunPipe(HANDLE pipe) {
    //char data[1024];
    std::vector<char> frame(32 * 1024 * 1024);
    DWORD numRead;

    ConnectNamedPipe(pipe, NULL);

    while (1) {
        numRead = -1;
        ReadFile(pipe, &frame[0], frame.size(), &numRead, NULL);
        printf("Got %d bytes in frame\n", numRead);
    }
}

int __cdecl main(int argc, wchar_t *argv[])
{
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    HANDLE pipe = CreateFramePipe();

    HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    HSWDEVICE hSwDevice;
    SW_DEVICE_CREATE_INFO createInfo = { 0 };
    PCWSTR description = L"Idd Sample Driver";

    // These match the Pnp id's in the inf file so OS will load the driver when the device is created    
    PCWSTR instanceId = L"IddSampleDriver";
    PCWSTR hardwareIds = L"IddSampleDriver\0\0";
    PCWSTR compatibleIds = L"IddSampleDriver\0\0";

    createInfo.cbSize = sizeof(createInfo);
    createInfo.pszzCompatibleIds = compatibleIds;
    createInfo.pszInstanceId = instanceId;
    createInfo.pszzHardwareIds = hardwareIds;
    createInfo.pszDeviceDescription = description;

    createInfo.CapabilityFlags = SWDeviceCapabilitiesRemovable |
                                 SWDeviceCapabilitiesSilentInstall |
                                 SWDeviceCapabilitiesDriverRequired;

    // Create the device
    HRESULT hr = SwDeviceCreate(L"IddSampleDriver",
                                L"HTREE\\ROOT\\0",
                                &createInfo,
                                0,
                                nullptr,
                                CreationCallback,
                                &hEvent,
                                &hSwDevice);
    if (FAILED(hr))
    {
        printf("SwDeviceCreate failed with 0x%lx\n", hr);
        return 1;
    }

    // Wait for callback to signal that the device has been created
    printf("Waiting for device to be created....\n");
    DWORD waitResult = WaitForSingleObject(hEvent, 10*1000);
    if (waitResult != WAIT_OBJECT_0)
    {
        printf("Wait for device creation failed\n");
        return 1;
    }
    printf("Device created\n\n");

    RunPipe(pipe);
    
    // Now wait for user to indicate the device should be stopped
    printf("Press 'x' to exit and destory the software device\n");
    bool bExit = false;
    do
    {
        // Wait for key press
        int key = _getch();

        if (key == 'x' || key == 'X')
        {
            bExit = true;
        }
    }while (!bExit);
    
    // Stop the device, this will cause the sample to be unloaded
    SwDeviceClose(hSwDevice);

    return 0;
}