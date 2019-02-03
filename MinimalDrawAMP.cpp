#include <amp.h>
#include <amp_graphics.h>

#include <d3dcommon.h>
#include <d3d11.h>
#include <atlcomcli.h>
#include "DirectXTK/WICTextureLoader.h"

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPTSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	WNDCLASSEX wcex = { sizeof(wcex) };
	wcex.lpfnWndProc = DefWindowProc;
	wcex.hInstance = hInstance;
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.lpszClassName = _T(__FUNCTION__);

	HWND hWnd = CreateWindow(
		MAKEINTATOM(RegisterClassEx(&wcex)), wcex.lpszClassName, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

	if (!hWnd) return 1;

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	CComPtr<ID3D11Device> dev;
	CComPtr<ID3D11DeviceContext> ctx;
	CComPtr<IDXGISwapChain> swapchain;

	DXGI_SWAP_CHAIN_DESC desc = {};
	desc.BufferCount = 1;
	desc.SampleDesc.Count = 1;
	desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.OutputWindow = hWnd;
	desc.Windowed = TRUE;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_UNORDERED_ACCESS | DXGI_USAGE_SHADER_INPUT;;

    D3D_FEATURE_LEVEL FeatureLevel = D3D_FEATURE_LEVEL_11_0;

	D3D11CreateDeviceAndSwapChain(
		0, D3D_DRIVER_TYPE_HARDWARE,
		0, D3D11_CREATE_DEVICE_DEBUG,
		&FeatureLevel, 1, D3D11_SDK_VERSION,
		&desc, &swapchain, &dev, 0, &ctx);

	if (!swapchain) return 1;

	using namespace concurrency::direct3d;
	using namespace concurrency::graphics;
	using namespace concurrency::graphics::direct3d;

    auto av = create_accelerator_view(dev);

	CComPtr<ID3D11Texture2D> image;
	CComPtr<ID3D11ShaderResourceView> image_srv;
	if (FAILED(CreateWICTextureFromFile(dev, ctx, L"Bruce", reinterpret_cast<ID3D11Resource**>(&image), &image_srv)))
		return 1;

    D3D11_TEXTURE2D_DESC input_tex_desc;
    image->GetDesc(&input_tex_desc);
    UINT img_width = input_tex_desc.Width;
    UINT img_height = input_tex_desc.Height;

    // create a texture the same size as image, it's used to store the effect applied texture
    texture<unorm4, 2> processed_texture(
        static_cast<int>(img_height), 
        static_cast<int>(img_width),
        8U, av);

    RECT rc;
    GetClientRect( hWnd, &rc );
    size_t wd_width = rc.right - rc.left;
    size_t wd_height = rc.bottom - rc.top;

    DXGI_SWAP_CHAIN_DESC sd;
    swapchain->GetDesc(&sd);
    if (sd.BufferDesc.Width != wd_width || sd.BufferDesc.Height != wd_height) {
        if (FAILED(swapchain->ResizeBuffers(sd.BufferCount, (UINT)wd_width, (UINT)wd_height, sd.BufferDesc.Format, 0)))
            return false;
    }

	CComPtr<ID3D11Texture2D> backbuffer;
	swapchain->GetBuffer(0, IID_PPV_ARGS(&backbuffer));

	auto tex = make_texture<unorm4, 2>(av, backbuffer);
	texture_view<unorm4, 2> tv(tex);
	const auto ext = tv.extent;

	auto image_texture = make_texture<unorm4, 2>(av, image);
    bool bInvert = false;

	MSG msg;
	while (::IsWindow(hWnd))
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            DispatchMessage(&msg);
            switch (msg.message)
            {
            case WM_KEYDOWN:
                bInvert = !bInvert;
                break;
            }
        }
		else
		{
			const float t = (msg.time%1000)*0.001f;
			concurrency::parallel_for_each(
				tv.accelerator_view, ext, [=, &image_texture](concurrency::index<2> idx) restrict(amp)
			{
                unorm4 val = image_texture[idx];
                if (bInvert)
                    val = unorm4(1.0) - val;
                    tv.set(idx, val);
			});

			swapchain->Present(1, 0);
		}
	return 0;
}
