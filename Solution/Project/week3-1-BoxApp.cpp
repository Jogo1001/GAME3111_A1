/** @file week3-1-BoxApp.cpp
 *  @brief Shows how to draw a box in Direct3D 12.
 *
 *   Controls:
 *   Hold the left mouse button down and move the mouse to rotate.
 *   Hold the right mouse button down and move the mouse to zoom in and out.
 *
 *  @author Hooman Salamat
 */


#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"

#include "../../Common/GeometryGenerator.h"
#include "FrameResource.h"

#include <iostream>
#include <string>

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

//step3: Our application class instantiates a vector of three frame resources, 
const int gNumFrameResources = 3;

//struct Vertex
//{
//    XMFLOAT3 Pos;
//    XMFLOAT4 Color;
//};
//
//struct ObjectConstants
//{
//    XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
//};
struct RenderItem
{
	RenderItem() = default;

	// World matrix of the shape that describes the object's local space
	// relative to the world space, which defines the position, orientation,
	// and scale of the object in the world.
	XMFLOAT4X4 World = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify object data, we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	// Geometry associated with this render-item. Note that multiple
	// render-items can share the same geometry.
	MeshGeometry* Geo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced parameters.
	UINT IndexCount = 0; //Number of indices read from the index buffer for each instance.
	UINT StartIndexLocation = 0; //The location of the first index read by the GPU from the index buffer.
	int BaseVertexLocation = 0; //A value added to each index before reading a vertex from the vertex buffer.
};

class ShapesApp : public D3DApp
{
public:
	ShapesApp(HINSTANCE hInstance);
    ShapesApp(const ShapesApp& rhs) = delete;
    ShapesApp& operator=(const ShapesApp& rhs) = delete;
	~ShapesApp();

	virtual bool Initialize()override;

private:
    virtual void OnResize()override;
    virtual void Update(const GameTimer& gt)override;
    virtual void Draw(const GameTimer& gt)override;

    virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

	void OnKeyboardInput(const GameTimer& gt);
	void UpdateCamera(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);

	void BuildDescriptorHeaps();
	void BuildConstantBufferViews();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildShapeGeometry();
	void BuildPSOs();


	//step5
	void BuildFrameResources();

	void BuildRenderItems();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);


private:
    
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
    ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;


	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	//step11: Our application will maintain lists of render items based on how they need to be
	//drawn; that is, render items that need different PSOs will be kept in different lists.

	// Render items divided by PSO.
	std::vector<RenderItem*> mOpaqueRitems;

	//std::vector<RenderItem*> mTransparentRitems;  //we could have render items for transparant items



	//step12: this mMainPassCB stores constant data that is fixed over a given
	//rendering pass such as the eye position, the view and projection matrices, and information
	//about the screen(render target) dimensions; it also includes game timing information,
	//which is useful data to have access to in shader programs.

	PassConstants mMainPassCB;

	UINT mPassCbvOffset = 0;

	bool mIsWireframe = false;

	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	float mTheta = 1.5f * XM_PI;
	float mPhi = 0.2f * XM_PI;
	float mRadius = 15.0f;

	POINT mLastMousePos;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
				   PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    try
    {
        ShapesApp theApp(hInstance);
        if(!theApp.Initialize())
            return 0;

        return theApp.Run();
    }
    catch(DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}

ShapesApp::ShapesApp(HINSTANCE hInstance)
: D3DApp(hInstance) 
{
}

ShapesApp::~ShapesApp()
{
	if (md3dDevice != nullptr)
		FlushCommandQueue();
}

bool ShapesApp::Initialize()
{
    if(!D3DApp::Initialize())
		return false;
		
    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));
 
	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildShapeGeometry();
	BuildRenderItems();
	BuildFrameResources();
	BuildDescriptorHeaps();
	BuildConstantBufferViews();
	BuildPSOs();

    // Execute the initialization commands.
    ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until initialization is complete.
    FlushCommandQueue();

	return true;
}

void ShapesApp::OnResize()
{
	D3DApp::OnResize();

    // The window resized, so update the aspect ratio and recompute the projection matrix.
    XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&mProj, P);
}



void ShapesApp::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);
	UpdateCamera(gt);

	//! Cycle through the circular frame resource array.
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	//! this section is really what D3DApp::FlushCommandQueue() used to do for us at the end of each draw() function!
	//! Has the GPU finished processing the commands of the current frame resource. 
	//! If not, wait until the GPU has completed commands up to this fence point.
	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		std::wstring text2 = L"GPU Completed " + std::to_wstring(mFence->GetCompletedValue()) + L" but current fence is " + std::to_wstring(mCurrFrameResource->Fence) + L"\n";
		OutputDebugString(text2.c_str());

		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	//The idea of these changes is to group constants based on update frequency. The per
	//pass constants only need to be updated once per rendering pass, and the object constants
	//only need to change when an object�s world matrix changes.
	UpdateObjectCBs(gt);
	UpdateMainPassCB(gt);

}

void ShapesApp::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	
    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(cmdListAlloc->Reset());

	if (mIsWireframe)
	{
		ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque_wireframe"].Get()));
	}
	else
	{
		ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));
	}

	//step3: A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
   // ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), mPSO.Get()));

    //Hooman: The subrectangle of the back buffer we draw into is called the viewport 
    //mScreenViewport.Width = 200;
    //mScreenViewport.Height  = 200;

    mCommandList->RSSetViewports(1, &mScreenViewport);

    //Hooman: The following example creates and sets a scissor rectangle 
    //that covers the upper - left quadrant of the back buffer :

    //mScissorRect = { 0, 0, mClientWidth / 2, mClientHeight / 2 };

    mCommandList->RSSetScissorRects(1, &mScissorRect);

    // Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));


    // Clear the back buffer and depth buffer.
    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	
    // Specify the buffers we are going to render to.
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
	int passCbvIndex = mPassCbvOffset + mCurrFrameResourceIndex;
	auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
	passCbvHandle.Offset(passCbvIndex, mCbvSrvUavDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable(1, passCbvHandle);

	DrawRenderItems(mCommandList.Get(), mOpaqueRitems);
	
    // Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    //step4: Done recording commands. Comment this line out to see what debugger will tell you in the output window.
	ThrowIfFailed(mCommandList->Close());
 
    // Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
	
	// swap the back and front buffers
		//HRESULT Present(UINT SyncInterval,UINT Flags);
	//SyncInterval: 0 - The presentation occurs immediately, there is no synchronization.
	//Flags defined by the DXGI_PRESENT constants. 0: Present a frame from each buffer (starting with the current buffer) to the output.
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// Wait until frame commands are complete.  This waiting is inefficient and is
	// done for simplicity.  Later we will show how to organize our rendering code
	// so we do not have to wait per frame.
	//FlushCommandQueue();
	mCurrFrameResource->Fence = ++mCurrentFence;

	// Add an instruction to the command queue to set a new fence point. 
	// Because we are on the GPU timeline, the new fence point won't be 
	// set until the GPU finishes processing all the commands prior to this Signal().
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}



void ShapesApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    mLastMousePos.x = x;
    mLastMousePos.y = y;

    SetCapture(mhMainWnd);
}

void ShapesApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void ShapesApp::OnMouseMove(WPARAM btnState, int x, int y)
{
    if((btnState & MK_LBUTTON) != 0)
    {
        // Make each pixel correspond to a quarter of a degree.
        float dx = XMConvertToRadians(0.25f*static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(0.25f*static_cast<float>(y - mLastMousePos.y));

        // Update angles based on input to orbit camera around box.
        mTheta += dx;
        mPhi += dy;

        // Restrict the angle mPhi.
        mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
    }
    else if((btnState & MK_RBUTTON) != 0)
    {
        // Make each pixel correspond to 0.005 unit in the scene.
        float dx = 0.005f*static_cast<float>(x - mLastMousePos.x);
        float dy = 0.005f*static_cast<float>(y - mLastMousePos.y);

        // Update the camera radius based on input.
        mRadius += dx - dy;

        // Restrict the radius.
        mRadius = MathHelper::Clamp(mRadius, 3.0f, 15.0f);
    }

    mLastMousePos.x = x;
    mLastMousePos.y = y;
}
void ShapesApp::OnKeyboardInput(const GameTimer& gt)
{
	//! Determines whether a key is up or down at the time the function is called, and whether the key was pressed after a previous call to GetAsyncKeyState.
	//! If the function succeeds, the return value specifies whether the key was pressed since the last call to GetAsyncKeyState, 
	//! and whether the key is currently up or down. If the most significant bit is set, the key is down, and if the least significant bit is set, the key was pressed after the previous call to GetAsyncKeyState.
	//! if (GetAsyncKeyState('1') & 0x8000) 

	short key = GetAsyncKeyState('1');
	//! you can use the virtual key code (0x31) for '1' as well
	//! https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
	//! short key = GetAsyncKeyState(0x31); 

	if (key & 0x8000)  //if one is pressed, 0x8000 = 32767 , key = -32767 = FFFFFFFFFFFF8001
		mIsWireframe = true;
	else
		mIsWireframe = false;
}
void ShapesApp::UpdateCamera(const GameTimer& gt)
{
	// Convert Spherical to Cartesian coordinates.
	mEyePos.x = mRadius * sinf(mPhi) * cosf(mTheta);
	mEyePos.z = mRadius * sinf(mPhi) * sinf(mTheta);
	mEyePos.y = mRadius * cosf(mPhi);

	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);
}

//step8: Update resources (cbuffers) in mCurrFrameResource
void ShapesApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for (auto& e : mAllRitems)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if (e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}
void ShapesApp::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mMainPassCB.EyePosW = mEyePos;
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}
void ShapesApp::BuildDescriptorHeaps()
{
	UINT objCount = (UINT)mOpaqueRitems.size();

	// Need a CBV descriptor for each object for each frame resource,
	// +1 for the perPass CBV for each frame resource.
	UINT numDescriptors = (objCount + 1) * gNumFrameResources;

	// Save an offset to the start of the pass CBVs.  These are the last 3 descriptors.
	mPassCbvOffset = objCount * gNumFrameResources;

	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NumDescriptors = numDescriptors;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&cbvHeapDesc,
		IID_PPV_ARGS(&mCbvHeap)));
}

void ShapesApp::BuildConstantBufferViews()
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	UINT objCount = (UINT)mOpaqueRitems.size();

	// Need a CBV descriptor for each object for each frame resource.
	for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
	{
		auto objectCB = mFrameResources[frameIndex]->ObjectCB->Resource();
		for (UINT i = 0; i < objCount; ++i)
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = objectCB->GetGPUVirtualAddress();

			// Offset to the ith object constant buffer in the buffer.
			cbAddress += i * objCBByteSize;

			// Offset to the object cbv in the descriptor heap.
			int heapIndex = frameIndex * objCount + i;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = objCBByteSize;

			md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
		}
	}

	UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

	// Last three descriptors are the pass CBVs for each frame resource.
	for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
	{
		auto passCB = mFrameResources[frameIndex]->PassCB->Resource();
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress = passCB->GetGPUVirtualAddress();

		// Offset to the pass cbv in the descriptor heap.
		int heapIndex = mPassCbvOffset + frameIndex;
		auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
		handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = cbAddress;
		cbvDesc.SizeInBytes = passCBByteSize;

		md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
	}
}




void ShapesApp::BuildRootSignature()
{
	// Shader programs typically require resources as input (constant buffers,
	// textures, samplers).  The root signature defines the resources the shader
	// programs expect.  If we think of the shader programs as a function, and
	// the input resources as function parameters, then the root signature can be
	// thought of as defining the function signature.  

	CD3DX12_DESCRIPTOR_RANGE cbvTable0;
	cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

	CD3DX12_DESCRIPTOR_RANGE cbvTable1;
	cbvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[2];

	// Create root CBVs.
	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0);
	slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable1);

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter, 0, nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);


	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if(errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(&mRootSignature)));
}




void ShapesApp::BuildShadersAndInputLayout()
{
    HRESULT hr = S_OK;
    
	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\VS.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\PS.hlsl", nullptr, "PS", "ps_5_1");


    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

void ShapesApp::BuildShapeGeometry()
{


	GeometryGenerator geoGen;


	// box geometry for castle's walls
	// Main walls for the castles
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 3.0f, 1.0f, 3);

	// smaller Box geometry for castle door
	// box that will be placed in the front wall
	GeometryGenerator::MeshData door = geoGen.CreateBox(1.0f, 2.0f, 1.0f, 3);

	// Cylinder geometry for castle towers
    // Creates cylindrical towers for castle corners
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.5f, 2.0f, 20, 20);

	// Grid geometry for land 
	// Creates a flat surface for the ground
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(4.0f, 20.0f, 40, 40);

	// Grid geometry for rooftop
	// Creates a  grid for a  rooftop
	GeometryGenerator::MeshData rooftop = geoGen.CreateGrid(4.0f, 5.0f, 260, 260);



	//new geometry
	GeometryGenerator::MeshData cone = geoGen.CreateCone(0.5f, 2.0f, 20);

	GeometryGenerator::MeshData wedge = geoGen.CreateWedge(1.0f, 2.0f, 1.0f, 20);

	GeometryGenerator::MeshData torus = geoGen.CreateTorus(2.0f, 0.5f, 32, 16);

	GeometryGenerator::MeshData pyramid = geoGen.CreatePyramid(2.0f, 3.0f, 2.0f);

	GeometryGenerator::MeshData diamond = geoGen.CreateDiamond(2.0f, 3.0f,2.0f);  

	GeometryGenerator::MeshData diamond1 = geoGen.CreateDiamond1(2.0f, 3.0f, 2.0f);



	

	// Vertex offsets for different geometry pieces
	UINT boxVertexOffset = 0;
	UINT cylinderVertexOffset = (UINT)box.Vertices.size();
	UINT gridVertexOffset = cylinderVertexOffset + (UINT)cylinder.Vertices.size();
	UINT roofVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
	UINT doorVertexOffset = boxVertexOffset + (UINT)box.Vertices.size() + (UINT)cylinder.Vertices.size() + (UINT)grid.Vertices.size() + (UINT)rooftop.Vertices.size();
	UINT coneVertexOffset = doorVertexOffset + (UINT)door.Vertices.size(); 
	UINT wedgeVertexOffset = coneVertexOffset + (UINT)cone.Vertices.size();
	UINT torusVertexOffset = wedgeVertexOffset + (UINT)wedge.Vertices.size();
	UINT pyramidVertexOffset = torusVertexOffset + (UINT)torus.Vertices.size();
	UINT diamondVertexOffset = pyramidVertexOffset + (UINT)diamond.Vertices.size();
	UINT diamond1VertexOffset = diamondVertexOffset + (UINT)diamond1.Vertices.size();

	
	

	// Index offsets for different geometry pieces
	UINT boxIndexOffset = 0;
	UINT cylinderIndexOffset = (UINT)box.Indices32.size();
	UINT gridIndexOffset = cylinderIndexOffset + (UINT)cylinder.Indices32.size();
	UINT roofIndexOffset = gridIndexOffset + (UINT)grid.Vertices.size();
	UINT doorIndexOffset = boxIndexOffset + (UINT)box.Indices32.size() + (UINT)cylinder.Indices32.size() + (UINT)grid.Indices32.size() + (UINT)rooftop.Indices32.size();
	UINT coneIndexOffset = doorIndexOffset + (UINT)door.Indices32.size();
	UINT wedgeIndexOffset = coneIndexOffset + (UINT)cone.Indices32.size();
	UINT torusIndexOffset = wedgeIndexOffset + (UINT)wedge.Indices32.size();
	UINT pyramidIndexOffset = torusIndexOffset + (UINT)torus.Indices32.size();
	UINT diamondIndexOffset = pyramidIndexOffset + (UINT)diamond.Indices32.size();
	UINT diamond1IndexOffset = diamondIndexOffset + (UINT)diamond1.Indices32.size();

	

	// submesh data - these describe ranges of indices and vertices for each component
	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.BaseVertexLocation = gridVertexOffset;

	SubmeshGeometry roofSubmesh;
	roofSubmesh.IndexCount = (UINT)rooftop.Indices32.size();
	roofSubmesh.StartIndexLocation = roofVertexOffset;
	roofSubmesh.BaseVertexLocation = roofVertexOffset;

	SubmeshGeometry doorSubmesh;
	doorSubmesh.IndexCount = (UINT)door.Indices32.size();
	doorSubmesh.StartIndexLocation = doorIndexOffset;
	doorSubmesh.BaseVertexLocation = doorVertexOffset;

	SubmeshGeometry coneSubmesh;
	coneSubmesh.IndexCount = (UINT)cone.Indices32.size();
	coneSubmesh.StartIndexLocation = coneIndexOffset;
	coneSubmesh.BaseVertexLocation = coneVertexOffset;

	SubmeshGeometry wedgeSubmesh;
	wedgeSubmesh.IndexCount = (UINT)wedge.Indices32.size();
	wedgeSubmesh.StartIndexLocation = wedgeIndexOffset;
	wedgeSubmesh.BaseVertexLocation = wedgeVertexOffset;


	SubmeshGeometry torusSubmesh; // Submesh for torus
	torusSubmesh.IndexCount = (UINT)torus.Indices32.size();
	torusSubmesh.StartIndexLocation = torusIndexOffset;
	torusSubmesh.BaseVertexLocation = torusVertexOffset;


	SubmeshGeometry pyramidSubmesh; // Submesh for torus
	pyramidSubmesh.IndexCount = (UINT)pyramid.Indices32.size();
	pyramidSubmesh.StartIndexLocation = pyramidIndexOffset;
	pyramidSubmesh.BaseVertexLocation = pyramidVertexOffset;

	SubmeshGeometry diamondSubmesh; // Submesh for torus
	diamondSubmesh.IndexCount = (UINT)diamond.Indices32.size();
	diamondSubmesh.StartIndexLocation = diamondIndexOffset;
	diamondSubmesh.BaseVertexLocation = diamondVertexOffset;

	SubmeshGeometry diamond1Submesh; // Submesh for torus
	diamond1Submesh.IndexCount = (UINT)diamond1.Indices32.size();
	diamond1Submesh.StartIndexLocation = diamond1IndexOffset;
	diamond1Submesh.BaseVertexLocation = diamond1VertexOffset;

	// Total vertex count
	auto totalVertexCount = box.Vertices.size() + cylinder.Vertices.size() + grid.Vertices.size()
		+ rooftop.Vertices.size() + door.Vertices.size() + cone.Vertices.size() + wedge.Vertices.size() + torus.Vertices.size() 
		+ pyramid.Vertices.size() + diamond.Vertices.size() + diamond1.Vertices.size();


	std::vector<Vertex> vertices(totalVertexCount);


	// Assign vertex positions and colors
	UINT k = 0;
	// Box vertices (blue walls)
	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::Crimson); // wall color
	}
	// Cylinder vertices (red towers)
	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::Red); // Tower color
	}
	// Grid vertices (green ground)
	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::Green); // Grass color
	}
	// Rooftop vertices (yellow roof)
	for (size_t i = 0; i < rooftop.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = rooftop.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::Yellow); // roof color
	}
	// Door vertices (black door)
	for (size_t i = 0; i < door.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = door.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::SaddleBrown); // door color
	}
	for (size_t i = 0; i < cone.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cone.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::Yellow); // cone color
	}
	for (size_t i = 0; i < wedge.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = wedge.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::Navy); // Wedge color 
	}
	for (size_t i = 0; i < torus.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = torus.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::DarkViolet); // Torus color
	}
	for (size_t i = 0; i < pyramid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = pyramid.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::MidnightBlue); // pyramid color
	}
	for (size_t i = 0; i < diamond.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = diamond.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::Yellow); // diamond color
	}

	for (size_t i = 0; i < diamond1.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = diamond.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::Yellow); // diamond color
	}
	// Combine all indices into one buffer
	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));
	indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
	indices.insert(indices.end(), std::begin(rooftop.GetIndices16()), std::end(rooftop.GetIndices16()));
	indices.insert(indices.end(), std::begin(door.GetIndices16()), std::end(door.GetIndices16()));
	indices.insert(indices.end(), std::begin(cone.GetIndices16()), std::end(cone.GetIndices16()));
	indices.insert(indices.end(), std::begin(wedge.GetIndices16()), std::end(wedge.GetIndices16()));
	indices.insert(indices.end(), std::begin(torus.GetIndices16()), std::end(torus.GetIndices16()));
	indices.insert(indices.end(), std::begin(pyramid.GetIndices16()), std::end(pyramid.GetIndices16()));
	indices.insert(indices.end(), std::begin(diamond.GetIndices16()), std::end(diamond.GetIndices16()));
	indices.insert(indices.end(), std::begin(diamond1.GetIndices16()), std::end(diamond1.GetIndices16()));

	// Upload combined geometry to GPU
	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);


	// Create a new mesh geometry
	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "shapeGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	// Create GPU buffers
	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);
	// Create GPU buffers
	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);


	// Set buffer 
	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	// Assign submesh geometries to the mesh geometry game object
	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;
	geo->DrawArgs["rooftop"] = roofSubmesh;
	geo->DrawArgs["door"] = doorSubmesh;
	geo->DrawArgs["cone"] = coneSubmesh;
	geo->DrawArgs["wedge"] = wedgeSubmesh;
	geo->DrawArgs["torus"] = torusSubmesh;
	geo->DrawArgs["pyramid"] = pyramidSubmesh;
	geo->DrawArgs["diamond"] = diamondSubmesh;
	geo->DrawArgs["diamond1"] = diamond1Submesh;

	mGeometries[geo->Name] = std::move(geo);
}

void ShapesApp::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	//
	// PSO for opaque objects.
	//
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));


	//
	// PSO for opaque wireframe objects.
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
	opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mPSOs["opaque_wireframe"])));
}
void ShapesApp::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
			1, (UINT)mAllRitems.size()));
	}
}

void ShapesApp::BuildRenderItems()
{

	// Castle dimensions
	float castleWidth = 5.0f;
	float castleDepth = 5.0f;
	float wallHeight = 1.0f;
	float towerHeight = 2.0f;

	// (BOX GEOMETRY ) Create walls between towers
	int wallIndex = 4; 
	for (int i = 0; i < 4; ++i)
	{
		auto boxRitem = std::make_unique<RenderItem>();
		XMFLOAT3 wallPosition;
		XMFLOAT3 wallScale;

		// Configure walls based on direction
		if (i == 0) // Front wall
		{
			wallPosition = XMFLOAT3(0.0f, wallHeight / 2, -castleDepth / 2);
			wallScale = XMFLOAT3(castleWidth, wallHeight, 0.2f);
		}
		else if (i == 1) // Right wall
		{
			wallPosition = XMFLOAT3(castleWidth / 2, wallHeight / 2, 0.0f);
			wallScale = XMFLOAT3(0.2f, wallHeight, castleDepth);
		}
		else if (i == 2) // Back wall
		{
			wallPosition = XMFLOAT3(0.0f, wallHeight / 2, castleDepth / 2);
			wallScale = XMFLOAT3(castleWidth, wallHeight, 0.2f);
		}
		else if (i == 3) // Left wall
		{
			wallPosition = XMFLOAT3(-castleWidth / 2, wallHeight / 2, 0.0f);
			wallScale = XMFLOAT3(0.2f, wallHeight, castleDepth);
		}

		XMStoreFloat4x4(&boxRitem->World, XMMatrixScaling(wallScale.x, wallScale.y, wallScale.z) * XMMatrixTranslation(wallPosition.x, wallPosition.y, wallPosition.z));
		boxRitem->ObjCBIndex = wallIndex++;
		boxRitem->Geo = mGeometries["shapeGeo"].get();
		boxRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
		boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
		boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
		mAllRitems.push_back(std::move(boxRitem));
	}
	
	// Create four towers (cylinders) at the corners and place cones on top
	for (int i = 0; i < 4; ++i)
	{
		// Create a cylinder (tower)
		auto cylinderRitem = std::make_unique<RenderItem>();
		XMFLOAT3 towerPosition;
		if (i == 0) towerPosition = XMFLOAT3(-castleWidth / 2, towerHeight / 2, -castleDepth / 2); // Front-left
		else if (i == 1) towerPosition = XMFLOAT3(castleWidth / 2, towerHeight / 2, -castleDepth / 2); // Front-right
		else if (i == 2) towerPosition = XMFLOAT3(-castleWidth / 2, towerHeight / 2, castleDepth / 2); // Back-left
		else if (i == 3) towerPosition = XMFLOAT3(castleWidth / 2, towerHeight / 2, castleDepth / 2); // Back-right

		XMStoreFloat4x4(&cylinderRitem->World, XMMatrixScaling(1.0f, towerHeight, 1.0f) * XMMatrixTranslation(towerPosition.x, towerPosition.y, towerPosition.z));
		cylinderRitem->ObjCBIndex = i;

		// Link to cylinder geometry
		cylinderRitem->Geo = mGeometries["shapeGeo"].get();
		cylinderRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		cylinderRitem->IndexCount = cylinderRitem->Geo->DrawArgs["cylinder"].IndexCount;
		cylinderRitem->StartIndexLocation = cylinderRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		cylinderRitem->BaseVertexLocation = cylinderRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;
		mAllRitems.push_back(std::move(cylinderRitem));

		// Now create a cone on top of the cylinder
		auto coneRitem = std::make_unique<RenderItem>();
		// Position the cone just above the cylinder
		XMFLOAT3 conePosition = XMFLOAT3(towerPosition.x, towerPosition.y + towerHeight / 2 + 1.0f, towerPosition.z); // Adjust the height to be just above the cylinder
		XMFLOAT3 coneScale = XMFLOAT3(1.0f, 1.0f, 1.0f); // Scale for the cone (you can adjust this)

		// Set the cone position
		XMStoreFloat4x4(&coneRitem->World,
			XMMatrixScaling(coneScale.x, coneScale.y, coneScale.z) *
			XMMatrixTranslation(conePosition.x, conePosition.y, conePosition.z));


		coneRitem->ObjCBIndex = wallIndex++;  // Increment index
		coneRitem->Geo = mGeometries["shapeGeo"].get();
		coneRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		coneRitem->IndexCount = coneRitem->Geo->DrawArgs["cone"].IndexCount;
		coneRitem->StartIndexLocation = coneRitem->Geo->DrawArgs["cone"].StartIndexLocation;
		coneRitem->BaseVertexLocation = coneRitem->Geo->DrawArgs["cone"].BaseVertexLocation;




		/////////////////////////////////////////////////////
		mAllRitems.push_back(std::move(coneRitem));
	}




	// Create a door in the front wall
	auto doorRitem = std::make_unique<RenderItem>();
	XMFLOAT3 doorPosition = XMFLOAT3(0.0f, wallHeight / 2, -castleDepth / 2 + 0.1f); // Position in front wall
	XMFLOAT3 doorScale = XMFLOAT3(1.0f, 0.7f, 0.5f); // A smaller scale for the door
	XMStoreFloat4x4(&doorRitem->World, XMMatrixScaling(doorScale.x, doorScale.y, doorScale.z)
	* XMMatrixTranslation(doorPosition.x, doorPosition.y - 0.83, doorPosition.z));// Adjust vertical position
	doorRitem->ObjCBIndex = wallIndex++;
	doorRitem->Geo = mGeometries["shapeGeo"].get();
	doorRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	doorRitem->IndexCount = doorRitem->Geo->DrawArgs["door"].IndexCount;
	doorRitem->StartIndexLocation = doorRitem->Geo->DrawArgs["door"].StartIndexLocation;
	doorRitem->BaseVertexLocation = doorRitem->Geo->DrawArgs["door"].BaseVertexLocation;
	mAllRitems.push_back(std::move(doorRitem));

	// Create a wedge on the left side of the castle (Fence post)
	auto leftWedgeRitem = std::make_unique<RenderItem>();
	XMFLOAT3 leftWedgePosition = XMFLOAT3(-castleWidth / 2 - 1.0f, wallHeight / 2, -castleDepth / 2 + 0.1f); // Adjusted position
	XMFLOAT3 leftWedgeScale = XMFLOAT3(0.5f, wallHeight, 0.5f); // Smaller wedge scale
	XMStoreFloat4x4(&leftWedgeRitem->World,
	XMMatrixScaling(leftWedgeScale.x, leftWedgeScale.y, leftWedgeScale.z) *
	XMMatrixTranslation(leftWedgePosition.x, leftWedgePosition.y, leftWedgePosition.z) *
	XMMatrixRotationY(XMConvertToRadians(-15.0f))); // Rotate for fence-like slant

	leftWedgeRitem->ObjCBIndex = wallIndex++;
	leftWedgeRitem->Geo = mGeometries["shapeGeo"].get();
	leftWedgeRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	leftWedgeRitem->IndexCount = leftWedgeRitem->Geo->DrawArgs["wedge"].IndexCount;
	leftWedgeRitem->StartIndexLocation = leftWedgeRitem->Geo->DrawArgs["wedge"].StartIndexLocation;
	leftWedgeRitem->BaseVertexLocation = leftWedgeRitem->Geo->DrawArgs["wedge"].BaseVertexLocation;
	mAllRitems.push_back(std::move(leftWedgeRitem));

	// Create a wedge on the right side of the castle (Fence post)
	auto rightWedgeRitem = std::make_unique<RenderItem>();
	XMFLOAT3 rightWedgePosition = XMFLOAT3(castleWidth / 2 + 1.0f, wallHeight / 2, -castleDepth / 2 + 0.1f); // Adjusted position
	XMFLOAT3 rightWedgeScale = XMFLOAT3(0.5f, wallHeight, 0.5f); // Smaller wedge scale
	XMStoreFloat4x4(&rightWedgeRitem->World,
	XMMatrixScaling(rightWedgeScale.x, rightWedgeScale.y, rightWedgeScale.z) *
	XMMatrixTranslation(rightWedgePosition.x, rightWedgePosition.y, rightWedgePosition.z) *
	XMMatrixRotationY(XMConvertToRadians(15.0f))); // Rotate for fence-like slant
	rightWedgeRitem->ObjCBIndex = wallIndex++;
	rightWedgeRitem->Geo = mGeometries["shapeGeo"].get();
	rightWedgeRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	rightWedgeRitem->IndexCount = rightWedgeRitem->Geo->DrawArgs["wedge"].IndexCount;
	rightWedgeRitem->StartIndexLocation = rightWedgeRitem->Geo->DrawArgs["wedge"].StartIndexLocation;
	rightWedgeRitem->BaseVertexLocation = rightWedgeRitem->Geo->DrawArgs["wedge"].BaseVertexLocation;
	mAllRitems.push_back(std::move(rightWedgeRitem));

	// Create two toruses on the ground plane
	for (int i = 0; i < 2; ++i)
	{
		auto torusRitem = std::make_unique<RenderItem>();
		XMFLOAT3 torusPosition;
		XMFLOAT3 torusScale = XMFLOAT3(0.5f, 0.5f, 0.5f); // Scale for the torus

		// Set positions for the two toruses
		if (i == 0) // First torus
		{
			torusPosition = XMFLOAT3(-2.0f, -0.8f, -2.0f); // Adjust Y to be above the ground
		}
		else // Second torus
		{
			torusPosition = XMFLOAT3(2.0f, -0.8f, -2.0f); 
		}

		XMStoreFloat4x4(&torusRitem->World,
			XMMatrixScaling(torusScale.x, torusScale.y, torusScale.z) *
			XMMatrixTranslation(torusPosition.x, torusPosition.y, torusPosition.z));

		torusRitem->ObjCBIndex = wallIndex++;
		torusRitem->Geo = mGeometries["shapeGeo"].get();
		torusRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		torusRitem->IndexCount = torusRitem->Geo->DrawArgs["torus"].IndexCount;
		torusRitem->StartIndexLocation = torusRitem->Geo->DrawArgs["torus"].StartIndexLocation;
		torusRitem->BaseVertexLocation = torusRitem->Geo->DrawArgs["torus"].BaseVertexLocation;

		mAllRitems.push_back(std::move(torusRitem));

	}
	for (int i = 0; i < 2; ++i)
	{
		auto torusRitem = std::make_unique<RenderItem>();
		XMFLOAT3 torusPosition;
		XMFLOAT3 torusScale = XMFLOAT3(0.5f, 0.5f, 0.5f); // Scale for the torus

		// Set positions for the two toruses
		if (i == 0) // Third torus
		{
			torusPosition = XMFLOAT3(-2.0f, -0.8f, 2.0f); // Adjust Y to be above the ground
		}
		else // fourth torus
		{
			torusPosition = XMFLOAT3(2.0f, -0.8f, 2.0f); // Adjust Y to be above the ground
		}

		// Set the world matrix for the torus
		XMStoreFloat4x4(&torusRitem->World,
			XMMatrixScaling(torusScale.x, torusScale.y, torusScale.z) *
			XMMatrixTranslation(torusPosition.x, torusPosition.y, torusPosition.z));

		torusRitem->ObjCBIndex = wallIndex++; // Increment index
		torusRitem->Geo = mGeometries["shapeGeo"].get();
		torusRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		torusRitem->IndexCount = torusRitem->Geo->DrawArgs["torus"].IndexCount;
		torusRitem->StartIndexLocation = torusRitem->Geo->DrawArgs["torus"].StartIndexLocation;
		torusRitem->BaseVertexLocation = torusRitem->Geo->DrawArgs["torus"].BaseVertexLocation;

		mAllRitems.push_back(std::move(torusRitem));
	}


	//Create Pyramid
	auto pyramidItem = std::make_unique<RenderItem>();
	XMFLOAT3 pyramidPosition = XMFLOAT3(0.0f, 2.0f, 0.0f); // Position for the pyramid
	XMFLOAT3 pyramidScale = XMFLOAT3(2.6f, 1.0f, 2.6f); // Scale for the pyramid
	XMStoreFloat4x4(&pyramidItem->World,
	XMMatrixScaling(pyramidScale.x, pyramidScale.y, pyramidScale.z) *
	XMMatrixTranslation(pyramidPosition.x, pyramidPosition.y, pyramidPosition.z));
	pyramidItem->ObjCBIndex = wallIndex++; // Increment index
	pyramidItem->Geo = mGeometries["shapeGeo"].get();
	pyramidItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	pyramidItem->IndexCount = pyramidItem->Geo->DrawArgs["pyramid"].IndexCount;
	pyramidItem->StartIndexLocation = pyramidItem->Geo->DrawArgs["pyramid"].StartIndexLocation;
	pyramidItem->BaseVertexLocation = pyramidItem->Geo->DrawArgs["pyramid"].BaseVertexLocation;
	mAllRitems.push_back(std::move(pyramidItem));


	//Create Diamond
	auto diamondItem = std::make_unique<RenderItem>();
	XMFLOAT3 diamondPosition = XMFLOAT3(0.0f, 5.5f, 0.0f); // Position for the pyramid
	XMFLOAT3 diamondScale = XMFLOAT3(0.2f, -0.2f, 0.2f); // Scale for the pyramid
	XMStoreFloat4x4(&diamondItem->World,
		XMMatrixScaling(diamondScale.x, diamondScale.y, diamondScale.z) *
		XMMatrixTranslation(diamondPosition.x, diamondPosition.y, diamondPosition.z));
	diamondItem->ObjCBIndex = wallIndex++; // Increment index
	diamondItem->Geo = mGeometries["shapeGeo"].get();
	diamondItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	diamondItem->IndexCount = diamondItem->Geo->DrawArgs["diamond"].IndexCount;
	diamondItem->StartIndexLocation = diamondItem->Geo->DrawArgs["diamond"].StartIndexLocation;
	diamondItem->BaseVertexLocation = diamondItem->Geo->DrawArgs["diamond"].BaseVertexLocation;
	mAllRitems.push_back(std::move(diamondItem));

	auto diamond1Item = std::make_unique<RenderItem>();
	XMFLOAT3 diamond1Position = XMFLOAT3(0.0f, 5.48f, 0.0f); // Position for the pyramid
	XMFLOAT3 diamond1Scale = XMFLOAT3(0.2f, 0.2f, 0.2f); // Scale for the pyramid
	XMStoreFloat4x4(&diamond1Item->World,
		XMMatrixScaling(diamond1Scale.x, diamond1Scale.y, diamond1Scale.z) *
		XMMatrixTranslation(diamond1Position.x, diamond1Position.y, diamond1Position.z));
	diamond1Item->ObjCBIndex = wallIndex++; // Increment index
	diamond1Item->Geo = mGeometries["shapeGeo"].get();
	diamond1Item->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	diamond1Item->IndexCount = diamond1Item->Geo->DrawArgs["diamond1"].IndexCount;
	diamond1Item->StartIndexLocation = diamond1Item->Geo->DrawArgs["diamond1"].StartIndexLocation;
	diamond1Item->BaseVertexLocation = diamond1Item->Geo->DrawArgs["diamond1"].BaseVertexLocation;
	mAllRitems.push_back(std::move(diamond1Item));




	// Create ground plane using grid
	float gridYOffset = -1.0f;
	auto gridRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&gridRitem->World,
		XMMatrixTranslation(0.0f, gridYOffset, 0.0f)  // Position below castle
		* XMMatrixScaling(3.0f, 1.0f, 1.0f));  // Scale grid to cover area
	gridRitem->ObjCBIndex = (UINT)mAllRitems.size();
	gridRitem->Geo = mGeometries["shapeGeo"].get();
	gridRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
	mAllRitems.push_back(std::move(gridRitem));






	for (auto& e : mAllRitems)
		mOpaqueRitems.push_back(e.get());
}

void ShapesApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();

	// For each render item...
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		// Offset to the CBV in the descriptor heap for this object and for this frame resource.
		UINT cbvIndex = mCurrFrameResourceIndex * (UINT)mOpaqueRitems.size() + ri->ObjCBIndex;
		auto cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
		cbvHandle.Offset(cbvIndex, mCbvSrvUavDescriptorSize);

		cmdList->SetGraphicsRootDescriptorTable(0, cbvHandle);

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}



