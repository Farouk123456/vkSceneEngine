#include "common.h"

_random RandomGenerator;

double millis()
{
    return glfwGetTime() * 1000;
}

std::string timestamp()
{
    return ((int)millis() < 10000) ? std::to_string((int)millis()) + "ms" : std::to_string((int)(millis() * 1e-3)) + "s";
}

// Map a float value from one range to another
float mapRange(float value, float inMin, float inMax, float outMin, float outMax)
{
    return outMin + (outMax - outMin) * ((value - inMin) / (inMax - inMin));
}

std::vector<char> readFile(const std::string &filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        LOG_FATAL("failed to find file: " + filename);
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

std::u32string trim(const std::u32string& str)
{
    const std::u32string whitespace = U" \t\n";
    const auto first = str.find_first_not_of(whitespace);
    if (first == std::u32string::npos) return U""; // String is all whitespace

    const auto last = str.find_last_not_of(whitespace);
    return str.substr(first, (last - first) + 1);
}

std::u32string trim(std::u32string_view str)
{
    const std::u32string whitespace = U" \t\n";
    const auto first = str.find_first_not_of(whitespace);
    if (first == std::u32string::npos) return U""; // String is all whitespace

    const auto last = str.find_last_not_of(whitespace);
    return std::u32string(str.begin(), str.end()).substr(first, (last - first) + 1);
}

std::vector<std::u32string> split_on_ascii(std::u32string_view str)
{
    std::vector<std::u32string> res;

    int last = 0;
    for (int i = 1; i < str.length(); i++)
    {
        if ((str[i] < 0x80) != (str[i-1] < 0x80))
        {
            res.emplace_back(str.begin() + last, str.begin() + i);
            last = i;
        }
    }

    res.emplace_back(str.begin() + last, str.end());
    return res;
}

std::vector<std::u32string> split_u32string_on_newline_andtrim(const std::u32string& str)
{
    std::vector<std::u32string> result;
    size_t start = 0;
    size_t pos = 0;

    while (pos < str.length()) {
        if (str[pos] == U'\n') {
            result.push_back(trim(str.substr(start, pos - start)));
            start = pos + 1;
        }
        pos++;
    }
    // Add the last segment if it doesn't end with a newline
    if (start < str.length()) {
        result.push_back(trim(str.substr(start)));
    }

    return result;
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void CmdSetPolygonModeEXT(VkInstance instance, VkCommandBuffer commandBuffer, VkPolygonMode polygonMode)
{
    auto func = (PFN_vkCmdSetPolygonModeEXT)vkGetInstanceProcAddr(instance, "vkCmdSetPolygonModeEXT");
    if (func != nullptr)
    {
        func(commandBuffer, polygonMode);
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks *pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        func(instance, debugMessenger, pAllocator);
    }
}

VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool commandPool)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void endSingleTimeCommands(VkDevice device, VkQueue queue, VkCommandBuffer commandBuffer, VkCommandPool commandPool)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void transitionImageLayout(uint32_t baseMip, uint32_t mipCount, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool, VkCommandBuffer commandBuffer)
{
    //------------------------------------
    // Optional one-time command buffer
    //------------------------------------
    bool createdCmd = (commandBuffer == VK_NULL_HANDLE);

    if (createdCmd)
        commandBuffer = beginSingleTimeCommands(device, commandPool);

    //------------------------------------
    // Detect aspect mask
    //------------------------------------
    VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    auto isDepthFormat = [](VkFormat f)
    {
        return f == VK_FORMAT_D32_SFLOAT ||
               f == VK_FORMAT_D32_SFLOAT_S8_UINT ||
               f == VK_FORMAT_D24_UNORM_S8_UINT ||
               f == VK_FORMAT_D16_UNORM ||
               f == VK_FORMAT_D16_UNORM_S8_UINT;
    };

    auto hasStencil = [](VkFormat f)
    {
        return f == VK_FORMAT_D32_SFLOAT_S8_UINT ||
               f == VK_FORMAT_D24_UNORM_S8_UINT ||
               f == VK_FORMAT_D16_UNORM_S8_UINT;
    };

    if (isDepthFormat(format))
    {
        aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (hasStencil(format))
            aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    //------------------------------------
    // Image barrier (sync2)
    //------------------------------------
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;

    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    barrier.image = image;

    barrier.subresourceRange.aspectMask = aspectMask;
    barrier.subresourceRange.baseMipLevel = baseMip;
    barrier.subresourceRange.levelCount = mipCount;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    //------------------------------------
    // Stage + Access selection
    //------------------------------------
    auto set = [&]( VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess)
    {
        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED)
        {
            barrier.srcStageMask  = VK_PIPELINE_STAGE_2_NONE;
            barrier.srcAccessMask = 0;
        } else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcStageMask  = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
            barrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcStageMask  = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
            barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
        {
            barrier.srcStageMask  = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
            barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
        } else
        {
            barrier.srcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
        }
        
        barrier.dstStageMask  = dstStage;
        barrier.dstAccessMask = dstAccess;
    };

    if (newLayout == VK_IMAGE_LAYOUT_GENERAL)
    {
        set(
            VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT
        );
    }
    else if (newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        set(
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT);
    }
    else if (newLayout == VK_IMAGE_LAYOUT_UNDEFINED)
    {
        set(
            VK_PIPELINE_STAGE_2_NONE,
            0);
    }
    else if (newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        set(
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    } else if (newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
    {
        set(
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_ACCESS_2_TRANSFER_READ_BIT);
    }
    else if (newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
    {
        set(VK_PIPELINE_STAGE_2_NONE,
            VK_ACCESS_2_NONE);
    }
    else if (newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
    {
        set(
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT
        );
    }
    else if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        set(
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
    }
    else
    {
        LOG_FATAL("Unsupported layout transition");
    }

    //------------------------------------
    // Dependency wrapper
    //------------------------------------
    VkDependencyInfo dep{};
    dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep.imageMemoryBarrierCount = 1;
    dep.pImageMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2(commandBuffer, &dep);

    //------------------------------------
    // Submit if we created command buffer
    //------------------------------------
    if (createdCmd)
        endSingleTimeCommands(device, graphicsQueue, commandBuffer, commandPool);
}

void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, VkDevice device, VkQueue queue, VkCommandPool commandPool, VkCommandBuffer commandBuff)
{
    VkCommandBuffer commandBuffer = (commandBuff) ? commandBuff : beginSingleTimeCommands(device, commandPool);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {0, 0, 0};
    region.imageExtent = {
        width,
        height,
        1
    };

    vkCmdCopyBufferToImage(
        commandBuffer,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    if (!commandBuff) endSingleTimeCommands(device, queue, commandBuffer, commandPool);
}


void copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize, VkFilter filter)
{
	VkImageBlit2 blitRegion{ .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr };

	blitRegion.srcOffsets[1].x = srcSize.width;
	blitRegion.srcOffsets[1].y = srcSize.height;
	blitRegion.srcOffsets[1].z = 1;

	blitRegion.dstOffsets[1].x = dstSize.width;
	blitRegion.dstOffsets[1].y = dstSize.height;
	blitRegion.dstOffsets[1].z = 1;

	blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blitRegion.srcSubresource.baseArrayLayer = 0;
	blitRegion.srcSubresource.layerCount = 1;
	blitRegion.srcSubresource.mipLevel = 0;

	blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blitRegion.dstSubresource.baseArrayLayer = 0;
	blitRegion.dstSubresource.layerCount = 1;
	blitRegion.dstSubresource.mipLevel = 0;

	VkBlitImageInfo2 blitInfo{ .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr };
	blitInfo.dstImage = destination;
	blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	blitInfo.srcImage = source;
	blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	blitInfo.filter = filter;
	blitInfo.regionCount = 1;
	blitInfo.pRegions = &blitRegion;

	vkCmdBlitImage2(cmd, &blitInfo);
}

void EndRenderPass(Window * win)
{
    vkCmdEndRendering(win->commandBuffers[win->currentFrameIndex]);
}

void BeginRenderPass(Window * win, VkAttachmentLoadOp load, VkAttachmentStoreOp store, std::array<VkClearValue, 2> clearValues, VkRect2D scissor, VkViewport viewport)
{
    VkRenderingAttachmentInfo colorAttachment{};

    if (settings::multisampling)
    {
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;

        colorAttachment.imageView = win->colorImageView;
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        colorAttachment.loadOp = load;
        colorAttachment.storeOp = store;

        colorAttachment.clearValue = clearValues[0];

        colorAttachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
        colorAttachment.resolveImageView = win->FB_ImgViews[win->currentFrameIndex];
        colorAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_GENERAL;
    } else 
    {
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;

        colorAttachment.imageView = win->FB_ImgViews[win->currentFrameIndex];
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        colorAttachment.loadOp = load;
        colorAttachment.storeOp = store;

        colorAttachment.clearValue = clearValues[0];
    }

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = win->depthImageView;
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.clearValue = clearValues[1];

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = scissor;
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = &depthAttachment;

    vkCmdBeginRendering(win->commandBuffers[win->currentFrameIndex], &renderingInfo);
}

