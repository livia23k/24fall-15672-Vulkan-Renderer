#include "Helpers.hpp"

#include "RTG.hpp"
#include "VK.hpp"
#include "refsol.hpp"

#include <utility>
#include <cassert>
#include <cstring>
#include <iostream>

Helpers::Allocation::Allocation(Allocation &&from) {
	assert(handle == VK_NULL_HANDLE && offset == 0 && size == 0 && mapped == nullptr);

	std::swap(handle, from.handle);
	std::swap(size, from.size);
	std::swap(offset, from.offset);
	std::swap(mapped, from.mapped);
}

Helpers::Allocation &Helpers::Allocation::operator=(Allocation &&from) {
	if (!(handle == VK_NULL_HANDLE && offset == 0 && size == 0 && mapped == nullptr)) {
		//not fatal, just sloppy, so complain but don't throw:
		std::cerr << "Replacing a non-empty allocation; device memory will leak." << std::endl;
	}

	std::swap(handle, from.handle);
	std::swap(size, from.size);
	std::swap(offset, from.offset);
	std::swap(mapped, from.mapped);

	return *this;
}

Helpers::Allocation::~Allocation() {
	if (!(handle == VK_NULL_HANDLE && offset == 0 && size == 0 && mapped == nullptr)) {
		std::cerr << "Destructing a non-empty Allocation; device memory will leak." << std::endl;
	}
}

//----------------------------

Helpers::AllocatedBuffer Helpers::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, MapFlag map) {
	AllocatedBuffer buffer;
	refsol::Helpers_create_buffer(rtg, size, usage, properties, (map == Mapped), &buffer);
	return buffer;
}

void Helpers::destroy_buffer(AllocatedBuffer &&buffer) {
	refsol::Helpers_destroy_buffer(rtg, &buffer);
}


Helpers::AllocatedImage Helpers::create_image(VkExtent2D const &extent, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, MapFlag map) {
	AllocatedImage image;
	refsol::Helpers_create_image(rtg, extent, format, tiling, usage, properties, (map == Mapped), &image);
	return image;
}

void Helpers::destroy_image(AllocatedImage &&image) {
	refsol::Helpers_destroy_image(rtg, &image);
}

//----------------------------

void Helpers::transfer_to_buffer(void *data, size_t size, AllocatedBuffer &target) {
	// Edit Start =========================================================================================================
	// refsol::Helpers_transfer_to_buffer(rtg, data, size, &target);
	
	AllocatedBuffer transfer_src = create_buffer(
		size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, //host visible memory, coherent (no special sync needed)
		Mapped
	);

	//DONE: copy data to transfer buffer
	std::memcpy(transfer_src.allocation.data(), data, size);

	//DONE: record CPU->GPU transfer to command buffer
	{
		VK( vkResetCommandBuffer(transfer_command_buffer, 0) );

		VkCommandBufferBeginInfo begin_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT //record again every submit
		};

		VK( vkBeginCommandBuffer(transfer_command_buffer, &begin_info) );

		VkBufferCopy copy_region{
			.srcOffset = 0,
			.dstOffset = 0,
			.size = size
		};
		
		vkCmdCopyBuffer(transfer_command_buffer, transfer_src.handle, target.handle, 1, &copy_region);

		VK( vkEndCommandBuffer(transfer_command_buffer) );
	}

	//DONE: run command buffer
	{
		VkSubmitInfo submit_info{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.commandBufferCount = 1,
			.pCommandBuffers = &transfer_command_buffer
		};
		VK( vkQueueSubmit(rtg.graphics_queue, 1, &submit_info, VK_NULL_HANDLE) );
	}

	//DONE: wait for command buffer to finish
	VK( vkQueueWaitIdle(rtg.graphics_queue) );

	//destroy to avoid buffer memory leaking
	destroy_buffer(std::move(transfer_src));
	//Edit End ===========================================================================================================
}

void Helpers::transfer_to_image(void *data, size_t size, AllocatedImage &target) {
	refsol::Helpers_transfer_to_image(rtg, data, size, &target);
}

//----------------------------

VkFormat Helpers::find_image_format(std::vector< VkFormat > const &candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const {
	return refsol::Helpers_find_image_format(rtg, candidates, tiling, features);
}

VkShaderModule Helpers::create_shader_module(uint32_t const *code, size_t bytes) const {
	VkShaderModule shader_module = VK_NULL_HANDLE;
	refsol::Helpers_create_shader_module(rtg, code, bytes, &shader_module);
	return shader_module;
}

//----------------------------

Helpers::Helpers(RTG const &rtg_) : rtg(rtg_) {
}

Helpers::~Helpers() {
}

void Helpers::create() {
	//Edit Start =========================================================================================================
	VkCommandPoolCreateInfo create_info{ //resources made with create and destroy are long-lived
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = rtg.graphics_queue_family.value()
	};
	VK( vkCreateCommandPool(rtg.device, &create_info, nullptr, &transfer_command_pool) );

	VkCommandBufferAllocateInfo alloc_info{ //resources made with alloc and free are short-lived, can be changed each frame
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = transfer_command_pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, 
		.commandBufferCount = 1
	};
	VK( vkAllocateCommandBuffers(rtg.device, &alloc_info, &transfer_command_buffer) );
	//Edit End ===========================================================================================================
}

void Helpers::destroy() {
	//Edit Start =========================================================================================================	
	if (transfer_command_buffer != VK_NULL_HANDLE) {
		vkFreeCommandBuffers(rtg.device, transfer_command_pool, 1, &transfer_command_buffer);
		transfer_command_buffer = VK_NULL_HANDLE;
	}

	if (transfer_command_pool != VK_NULL_HANDLE) {
		vkDestroyCommandPool(rtg.device, transfer_command_pool, nullptr);
		transfer_command_pool = VK_NULL_HANDLE;
	}
	//Edit End ===========================================================================================================
}
