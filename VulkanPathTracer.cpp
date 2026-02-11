#include "VulkanPathTracer.h"

#include "VulkanPathTracerSpv.h"

#ifdef ENABLE_VULKAN_COMPUTE
#include <vulkan/vulkan.h>
#endif

#include <algorithm>
#include <cstring>

#ifdef ENABLE_VULKAN_COMPUTE
struct VulkanPathTracer::Impl {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    uint32_t queueFamilyIndex = 0;
    VkQueue queue = VK_NULL_HANDLE;

    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;

    VkImage accumImage = VK_NULL_HANDLE;
    VkImage outputImage = VK_NULL_HANDLE;
    VkDeviceMemory accumMemory = VK_NULL_HANDLE;
    VkDeviceMemory outputMemory = VK_NULL_HANDLE;
    VkImageView accumImageView = VK_NULL_HANDLE;
    VkImageView outputImageView = VK_NULL_HANDLE;

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    void *stagingMapped = nullptr;

    VkShaderModule shaderModule = VK_NULL_HANDLE;
};
#endif

VulkanPathTracer::VulkanPathTracer() {
#ifdef ENABLE_VULKAN_COMPUTE
    m_impl = new Impl;
#endif
}

VulkanPathTracer::~VulkanPathTracer() {
    cleanup();
#ifdef ENABLE_VULKAN_COMPUTE
    delete m_impl;
    m_impl = nullptr;
#endif
}

bool VulkanPathTracer::initialize(int width, int height) {
    m_width = width;
    m_height = height;
    m_frameIndex = 0;
    m_hostOutput.assign(static_cast<size_t>(width) * static_cast<size_t>(height), 0u);

#ifdef ENABLE_VULKAN_COMPUTE
    return initializeInternal(width, height);
#else
    Q_UNUSED(width)
    Q_UNUSED(height)
    m_lastError = QStringLiteral("Vulkan compute backend is not enabled in this build");
    return false;
#endif
}

bool VulkanPathTracer::renderFrame(int maxDepth) {
#ifdef ENABLE_VULKAN_COMPUTE
    if (!m_impl || m_impl->device == VK_NULL_HANDLE || m_impl->pipeline == VK_NULL_HANDLE) {
        m_lastError = QStringLiteral("Vulkan compute is not initialized");
        return false;
    }

    struct PushConstants {
        int width;
        int height;
        int frameIndex;
        int maxDepth;
    } pc{m_width, m_height, m_frameIndex, std::clamp(maxDepth, 1, 64)};

    vkResetFences(m_impl->device, 1, &m_impl->fence);
    vkResetCommandBuffer(m_impl->commandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(m_impl->commandBuffer, &beginInfo);

    VkImageMemoryBarrier toGeneral[2] = {};
    for (int i = 0; i < 2; ++i) {
        toGeneral[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toGeneral[i].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        toGeneral[i].newLayout = VK_IMAGE_LAYOUT_GENERAL;
        toGeneral[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toGeneral[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toGeneral[i].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        toGeneral[i].subresourceRange.baseMipLevel = 0;
        toGeneral[i].subresourceRange.levelCount = 1;
        toGeneral[i].subresourceRange.baseArrayLayer = 0;
        toGeneral[i].subresourceRange.layerCount = 1;
        toGeneral[i].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        toGeneral[i].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
    }
    toGeneral[0].image = m_impl->accumImage;
    toGeneral[1].image = m_impl->outputImage;

    vkCmdPipelineBarrier(
        m_impl->commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        2,
        toGeneral);

    vkCmdBindPipeline(m_impl->commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_impl->pipeline);
    vkCmdBindDescriptorSets(
        m_impl->commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        m_impl->pipelineLayout,
        0,
        1,
        &m_impl->descriptorSet,
        0,
        nullptr);
    vkCmdPushConstants(m_impl->commandBuffer, m_impl->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants), &pc);

    const uint32_t gx = static_cast<uint32_t>((m_width + 7) / 8);
    const uint32_t gy = static_cast<uint32_t>((m_height + 7) / 8);
    vkCmdDispatch(m_impl->commandBuffer, gx, gy, 1);

    VkImageMemoryBarrier toTransfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toTransfer.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = m_impl->outputImage;
    toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toTransfer.subresourceRange.baseMipLevel = 0;
    toTransfer.subresourceRange.levelCount = 1;
    toTransfer.subresourceRange.baseArrayLayer = 0;
    toTransfer.subresourceRange.layerCount = 1;
    toTransfer.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(
        m_impl->commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &toTransfer);

    VkBufferImageCopy copyRegion{};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = static_cast<uint32_t>(m_width);
    copyRegion.bufferImageHeight = static_cast<uint32_t>(m_height);
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent = {static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height), 1};

    vkCmdCopyImageToBuffer(
        m_impl->commandBuffer,
        m_impl->outputImage,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        m_impl->stagingBuffer,
        1,
        &copyRegion);

    VkImageMemoryBarrier toGeneralAfterCopy{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toGeneralAfterCopy.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toGeneralAfterCopy.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    toGeneralAfterCopy.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toGeneralAfterCopy.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toGeneralAfterCopy.image = m_impl->outputImage;
    toGeneralAfterCopy.subresourceRange = toTransfer.subresourceRange;
    toGeneralAfterCopy.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    toGeneralAfterCopy.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

    vkCmdPipelineBarrier(
        m_impl->commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &toGeneralAfterCopy);

    VkBufferMemoryBarrier hostVisible{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    hostVisible.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    hostVisible.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    hostVisible.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    hostVisible.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    hostVisible.buffer = m_impl->stagingBuffer;
    hostVisible.offset = 0;
    hostVisible.size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(
        m_impl->commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_HOST_BIT,
        0,
        0,
        nullptr,
        1,
        &hostVisible,
        0,
        nullptr);

    vkEndCommandBuffer(m_impl->commandBuffer);

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_impl->commandBuffer;
    vkQueueSubmit(m_impl->queue, 1, &submitInfo, m_impl->fence);
    vkWaitForFences(m_impl->device, 1, &m_impl->fence, VK_TRUE, UINT64_MAX);

    std::memcpy(
        m_hostOutput.data(),
        m_impl->stagingMapped,
        static_cast<size_t>(m_width) * static_cast<size_t>(m_height) * sizeof(unsigned int));

    ++m_frameIndex;
    return true;
#else
    Q_UNUSED(maxDepth)
    m_lastError = QStringLiteral("Vulkan compute backend is not enabled in this build");
    return false;
#endif
}

const unsigned int *VulkanPathTracer::hostPixels() const {
    return m_hostOutput.data();
}

int VulkanPathTracer::frameIndex() const {
    return m_frameIndex;
}

QString VulkanPathTracer::lastError() const {
    return m_lastError;
}

#ifdef ENABLE_VULKAN_COMPUTE
static uint32_t findMemoryType(VkPhysicalDevice pd, uint32_t typeBits, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(pd, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeBits & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    return UINT32_MAX;
}

bool VulkanPathTracer::initializeInternal(int width, int height) {
    cleanup();

    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName = "Qt RayTracer Vulkan Compute";
    appInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo instanceInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instanceInfo.pApplicationInfo = &appInfo;
    if (vkCreateInstance(&instanceInfo, nullptr, &m_impl->instance) != VK_SUCCESS) {
        m_lastError = QStringLiteral("vkCreateInstance failed");
        return false;
    }

    uint32_t pdCount = 0;
    vkEnumeratePhysicalDevices(m_impl->instance, &pdCount, nullptr);
    if (pdCount == 0) {
        m_lastError = QStringLiteral("No Vulkan physical device found");
        return false;
    }
    std::vector<VkPhysicalDevice> pds(pdCount);
    vkEnumeratePhysicalDevices(m_impl->instance, &pdCount, pds.data());
    m_impl->physicalDevice = pds[0];

    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_impl->physicalDevice, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> qProps(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_impl->physicalDevice, &qCount, qProps.data());

    bool foundCompute = false;
    for (uint32_t i = 0; i < qCount; ++i) {
        if (qProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            m_impl->queueFamilyIndex = i;
            foundCompute = true;
            break;
        }
    }
    if (!foundCompute) {
        m_lastError = QStringLiteral("No Vulkan compute queue family found");
        return false;
    }

    float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueInfo.queueFamilyIndex = m_impl->queueFamilyIndex;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;

    VkDeviceCreateInfo dInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dInfo.queueCreateInfoCount = 1;
    dInfo.pQueueCreateInfos = &queueInfo;
    if (vkCreateDevice(m_impl->physicalDevice, &dInfo, nullptr, &m_impl->device) != VK_SUCCESS) {
        m_lastError = QStringLiteral("vkCreateDevice failed");
        return false;
    }
    vkGetDeviceQueue(m_impl->device, m_impl->queueFamilyIndex, 0, &m_impl->queue);

    if (!createPipeline()) {
        return false;
    }
    if (!createImagesAndBuffers()) {
        return false;
    }
    if (!recordAndSubmitInitClear()) {
        return false;
    }

    m_width = width;
    m_height = height;
    m_frameIndex = 0;
    return true;
}

bool VulkanPathTracer::createPipeline() {
    VkShaderModuleCreateInfo shaderInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    shaderInfo.codeSize = kPathtraceVulkanSpvSize;
    shaderInfo.pCode = kPathtraceVulkanSpv;
    if (vkCreateShaderModule(m_impl->device, &shaderInfo, nullptr, &m_impl->shaderModule) != VK_SUCCESS) {
        m_lastError = QStringLiteral("vkCreateShaderModule failed");
        return false;
    }

    VkDescriptorSetLayoutBinding bindings[2] = {};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo dslInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dslInfo.bindingCount = 2;
    dslInfo.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(m_impl->device, &dslInfo, nullptr, &m_impl->descriptorSetLayout) != VK_SUCCESS) {
        m_lastError = QStringLiteral("vkCreateDescriptorSetLayout failed");
        return false;
    }

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(int) * 4;

    VkPipelineLayoutCreateInfo plInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plInfo.setLayoutCount = 1;
    plInfo.pSetLayouts = &m_impl->descriptorSetLayout;
    plInfo.pushConstantRangeCount = 1;
    plInfo.pPushConstantRanges = &pushRange;
    if (vkCreatePipelineLayout(m_impl->device, &plInfo, nullptr, &m_impl->pipelineLayout) != VK_SUCCESS) {
        m_lastError = QStringLiteral("vkCreatePipelineLayout failed");
        return false;
    }

    VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = m_impl->shaderModule;
    stage.pName = "main";

    VkComputePipelineCreateInfo cpInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    cpInfo.stage = stage;
    cpInfo.layout = m_impl->pipelineLayout;

    if (vkCreateComputePipelines(m_impl->device, VK_NULL_HANDLE, 1, &cpInfo, nullptr, &m_impl->pipeline) != VK_SUCCESS) {
        m_lastError = QStringLiteral("vkCreateComputePipelines failed");
        return false;
    }

    return true;
}

bool VulkanPathTracer::createImagesAndBuffers() {
    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height), 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    imageInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    if (vkCreateImage(m_impl->device, &imageInfo, nullptr, &m_impl->accumImage) != VK_SUCCESS) {
        m_lastError = QStringLiteral("vkCreateImage(accum) failed");
        return false;
    }

    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    if (vkCreateImage(m_impl->device, &imageInfo, nullptr, &m_impl->outputImage) != VK_SUCCESS) {
        m_lastError = QStringLiteral("vkCreateImage(output) failed");
        return false;
    }

    VkMemoryRequirements accumReq;
    vkGetImageMemoryRequirements(m_impl->device, m_impl->accumImage, &accumReq);
    VkMemoryRequirements outReq;
    vkGetImageMemoryRequirements(m_impl->device, m_impl->outputImage, &outReq);

    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = accumReq.size;
    allocInfo.memoryTypeIndex = findMemoryType(m_impl->physicalDevice, accumReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (allocInfo.memoryTypeIndex == UINT32_MAX || vkAllocateMemory(m_impl->device, &allocInfo, nullptr, &m_impl->accumMemory) != VK_SUCCESS) {
        m_lastError = QStringLiteral("vkAllocateMemory(accum) failed");
        return false;
    }
    vkBindImageMemory(m_impl->device, m_impl->accumImage, m_impl->accumMemory, 0);

    allocInfo.allocationSize = outReq.size;
    allocInfo.memoryTypeIndex = findMemoryType(m_impl->physicalDevice, outReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (allocInfo.memoryTypeIndex == UINT32_MAX || vkAllocateMemory(m_impl->device, &allocInfo, nullptr, &m_impl->outputMemory) != VK_SUCCESS) {
        m_lastError = QStringLiteral("vkAllocateMemory(output) failed");
        return false;
    }
    vkBindImageMemory(m_impl->device, m_impl->outputImage, m_impl->outputMemory, 0);

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    viewInfo.image = m_impl->accumImage;
    viewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    if (vkCreateImageView(m_impl->device, &viewInfo, nullptr, &m_impl->accumImageView) != VK_SUCCESS) {
        m_lastError = QStringLiteral("vkCreateImageView(accum) failed");
        return false;
    }

    viewInfo.image = m_impl->outputImage;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    if (vkCreateImageView(m_impl->device, &viewInfo, nullptr, &m_impl->outputImageView) != VK_SUCCESS) {
        m_lastError = QStringLiteral("vkCreateImageView(output) failed");
        return false;
    }

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSize.descriptorCount = 2;

    VkDescriptorPoolCreateInfo dpInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpInfo.maxSets = 1;
    dpInfo.poolSizeCount = 1;
    dpInfo.pPoolSizes = &poolSize;
    if (vkCreateDescriptorPool(m_impl->device, &dpInfo, nullptr, &m_impl->descriptorPool) != VK_SUCCESS) {
        m_lastError = QStringLiteral("vkCreateDescriptorPool failed");
        return false;
    }

    VkDescriptorSetAllocateInfo dsAlloc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dsAlloc.descriptorPool = m_impl->descriptorPool;
    dsAlloc.descriptorSetCount = 1;
    dsAlloc.pSetLayouts = &m_impl->descriptorSetLayout;
    if (vkAllocateDescriptorSets(m_impl->device, &dsAlloc, &m_impl->descriptorSet) != VK_SUCCESS) {
        m_lastError = QStringLiteral("vkAllocateDescriptorSets failed");
        return false;
    }

    VkDescriptorImageInfo accumInfo{};
    accumInfo.imageView = m_impl->accumImageView;
    accumInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkDescriptorImageInfo outInfo{};
    outInfo.imageView = m_impl->outputImageView;
    outInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet writes[2] = {};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = m_impl->descriptorSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[0].descriptorCount = 1;
    writes[0].pImageInfo = &accumInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_impl->descriptorSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &outInfo;

    vkUpdateDescriptorSets(m_impl->device, 2, writes, 0, nullptr);

    VkCommandPoolCreateInfo cpInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpInfo.queueFamilyIndex = m_impl->queueFamilyIndex;
    cpInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCreateCommandPool(m_impl->device, &cpInfo, nullptr, &m_impl->commandPool);

    VkCommandBufferAllocateInfo cbAlloc{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbAlloc.commandPool = m_impl->commandPool;
    cbAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbAlloc.commandBufferCount = 1;
    vkAllocateCommandBuffers(m_impl->device, &cbAlloc, &m_impl->commandBuffer);

    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    vkCreateFence(m_impl->device, &fenceInfo, nullptr, &m_impl->fence);

    const VkDeviceSize stagingSize = static_cast<VkDeviceSize>(m_width) * static_cast<VkDeviceSize>(m_height) * 4u;
    VkBufferCreateInfo bufInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufInfo.size = stagingSize;
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(m_impl->device, &bufInfo, nullptr, &m_impl->stagingBuffer);

    VkMemoryRequirements bufReq;
    vkGetBufferMemoryRequirements(m_impl->device, m_impl->stagingBuffer, &bufReq);
    VkMemoryAllocateInfo bufAlloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    bufAlloc.allocationSize = bufReq.size;
    bufAlloc.memoryTypeIndex = findMemoryType(
        m_impl->physicalDevice,
        bufReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (bufAlloc.memoryTypeIndex == UINT32_MAX || vkAllocateMemory(m_impl->device, &bufAlloc, nullptr, &m_impl->stagingMemory) != VK_SUCCESS) {
        m_lastError = QStringLiteral("vkAllocateMemory(staging) failed");
        return false;
    }
    vkBindBufferMemory(m_impl->device, m_impl->stagingBuffer, m_impl->stagingMemory, 0);
    vkMapMemory(m_impl->device, m_impl->stagingMemory, 0, VK_WHOLE_SIZE, 0, &m_impl->stagingMapped);

    return true;
}

bool VulkanPathTracer::recordAndSubmitInitClear() {
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(m_impl->commandBuffer, &begin);

    VkImageMemoryBarrier toGeneral[2] = {};
    for (int i = 0; i < 2; ++i) {
        toGeneral[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toGeneral[i].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        toGeneral[i].newLayout = VK_IMAGE_LAYOUT_GENERAL;
        toGeneral[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toGeneral[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toGeneral[i].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        toGeneral[i].subresourceRange.levelCount = 1;
        toGeneral[i].subresourceRange.layerCount = 1;
        toGeneral[i].srcAccessMask = 0;
        toGeneral[i].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    }
    toGeneral[0].image = m_impl->accumImage;
    toGeneral[1].image = m_impl->outputImage;

    vkCmdPipelineBarrier(
        m_impl->commandBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        2,
        toGeneral);

    VkClearColorValue clear{};
    vkCmdClearColorImage(
        m_impl->commandBuffer,
        m_impl->accumImage,
        VK_IMAGE_LAYOUT_GENERAL,
        &clear,
        1,
        &toGeneral[0].subresourceRange);
    vkCmdClearColorImage(
        m_impl->commandBuffer,
        m_impl->outputImage,
        VK_IMAGE_LAYOUT_GENERAL,
        &clear,
        1,
        &toGeneral[1].subresourceRange);

    vkEndCommandBuffer(m_impl->commandBuffer);

    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &m_impl->commandBuffer;
    vkQueueSubmit(m_impl->queue, 1, &submit, m_impl->fence);
    vkWaitForFences(m_impl->device, 1, &m_impl->fence, VK_TRUE, UINT64_MAX);
    vkResetFences(m_impl->device, 1, &m_impl->fence);
    vkResetCommandBuffer(m_impl->commandBuffer, 0);

    return true;
}

void VulkanPathTracer::cleanup() {
    if (!m_impl) {
        return;
    }
    if (m_impl->device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_impl->device);
    }

    if (m_impl->stagingMapped) {
        vkUnmapMemory(m_impl->device, m_impl->stagingMemory);
        m_impl->stagingMapped = nullptr;
    }
    if (m_impl->stagingBuffer) vkDestroyBuffer(m_impl->device, m_impl->stagingBuffer, nullptr);
    if (m_impl->stagingMemory) vkFreeMemory(m_impl->device, m_impl->stagingMemory, nullptr);

    if (m_impl->accumImageView) vkDestroyImageView(m_impl->device, m_impl->accumImageView, nullptr);
    if (m_impl->outputImageView) vkDestroyImageView(m_impl->device, m_impl->outputImageView, nullptr);
    if (m_impl->accumImage) vkDestroyImage(m_impl->device, m_impl->accumImage, nullptr);
    if (m_impl->outputImage) vkDestroyImage(m_impl->device, m_impl->outputImage, nullptr);
    if (m_impl->accumMemory) vkFreeMemory(m_impl->device, m_impl->accumMemory, nullptr);
    if (m_impl->outputMemory) vkFreeMemory(m_impl->device, m_impl->outputMemory, nullptr);

    if (m_impl->fence) vkDestroyFence(m_impl->device, m_impl->fence, nullptr);
    if (m_impl->commandPool) vkDestroyCommandPool(m_impl->device, m_impl->commandPool, nullptr);

    if (m_impl->descriptorPool) vkDestroyDescriptorPool(m_impl->device, m_impl->descriptorPool, nullptr);
    if (m_impl->pipeline) vkDestroyPipeline(m_impl->device, m_impl->pipeline, nullptr);
    if (m_impl->pipelineLayout) vkDestroyPipelineLayout(m_impl->device, m_impl->pipelineLayout, nullptr);
    if (m_impl->descriptorSetLayout) vkDestroyDescriptorSetLayout(m_impl->device, m_impl->descriptorSetLayout, nullptr);
    if (m_impl->shaderModule) vkDestroyShaderModule(m_impl->device, m_impl->shaderModule, nullptr);

    if (m_impl->device) vkDestroyDevice(m_impl->device, nullptr);
    if (m_impl->instance) vkDestroyInstance(m_impl->instance, nullptr);

    *m_impl = Impl{};
}
#else
bool VulkanPathTracer::initializeInternal(int width, int height) {
    Q_UNUSED(width)
    Q_UNUSED(height)
    return false;
}

bool VulkanPathTracer::createPipeline() { return false; }
bool VulkanPathTracer::createImagesAndBuffers() { return false; }
bool VulkanPathTracer::recordAndSubmitInitClear() { return false; }
void VulkanPathTracer::cleanup() {}
#endif
