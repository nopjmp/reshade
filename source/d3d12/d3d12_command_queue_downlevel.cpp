/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "d3d12_device.hpp"
#include "d3d12_command_list.hpp"
#include "d3d12_command_queue.hpp"
#include "d3d12_command_queue_downlevel.hpp"
#include "runtime_d3d12.hpp"

D3D12CommandQueueDownlevel::D3D12CommandQueueDownlevel(D3D12CommandQueue *queue, ID3D12CommandQueueDownlevel *original) :
	_orig(original),
	_parent_queue(queue),
	_runtime(new reshade::d3d12::runtime_d3d12(queue->_device, queue, nullptr))
{
	assert(_orig != nullptr && _parent_queue != nullptr);
}

HRESULT STDMETHODCALLTYPE D3D12CommandQueueDownlevel::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
		return E_POINTER;

	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(ID3D12CommandQueueDownlevel))
	{
		AddRef();
		*ppvObj = this;
		return S_OK;
	}

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE D3D12CommandQueueDownlevel::AddRef()
{
	_orig->AddRef();
	return InterlockedIncrement(&_ref);
}
ULONG   STDMETHODCALLTYPE D3D12CommandQueueDownlevel::Release()
{
	const ULONG ref = InterlockedDecrement(&_ref);
	if (ref != 0)
		return _orig->Release(), ref;

	// Delete runtime first to release all internal references to device objects
	delete _runtime;

	const auto orig = _orig;
#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Destroying ID3D12CommandQueueDownlevel object " << this << " (" << orig << ").";
#endif
	delete this;

	// Only release internal reference after the runtime has been destroyed, so any references it held are cleaned up at this point
	const ULONG ref_orig = orig->Release();
	if (ref_orig > 1) // Verify internal reference count against one instead of zero because parent queue still holds a reference
		LOG(WARN) << "Reference count for ID3D12CommandQueueDownlevel object " << this << " (" << orig << ") is inconsistent.";
	return 0;
}

HRESULT STDMETHODCALLTYPE D3D12CommandQueueDownlevel::Present(ID3D12GraphicsCommandList *pOpenCommandList, ID3D12Resource *pSourceTex2D, HWND hWindow, D3D12_DOWNLEVEL_PRESENT_FLAGS Flags)
{
	RESHADE_ADDON_EVENT(present, _parent_queue, _runtime);

	assert(pSourceTex2D != nullptr);
	_runtime->on_present(pSourceTex2D, hWindow);

	// Get original command list pointer from proxy object
	if (com_ptr<D3D12GraphicsCommandList> command_list_proxy;
		SUCCEEDED(pOpenCommandList->QueryInterface(&command_list_proxy)))
		pOpenCommandList = command_list_proxy->_orig;

	return _orig->Present(pOpenCommandList, pSourceTex2D, hWindow, Flags);
}
