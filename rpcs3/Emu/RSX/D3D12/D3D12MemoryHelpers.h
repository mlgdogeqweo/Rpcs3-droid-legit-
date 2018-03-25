#pragma once
#include "D3D12Utils.h"
#include "d3dx12.h"
#include "../Common/ring_buffer_helper.h"
#include <list>
#include <mutex>

struct d3d12_data_heap : public data_heap
{
	ComPtr<ID3D12Resource> m_heap;
public:
	d3d12_data_heap() = default;
	~d3d12_data_heap() = default;
	d3d12_data_heap(const d3d12_data_heap&) = delete;
	d3d12_data_heap(d3d12_data_heap&&) = delete;

	template <typename... arg_type>
	void init(ID3D12Device *device, size_t heap_size, D3D12_HEAP_TYPE type, D3D12_RESOURCE_STATES state)
	{
		m_size = heap_size;
		m_put_pos = 0;
		m_get_pos = heap_size - 1;

		D3D12_HEAP_PROPERTIES heap_properties = {};
		heap_properties.Type = type;
		CHECK_HRESULT(device->CreateCommittedResource(&heap_properties,
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(heap_size),
			state,
			nullptr,
			IID_PPV_ARGS(m_heap.GetAddressOf()))
			);
	}

	template<typename T>
	T* map(const D3D12_RANGE &range)
	{
		void *buffer;
		CHECK_HRESULT(m_heap->Map(0, &range, &buffer));
		void *mapped_buffer = (char*)buffer + range.Begin;
		return static_cast<T*>(mapped_buffer);
	}

	template<typename T>
	T* map(size_t heap_offset)
	{
		void *buffer;
		CHECK_HRESULT(m_heap->Map(0, nullptr, &buffer));
		void *mapped_buffer = (char*)buffer + heap_offset;
		return static_cast<T*>(mapped_buffer);
	}

	void unmap(const D3D12_RANGE &range)
	{
		m_heap->Unmap(0, &range);
	}

	void unmap()
	{
		m_heap->Unmap(0, nullptr);
	}

	ID3D12Resource* get_heap()
	{
		return m_heap.Get();
	}
};

struct texture_entry
{
	u8 m_format;
	bool m_is_dirty;
	size_t m_width;
	size_t m_height;
	size_t m_mipmap;
	size_t m_depth;

	texture_entry() : m_format(0), m_width(0), m_height(0), m_depth(0), m_is_dirty(true)
	{}

	texture_entry(u8 f, size_t w, size_t h, size_t d, size_t m) : m_format(f), m_width(w), m_height(h), m_depth(d), m_is_dirty(false), m_mipmap(m)
	{}

	bool operator==(const texture_entry &other)
	{
		return (m_format == other.m_format && m_width == other.m_width && m_height == other.m_height && m_mipmap == other.m_mipmap && m_depth == other.m_depth);
	}
};

/**
* Manages cache of data (texture/vertex/index)
*/
struct data_cache
{
private:
	/**
	* Mutex protecting m_dataCache access
	* Memory protection fault catch can be generated by any thread and
	* modifies it.
	*/
	shared_mutex m_mut;

	std::unordered_map<u64, std::pair<texture_entry, ComPtr<ID3D12Resource>> > m_address_to_data; // Storage
	std::list <std::tuple<u64, u32, u32> > m_protected_ranges; // address, start of protected range, size of protected range
public:
	data_cache() = default;
	~data_cache() = default;
	data_cache(const data_cache&) = delete;
	data_cache(data_cache&&) = delete;

	void store_and_protect_data(u64 key, u32 start, size_t size, u8 format, size_t w, size_t h, size_t d, size_t m, ComPtr<ID3D12Resource> data);

	/**
	* Make memory from start to start + size write protected.
	* Associate key to this range so that when a write is detected, data at key is marked dirty.
	*/
	void protect_data(u64 key, u32 start, size_t size);

	/**
	 * Remove all data containing addr from cache, unprotect them. Returns false if no data is modified.
	 */
	bool invalidate_address(u32 addr);

	std::pair<texture_entry, ComPtr<ID3D12Resource> > *find_data_if_available(u64 key);

	void unprotect_all();

	/**
	* Remove data stored at key, and returns a ComPtr owning it.
	* The caller is responsible for releasing the ComPtr.
	*/
	ComPtr<ID3D12Resource> remove_from_cache(u64 key);
};

/**
* Stores data that are "ping ponged" between frame.
* For instance command allocator : maintains 2 command allocators and
* swap between them when frame is flipped.
*/
struct resource_storage
{
	resource_storage() = default;
	~resource_storage() = default;
	resource_storage(const resource_storage&) = delete;
	resource_storage(resource_storage&&) = delete;

	bool in_use; // False until command list has been populated at least once
	ComPtr<ID3D12Fence> frame_finished_fence;
	UINT64 fence_value;
	HANDLE frame_finished_handle;

	// Pointer to device, not owned by ResourceStorage
	ID3D12Device *m_device;
	ComPtr<ID3D12CommandAllocator> command_allocator;
	ComPtr<ID3D12GraphicsCommandList> command_list;

	// Descriptor heap
	ComPtr<ID3D12DescriptorHeap> descriptors_heap;
	size_t descriptors_heap_index;

	// Sampler heap
	ComPtr<ID3D12DescriptorHeap> sampler_descriptor_heap[2];
	size_t sampler_descriptors_heap_index;
	size_t current_sampler_index;

	size_t render_targets_descriptors_heap_index;
	ComPtr<ID3D12DescriptorHeap> render_targets_descriptors_heap;
	size_t depth_stencil_descriptor_heap_index;
	ComPtr<ID3D12DescriptorHeap> depth_stencil_descriptor_heap;

	ComPtr<ID3D12Resource> ram_framebuffer;

	/// Texture that were invalidated
	std::list<ComPtr<ID3D12Resource> > dirty_textures;

	/**
	 * Start position in heaps of resources used for this frame.
	 * This means newer resources shouldn't allocate memory crossing this position
	 * until the frame rendering is over.
	 */
	size_t buffer_heap_get_pos;
	size_t readback_heap_get_pos;

	void reset();
	void init(ID3D12Device *device);
	void set_new_command_list();
	void wait_and_clean();
	void release();
};
