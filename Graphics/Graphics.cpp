#include "Graphics.h"

bool Graphics::Initialize(HWND hwnd, int width, int height)
{
	if (!InitializeDirectX(hwnd, width, height))
		return false;

	if (!InitializeShaders())
		return false;

	if (!InitializeScene())
		return false;

	return true;
}

void Graphics::RenderFrame()
{
	float bgcolor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	this->deviceContext->ClearRenderTargetView(this->renderTargetView.Get(), bgcolor);
	this->deviceContext->ClearDepthStencilView(this->depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	this->deviceContext->IASetInputLayout(this->vertexshader.GetInputLayout());
	this->deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY::D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	this->deviceContext->RSSetState(this->rasterizerState.Get());
	this->deviceContext->VSSetShader(vertexshader.GetShader(), NULL, 0);
	this->deviceContext->PSSetShader(pixelshader.GetShader(), NULL, 0);

	UINT stride = sizeof(Vertex);
	UINT offset = 0;

	{
		//For each outline, we'll have to clear the stencil so that previous shapes aren't obscuring new shapes
		this->deviceContext->ClearDepthStencilView(this->depthStencilView.Get(), D3D11_CLEAR_STENCIL, 0.0f, 0);
		{
			//Green Tri (We have to draw green tri first so we know which pixels to omit for red tri)
			this->deviceContext->OMSetDepthStencilState(this->depthStencilState_default.Get(), 0); //Use default stencil so all pixels update stencil (stencil ref is irrelevant here)
			this->deviceContext->PSSetShader(NULL, NULL, 0); //DISABLE PIXEL SHADER SO WE AREN'T DRAWING GREEN TRI
			this->deviceContext->IASetVertexBuffers(0, 1, vertexBuffer2.GetAddressOf(), &stride, &offset);
			this->deviceContext->Draw(8, 0);
		}
		{
			//Red square
			this->deviceContext->OMSetDepthStencilState(this->depthStencilState_discard.Get(), 0); //use discard stencil and only draw where stencil value = 0 (pixel has not been accessed)
			this->deviceContext->PSSetShader(pixelshader.GetShader(), NULL, 0); //ENABLE PIXEL SHADER SO WE CAN DRAW RED TRI
			this->deviceContext->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
			this->deviceContext->Draw(8, 0);
		}
	}

	this->swapChain->Present(0, NULL);
}

Vertex * Graphics::GetSquareVertices(float x0, float y0, float x1, float y1, float width, float height, Vertex* v, unsigned int startIndex)
{
	float xx0 = 2.0f * (x0 - 0.5f) / width - 1.0f;
	float yy0 = 1.0f - 2.0f * (y0 - 0.5f) / height;
	float xx1 = 2.0f * (x1 - 0.5f) / width - 1.0f;
	float yy1 = 1.0f - 2.0f * (y1 - 0.5f) / height;

	v[startIndex] = Vertex(xx0, yy0, 0.0f, 0.0f, 1.0f, 0.0f);
	v[startIndex + 1] = Vertex(xx1, yy0, 0.0f, 0.0f, 1.0f, 0.0f);
	v[startIndex + 2] = Vertex(xx0, yy1, 0.0f, 0.0f, 1.0f, 0.0f);
	v[startIndex + 3] = Vertex(xx1, yy1, 0.0f, 0.0f, 1.0f, 0.0f);

	return v;
}

bool Graphics::InitializeDirectX(HWND hwnd, int width, int height)
{
	std::vector<AdapterData> adapters = AdapterReader::GetAdapters();

	if (adapters.size() < 1)
	{
		ErrorLogger::Log("No IDXGI Adapters found.");
		return false;
	}

	DXGI_SWAP_CHAIN_DESC scd;
	ZeroMemory(&scd, sizeof(DXGI_SWAP_CHAIN_DESC));

	scd.BufferDesc.Width = width;
	scd.BufferDesc.Height = height;
	scd.BufferDesc.RefreshRate.Numerator = 60;
	scd.BufferDesc.RefreshRate.Denominator = 1;
	scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	scd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	scd.SampleDesc.Count = 1;
	scd.SampleDesc.Quality = 0;

	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scd.BufferCount = 1;
	scd.OutputWindow = hwnd;
	scd.Windowed = TRUE;
	scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	HRESULT hr;
	hr = D3D11CreateDeviceAndSwapChain(
		adapters[0].pAdapter, //IDXGI Adapter
		D3D_DRIVER_TYPE_UNKNOWN,
		NULL, //FOR SOFTWARE DRIVER TYPE
		NULL, //FLAGS FOR RUNTIME LAYERS
		NULL, //FEATURE LEVELS ARRAY
		0, //# OF FEATURE LEVELS IN ARRAY
		D3D11_SDK_VERSION,
		&scd, //Swapchain description
		this->swapChain.GetAddressOf(), //Swapchain Address
		this->device.GetAddressOf(), //Device Address
		NULL, //Supported feature level
		this->deviceContext.GetAddressOf()
	); //Device Context Address

	if (FAILED(hr))
	{
		ErrorLogger::Log(hr, "Failed to create device and swapchain.");
		return false;
	}

	Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
	hr = this->swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuffer.GetAddressOf()));
	if (FAILED(hr)) //If error occurred
	{
		ErrorLogger::Log(hr, "GetBuffer Failed.");
		return false;
	}

	hr = this->device->CreateRenderTargetView(backBuffer.Get(), NULL, this->renderTargetView.GetAddressOf());
	if (FAILED(hr)) //If error occurred
	{
		ErrorLogger::Log(hr, "Failed to create render target view.");
		return false;
	}

	//Describe our Depth/Stencil Buffer
	D3D11_TEXTURE2D_DESC depthStencilDesc;

	depthStencilDesc.Width = width;
	depthStencilDesc.Height = height;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.ArraySize = 1;
	depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthStencilDesc.SampleDesc.Count = 1;
	depthStencilDesc.SampleDesc.Quality = 0;
	depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
	depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthStencilDesc.CPUAccessFlags = 0;
	depthStencilDesc.MiscFlags = 0;	

	hr = this->device->CreateTexture2D(&depthStencilDesc, NULL, this->depthStencilBuffer.GetAddressOf());

	if (FAILED(hr)) //If error occurred
	{
		ErrorLogger::Log(hr, "Failed to create depth stencil buffer.");
		return false;
	}

	hr = this->device->CreateDepthStencilView(this->depthStencilBuffer.Get(), NULL, this->depthStencilView.GetAddressOf());

	if (FAILED(hr)) //If error occurred
	{
		ErrorLogger::Log(hr, "Failed to create depth stencil view.");
		return false;
	}

	this->deviceContext->OMSetRenderTargets(1, this->renderTargetView.GetAddressOf(), this->depthStencilView.Get());

	//Create depth stencil state
	D3D11_DEPTH_STENCIL_DESC depthstencildesc;
	ZeroMemory(&depthstencildesc, sizeof(D3D11_DEPTH_STENCIL_DESC));

	//We are ignoring depth so DepthEnable = FALSE and DepthFunc = NEVER
	depthstencildesc.DepthEnable = FALSE;
	depthstencildesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK::D3D11_DEPTH_WRITE_MASK_ALL;
	depthstencildesc.DepthFunc = D3D11_COMPARISON_FUNC::D3D11_COMPARISON_NEVER;

	depthstencildesc.StencilEnable = TRUE;
	depthstencildesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	depthstencildesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;

	depthstencildesc.FrontFace.StencilFunc = D3D11_COMPARISON_FUNC::D3D11_COMPARISON_ALWAYS; //In the default stencil state, always pass, don't even do a comparison function
	depthstencildesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP; //KEEP STENCIL BUFFER VALUE (WILL NEVER FAIL SO TECHNICALLY IRRELEVANT)
	/*
	Explanation of "depthstencildesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_INCR_SAT; "
	INCREMENT & CLAMP VALUE - IF THE VALUE WAS 0 (DEFAULT), AND WE PASS TEST (WHICH WE WILL SINCE WE USE ALWAYS)
	INCREMENT VALUE OF STENCIL BUFFER BY 1 AND CLAMP. MAX 255 I THINK?
	THE DISCARD DEPTH STENCIL WILL USE THIS VALUE TO COMPARE TO SEE IF A PIXEL HAS BEEN WRITTEN TO IN THAT STENCIL
	BUFFER LOCATION.*/
	depthstencildesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_INCR_SAT;
	depthstencildesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP; //KEEP STENCIL BUFFER VALUE (DEPTH IS DISABLED SO SHOULD NEVER FAIL? TECHNICALLY IRRELEVANT)

	// We are not rendering backfaces, so never run any comparison function on backface (D3D11_COMPARISON_NEVER)
	depthstencildesc.BackFace.StencilFunc = D3D11_COMPARISON_FUNC::D3D11_COMPARISON_NEVER;
	depthstencildesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP; //NOT RELEVANT, TEST NEVER OCCURS
	depthstencildesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP; //NOT RELEVANT, TEST NEVER OCCURS
	depthstencildesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP; //NOT RELEVANT, TEST NEVER OCCURS

	hr = this->device->CreateDepthStencilState(&depthstencildesc, this->depthStencilState_default.GetAddressOf());

	if (FAILED(hr))
	{
		ErrorLogger::Log(hr, "Failed to create depth stencil state.");
		return false;
	}

	//Discard blend state (Just need to change a few things from default depth stencil)
	depthstencildesc.FrontFace.StencilFunc = D3D11_COMPARISON_FUNC::D3D11_COMPARISON_EQUAL; //ONLY PASS IF THE SOURCE PIXEL VALUE = STENCILREF
	depthstencildesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP; //KEEP STENCIL BUFFER VALUE (same as default)
	depthstencildesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP; //KEEP STENCIL BUFFER VALUE (Notice in default we incremented, but in this we don't! We only want to compare to the stencil value written by the previous stencil state)
	depthstencildesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP; //KEEP STENCIL BUFFER VALUE (same as default)

	hr = this->device->CreateDepthStencilState(&depthstencildesc, this->depthStencilState_discard.GetAddressOf());

	if (FAILED(hr))
	{
		ErrorLogger::Log(hr, "Failed to create depth stencil state.");
		return false;
	}

	//Create the Viewport
	D3D11_VIEWPORT viewport;
	ZeroMemory(&viewport, sizeof(D3D11_VIEWPORT));

	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = width;
	viewport.Height = height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	//Set the Viewport
	this->deviceContext->RSSetViewports(1, &viewport);

	//Create Rasterizer State
	D3D11_RASTERIZER_DESC rasterizerDesc;
	ZeroMemory(&rasterizerDesc, sizeof(D3D11_RASTERIZER_DESC));

	rasterizerDesc.FillMode = D3D11_FILL_MODE::D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_MODE::D3D11_CULL_NONE;

	hr = this->device->CreateRasterizerState(&rasterizerDesc, this->rasterizerState.GetAddressOf());
	if (FAILED(hr))
	{
		ErrorLogger::Log(hr, "Failed to create rasterizer state.");
		return false;
	}

	return true;
}

bool Graphics::InitializeShaders()
{

	std::wstring shaderfolder = L"";
	#pragma region DetermineShaderPath
	if (IsDebuggerPresent() == TRUE)
	{
		#ifdef _DEBUG //Debug Mode
			#ifdef _WIN64 //x64
				shaderfolder = L".\\x64\\Debug\\";
			#else  //x86 (Win32)
				shaderfolder = L"..\\Debug\\";
			#endif
		#else //Release Mode
			#ifdef _WIN64 //x64
				shaderfolder = L".\\x64\\Release\\";
			#else  //x86 (Win32)
				shaderfolder = L".\\Release\\";
			#endif
		#endif
	}

	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT::DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_CLASSIFICATION::D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT::DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_CLASSIFICATION::D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	UINT numElements = ARRAYSIZE(layout);

	if (!vertexshader.Initialize(this->device, shaderfolder + L"vertexshader.cso", layout, numElements))
		return false;

	if (!pixelshader.Initialize(this->device, shaderfolder + L"pixelshader.cso"))
		return false;

	return true;
}

bool Graphics::InitializeScene()
{
	// Green square

	Vertex v[8];

	GetSquareVertices(20.0f, 20.0f, 250.0f, 500.f, 800.0f, 600.0f, v, 0);
	GetSquareVertices(50.0f, 50.0f, 280.0, 530.0f, 800.0f, 600.0f, v, 4);

	D3D11_BUFFER_DESC vertexBufferDesc;
	ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));

	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = sizeof(Vertex) * ARRAYSIZE(v);	
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA vertexBufferData;
	ZeroMemory(&vertexBufferData, sizeof(vertexBufferData));
	vertexBufferData.pSysMem = v;

	HRESULT hr = this->device->CreateBuffer(&vertexBufferDesc, &vertexBufferData, this->vertexBuffer.GetAddressOf());
	if (FAILED(hr))
	{
		ErrorLogger::Log(hr, "Failed to create vertex buffer.");
		return false;
	}	

	float sideThickness = 5.0f;
	float topThickness= 5.0f;
	
	Vertex v2[8];

	GetSquareVertices(20.0f + sideThickness, 20.0f + topThickness, 250.0f - sideThickness, 500.0f - topThickness, 800.0f, 600.0f, v2, 0);
	GetSquareVertices(50.0f + sideThickness, 50.0f + topThickness, 280.0f - sideThickness, 530.0f - topThickness, 800.0f, 600.0f, v2, 4);

	ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));

	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = sizeof(Vertex) * ARRAYSIZE(v2);
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;

	ZeroMemory(&vertexBufferData, sizeof(vertexBufferData));
	vertexBufferData.pSysMem = v2;

	hr = this->device->CreateBuffer(&vertexBufferDesc, &vertexBufferData, this->vertexBuffer2.GetAddressOf());
	if (FAILED(hr))
	{
		ErrorLogger::Log(hr, "Failed to create vertex buffer.");
		return false;
	}

	return true;
}