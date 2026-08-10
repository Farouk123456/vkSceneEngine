#include "common.h"
#include <vulkan/vulkan_core.h>

std::mutex TextDrawer::cacheMutex;
std::unordered_map<std::pair<std::string, std::u32string>, bool, TextDrawer::PairHash> TextDrawer::loaded;
std::unordered_set<TextDrawer::GlyphKey, TextDrawer::GlyphKeyHash> TextDrawer::atlasGlyphCache;
std::unordered_map<std::tuple<std::u32string, std::string, int>, std::vector<float>, TextDrawer::TupleHash> TextDrawer::measureTextWidthCumulativeCache;
std::unordered_map<int, std::vector<Font*>> TextDrawer::fallbackFontCache;
std::unordered_map<std::tuple<std::u32string,std::string,int>, TextDrawer::ShapedText, TextDrawer::TupleHash> TextDrawer::shapeCache;
std::unordered_map<std::tuple<std::u32string,std::string,int>, std::vector<TextLineData::Vertex>, TextDrawer::TupleHash> TextDrawer::vertexCache;
std::unordered_map<TextDrawer::WrapKey, std::vector<TextLineData>, TextDrawer::WrapKeyHash> TextDrawer::wrapCache;


void TextDrawer::init(Window * win, AssetManager * am, std::array<std::string,2> shaders)
{
    this->win = win;
    this->am = am;
    this->used_shaders = shaders;

    uniform.proj = glm::ortho(0.f, (float)win->currWinW, 0.f, (float)win->currWinH);
    shader.init(win->Vk, win);

    {
        std::vector<VkVertexInputBindingDescription> vertexBindingDescs = {
            VkVertexInputBindingDescription{0, sizeof(float) * 2, VK_VERTEX_INPUT_RATE_VERTEX},
            VkVertexInputBindingDescription{1, sizeof(float) * 8 + sizeof(int) * 2, VK_VERTEX_INPUT_RATE_INSTANCE}
        };

        std::vector<VkVertexInputAttributeDescription> vertexAttrDescs = {
            VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32_SFLOAT, 0},
            VkVertexInputAttributeDescription{1, 1, VK_FORMAT_R32G32_SFLOAT, 0},
            VkVertexInputAttributeDescription{2, 1, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 2 },
            VkVertexInputAttributeDescription{3, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(float) * 4 },
            VkVertexInputAttributeDescription{4, 1, VK_FORMAT_R32_SINT, sizeof(float) * 8},
            VkVertexInputAttributeDescription{5, 1, VK_FORMAT_R32_SINT, sizeof(float) * 8 + sizeof(int)},
        };

        shader.pushUnfiormLayoutBinding(UniformLayoutBindingelement{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT});
        shader.pushUnfiormLayoutBinding(UniformLayoutBindingelement{(uint)am->getAllFonts().size(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT});
        shader.setupUnfiormLayout();

        VkPushConstantRange push_constant;
        push_constant.offset = 0;
        push_constant.size = sizeof(Uniform);
        push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        shader.createGraphicsPipeline_customVertexLayout(shaders[0], shaders[1], vertexBindingDescs, vertexAttrDescs, &push_constant);
    }

    float Mesh[] = { 0,0,   0,1,  1,1,  1,0 };
    uint32_t MeshInd[] = {0, 1, 2, 2, 3, 0};

    for (uint i = 0; i < settings::maxFramesInFlight; i++)
    {
        VBOs[i].createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 8 * sizeof(float), win->graphicsQueue, win->Vk, win->commandPool);
        IBOs[i].createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 6 * sizeof(uint32_t), win->graphicsQueue, win->Vk, win->commandPool);
        
        InstVBOs[i].createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 1024*sizeof(TextLineData::Vertex), win->graphicsQueue, win->Vk, win->commandPool);
        TransformBOs[i].createBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 1024*sizeof(transform), win->graphicsQueue, win->Vk, win->commandPool);
    
        VBOs[i].writeToBuffer(Mesh, 8 * sizeof(float));
        IBOs[i].writeToBuffer(MeshInd, 6 * sizeof(uint32_t));
    }

    shader.updateSSBOs(TransformBOs, 1024*sizeof(transform), settings::maxFramesInFlight);
    
    for (int i = 0; i < am->getBitmaps().size(); i++)
    {
        TextureAtlas atl = am->getBitmaps()[i];
        shader.updateUniformTexture(atl.texture.ImgView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, atl.texture.sampler, i, settings::maxFramesInFlight);
    }

    instance = raqm_create();
}

void TextDrawer::setFallbackFonts(std::vector<std::string> ss)
{
    font_order = ss;
    fallbackFontCache.clear();
}

void TextDrawer::writeToGPU(VkCommandBuffer commandBuffer, int currFrameIndex)
{
    if (Ts.size() * sizeof(transform) > TransformBOs[0].getByteSize())
    {
        vkQueueWaitIdle(win->graphicsQueue);

        for (int i = 0; i < settings::maxFramesInFlight; i++)
        {
            // over allocate to decreasse likelihood of having to executte this again
            TransformBOs[i].resizeBuffer(2*Ts.size()*sizeof(transform), false);
        }

        shader.updateSSBOs(TransformBOs, 2*Ts.size()*sizeof(transform), settings::maxFramesInFlight);
    }
    
    vertices.clear();

    for (auto& l : pendingLines)
        for (auto& v : l.shapingV) vertices.emplace_back(v);

    if (vertices.size() > 0)
    {
        instance_size = vertices.size();
        // Upload to GPU
        if(currFrameIndex >= 0)
        {
            InstVBOs[currFrameIndex].writeToBuffer(vertices.data(), vertices.size() * sizeof(TextLineData::Vertex), commandBuffer);
            TransformBOs[currFrameIndex].writeToBuffer(Ts.data(), Ts.size() * sizeof(transform), commandBuffer);
        } else
        {
            for (uint i = 0; i < settings::maxFramesInFlight; i++)
            {
                InstVBOs[i].writeToBuffer(vertices.data(), vertices.size() * sizeof(TextLineData::Vertex), commandBuffer);
                TransformBOs[i].writeToBuffer(Ts.data(), Ts.size() * sizeof(transform), commandBuffer);
            }
        }
    } else
    {
        instance_size = 0;
    }
    
    Ts.clear();
    pendingLines.clear();
}

void TextDrawer::DrawCallInRenderPass(int currentFrameIndex)
{
    if (!instance_size) return;

    VkBuffer vbo[] = {VBOs[currentFrameIndex].getHandle(), InstVBOs[currentFrameIndex].getHandle()};
    VkDeviceSize offsets[] = {0, 0};

    vkCmdBindIndexBuffer(win->commandBuffers[currentFrameIndex], IBOs[currentFrameIndex].getHandle(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindVertexBuffers(win->commandBuffers[currentFrameIndex], 0, 2, vbo, offsets);
    vkCmdBindPipeline(win->commandBuffers[currentFrameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, shader.getPipeline());
    vkCmdBindDescriptorSets(win->commandBuffers[currentFrameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, shader.getPipelineLayout(), 0, 1, &shader.getDescriptorSets()[currentFrameIndex], 0, nullptr);
    
    uniform.proj = glm::ortho(0.f, (float)win->currWinW, 0.f, (float)win->currWinH);    
    vkCmdPushConstants(win->commandBuffers[currentFrameIndex], shader.getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Uniform), &uniform);

    vkCmdDrawIndexed(win->commandBuffers[currentFrameIndex], 6, instance_size, 0, 0, 0);
}

void TextDrawer::destroy()
{
    raqm_destroy(instance);

    for (uint i = 0; i < settings::maxFramesInFlight; i++)
    {
        VBOs[i].destroyBuffer();
        IBOs[i].destroyBuffer();
        TransformBOs[i].destroyBuffer();
        InstVBOs[i].destroyBuffer();
    }

    shader.destroy();
}

raqm_direction_t TextDrawer::detectParagraphDirection(const std::u32string& text)
{
    for (char32_t cp : text)
    {
        if ((cp >= 0x05BE && cp <= 0x10B7F) ||  // Hebrew, Arabic, etc
            (cp >= 0x0600 && cp <= 0x06FF) ||
            (cp >= 0x0750 && cp <= 0x077F))
            return RAQM_DIRECTION_RTL;

        if ((cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z'))
            return RAQM_DIRECTION_LTR;
    }

    return RAQM_DIRECTION_LTR; // default fallback
}

std::vector<TextDrawer::RAQMfontfallback> TextDrawer::getFontFallbackRuns(std::u32string_view str, const Font& primary)
{
    std::vector<RAQMfontfallback> runs;
    if (str.empty()) return runs;

    auto it = fallbackFontCache.find(primary.fontSize);
    if (it == fallbackFontCache.end())
    {
        std::vector<Font*> resolved;
        resolved.reserve(font_order.size());
        for (auto& name : font_order)
            resolved.emplace_back(&am->getFont(name, primary.fontSize)); // requires ref-returning getFont
        it = fallbackFontCache.emplace(primary.fontSize, std::move(resolved)).first;
    }
    const std::vector<Font*>& fallbackFonts = it->second;

    size_t runStart = 0;
    const Font* curr = &primary;

    auto findFont = [&](char32_t cp) -> const Font*
    {
        if (FT_Get_Char_Index(primary.fontFace, cp)) return &primary;
        for (Font* f : fallbackFonts)
            if (FT_Get_Char_Index(f->fontFace, cp)) return f;
        return &primary;
    };

    for (size_t i = 0; i < str.size(); ++i)
    {
        const Font* next = findFont(str[i]);
        if (next != curr)
        {
            runs.emplace_back(*curr, (int)runStart, (int)(i - runStart));
            curr = next;
            runStart = i;
        }
    }
    runs.emplace_back(*curr, (int)runStart, (int)(str.size() - runStart));
    return runs;
}

std::vector<float> TextDrawer::measureTextWidthCumulative(const std::u32string& text, const Font& primary_font, int fontSize)
{
    std::lock_guard lock(cacheMutex);
    if (TextDrawer::measureTextWidthCumulativeCache.contains({text, primary_font.name, fontSize}))
        return TextDrawer::measureTextWidthCumulativeCache[{text, primary_font.name, fontSize}];

    std::vector<float> result;

    if (text.empty())
        return {0};

    std::vector <RAQMfontfallback> ff = getFontFallbackRuns(text, primary_font);

    raqm_clear_contents(instance);
    raqm_direction_t dir = RAQM_DIRECTION_DEFAULT;

    raqm_set_text(instance, reinterpret_cast<const uint32_t*>(text.data()), text.length());

    for (auto& f : ff)
    {
        raqm_set_freetype_face_range(instance, f.font.fontFace, f.start, f.len);
    }

    raqm_set_par_direction(instance, dir);
        
    raqm_layout(instance);

    size_t glyphCount = 0;
    raqm_glyph_t* glyphs = raqm_get_glyphs(instance, &glyphCount);

    std::vector<float> clusterAdvance(text.size(), 0.f);
    float totalWidth = 0.f;

    for (size_t g = 0; g < glyphCount; ++g)
    {
        const auto& glyph = glyphs[g];

        Font ft = primary_font;
        float scale = (float)fontSize / primary_font.fontSize;

        for (auto& f : ff)
        {
            if (glyph.cluster >= f.start &&
                glyph.cluster <  f.start + f.len)
            {
                ft = f.font;
                scale = (float)fontSize / ft.fontSize;
                break;
            }
        }

        float adv = (glyph.x_advance / 64.f) * scale;

        if (glyph.cluster < clusterAdvance.size())
            clusterAdvance[glyph.cluster] += adv;
    }

    result.assign(text.size() + 1, 0.f);

    float running = 0.f;

    for (size_t i = 0; i < text.size(); ++i)
    {
        running += clusterAdvance[i];
        result[i + 1] = running;
    }

    TextDrawer::measureTextWidthCumulativeCache[{text, primary_font.name, fontSize}] = result;
    return result;
}
    
float TextDrawer::measureTextWidth(const std::u32string& text, const Font& primary_font, int fontSize)
{
    return measureTextWidthCumulative(text, primary_font, fontSize).back();
}

void TextDrawer::ensureGlyphsInAtlasFor(const std::u32string& text, const std::string& fontName, int fontSize, VkCommandBuffer cmd)
{
    std::lock_guard lock(cacheMutex);
    if (!TextDrawer::loaded.emplace(std::make_pair(fontName, text), true).second)
        return;

    std::u32string str = U"";
    auto ascii_words = split_on_ascii(text);
    Font& primary = am->getFont(fontName, fontSize);

    for (auto& wo : ascii_words)
    {
        if (!TextDrawer::loaded.emplace(std::make_pair(primary.name, wo), true).second)
            continue;
        str += wo;
    }
    if (str.empty()) return;

    raqm_clear_contents(instance);
    raqm_set_text(instance, reinterpret_cast<const uint32_t*>(str.data()), str.size());
    raqm_set_par_direction(instance, RAQM_DIRECTION_DEFAULT);
    auto fallbackRuns = getFontFallbackRuns(str, primary);
    for (auto& run : fallbackRuns)
        raqm_set_freetype_face_range(instance, run.font.fontFace, run.start, run.len);
    raqm_layout(instance);

    size_t glyphCount = 0;
    raqm_glyph_t* glyphs = raqm_get_glyphs(instance, &glyphCount);

    std::unordered_map<std::string, std::vector<uint32_t>> newGlyphsPerFont;
    std::vector<const Font*> clusterFonts(str.size(), &primary);
    for (auto& f : fallbackRuns)
        for (int i = 0; i < f.len; ++i)
            clusterFonts[f.start + i] = &f.font;

    for (size_t i = 0; i < glyphCount; i++)
    {
        const Font& font = *clusterFonts[glyphs[i].cluster];
        char32_t glyphIndex = glyphs[i].index;

        GlyphKey key{ font.name, glyphIndex };
        if (TextDrawer::atlasGlyphCache.insert(key).second)
            newGlyphsPerFont[font.name].emplace_back(glyphIndex);
    }

    for (auto& [fName, glyphList] : newGlyphsPerFont)
        if (!glyphList.empty())
            am->loadGlyphsIndices(glyphList.data(), glyphList.size(), fName, cmd);

    recreatePipelineIfNeeded();
}

void TextDrawer::recreatePipelineIfNeeded()
{
    bool recreate = false;
    for (auto& b : shader.getLayoutBindings())
        if (b.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER && b.descriptorCount < am->getBitmaps().size())
        { recreate = true; break; }

    if (!recreate) return;

    LOG_DEBUG("Need to recreate Pipeline");
    vkDeviceWaitIdle(win->Vk->device);

    shader.destroy();
    shader = Shader();
    shader.init(win->Vk, win);

    std::vector<VkVertexInputBindingDescription> vertexBindingDescs = {
        VkVertexInputBindingDescription{0, sizeof(float) * 2, VK_VERTEX_INPUT_RATE_VERTEX},
        VkVertexInputBindingDescription{1, sizeof(float) * 8 + sizeof(int) * 2, VK_VERTEX_INPUT_RATE_INSTANCE}
    };
    std::vector<VkVertexInputAttributeDescription> vertexAttrDescs = {
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32_SFLOAT, 0},
        VkVertexInputAttributeDescription{1, 1, VK_FORMAT_R32G32_SFLOAT, 0},
        VkVertexInputAttributeDescription{2, 1, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 2},
        VkVertexInputAttributeDescription{3, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(float) * 4},
        VkVertexInputAttributeDescription{4, 1, VK_FORMAT_R32_SINT, sizeof(float) * 8},
        VkVertexInputAttributeDescription{5, 1, VK_FORMAT_R32_SINT, sizeof(float) * 8 + sizeof(int)},
    };

    shader.pushUnfiormLayoutBinding(UniformLayoutBindingelement{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT});
    shader.pushUnfiormLayoutBinding(UniformLayoutBindingelement{(uint)am->getBitmaps().size(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT});
    
    VkPushConstantRange push_constant;
    push_constant.offset = 0;
    push_constant.size = sizeof(Uniform);
    push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    
    shader.setupUnfiormLayout();
    shader.createGraphicsPipeline_customVertexLayout(used_shaders[0], used_shaders[1], vertexBindingDescs, vertexAttrDescs, &push_constant);

    shader.updateSSBOs(TransformBOs, 128 * sizeof(transform), settings::maxFramesInFlight);
    for (int i = 0; i < am->getBitmaps().size(); i++)
    {
        TextureAtlas atl = am->getBitmaps()[i];
        shader.updateUniformTexture(atl.texture.ImgView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, atl.texture.sampler, i, settings::maxFramesInFlight);
    }
}

void TextDrawer::addGlyphVertex(std::vector<TextLineData::Vertex>& vertices, const Font& font,
                                 char32_t glyphIndex, int x_offset, int y_offset,
                                 glm::vec2& cursor, float scale, int tIdx)
{
    if (!font.glyphDescriptions.contains(glyphIndex)) return;
    const auto& gd = font.glyphDescriptions.at(glyphIndex);

    float Xpos = cursor.x + (x_offset / 64.f) * scale + gd.bearing.x * scale;
    float Ypos = cursor.y - (y_offset / 64.f) * scale + gd.bearing.y * scale;

    vertices.emplace_back(TextLineData::Vertex{
        {Xpos, Ypos},
        {gd.size.x * scale, gd.size.y * scale},
        {gd.minUV.x, gd.minUV.y, gd.maxUV.x, gd.maxUV.y},
        gd.BitmapIndex, tIdx
    });
}


const TextDrawer::ShapedText& TextDrawer::shapeText(const std::u32string& text, const Font& primary_font, int fontSize, std::vector<const Font*> clusterFonts)
{
    auto key = std::make_tuple(text, primary_font.name, fontSize);
    std::lock_guard lock(cacheMutex);
    auto it = shapeCache.find(key);
    if (it != shapeCache.end())
        return it->second;

    ShapedText result;
    if (text.empty())
    {
        return shapeCache.emplace(key, std::move(result)).first->second;
    }

    std::vector<RAQMfontfallback> ff = getFontFallbackRuns(text, primary_font);

    raqm_clear_contents(instance);
    raqm_set_text(instance, reinterpret_cast<const uint32_t*>(text.data()), text.length());
    for (auto& f : ff)
        raqm_set_freetype_face_range(instance, f.font.fontFace, f.start, f.len);
    raqm_set_par_direction(instance, RAQM_DIRECTION_DEFAULT);
    raqm_layout(instance);

    size_t glyphCount = 0;
    raqm_glyph_t* glyphs = raqm_get_glyphs(instance, &glyphCount);
    result.glyphs.assign(glyphs, glyphs + glyphCount); // one copy, out of raqm's reused buffer

    std::vector<float> clusterAdvance(text.size(), 0.f);
    for (auto& glyph : result.glyphs)
    {
        const Font* fnt = clusterFonts[glyph.cluster];
        float scale = (fnt == &primary_font) ? (float)fontSize / primary_font.fontSize
                                              : (float)fontSize / fnt->fontSize;
        float adv = (glyph.x_advance / 64.f) * scale;
        if (glyph.cluster < clusterAdvance.size())
            clusterAdvance[glyph.cluster] += adv;
    }

    return shapeCache.emplace(key, std::move(result)).first->second;
}


void TextDrawer::shapeAndEmit(const std::u32string& text, const Font& primary,
                    size_t subStart, size_t subEnd,
                    glm::vec2 cursor, float scale, int tIdx,
                    std::vector<TextLineData::Vertex>& out)
{
    if (text.empty()) return;

    int shapingFontSize = (int)(primary.fontSize * scale + 0.5f);
    bool fullRange = (subStart == 0 && subEnd == text.size());
    auto key = std::make_tuple(text, primary.name, shapingFontSize);

    if (fullRange)
    {
        std::lock_guard lock(cacheMutex);
        auto it = vertexCache.find(key);
        if (it != vertexCache.end())
        {
            size_t base = out.size();
            out.insert(out.end(), it->second.begin(), it->second.end());
            for (size_t i = base; i < out.size(); ++i)
            {
                out[i].pos.x += cursor.x;   // per-call alignment offset only
                out[i].transformIdx = tIdx;
            }
            return;
        }
    }

    std::vector<RAQMfontfallback> ff = getFontFallbackRuns(text, primary);

    std::vector<const Font*> clusterFonts(text.size(), &primary);
    for (auto& f : ff)
        for (int i = 0; i < f.len; ++i)
            clusterFonts[f.start + i] = &f.font;

    const ShapedText& shaped = shapeText(text, primary, shapingFontSize, clusterFonts);

    std::vector<TextLineData::Vertex> generated;
    generated.reserve(shaped.glyphs.size());
    glm::vec2 localCursor(0.f, cursor.y); // build cursor.x-independent so it's reusable

    for (auto& glyph : shaped.glyphs)
    {
        if (glyph.cluster < subStart || glyph.cluster >= subEnd) continue;

        const Font* fnt = clusterFonts[glyph.cluster];
        float gScale = (fnt == &primary) ? scale : (primary.fontSize * scale) / (float)fnt->fontSize;

        float xAdv = (glyph.x_advance / 64.f) * gScale;
        float yAdv = (glyph.y_advance / 64.f) * gScale;

        addGlyphVertex(generated, *fnt, glyph.index, glyph.x_offset, glyph.y_offset, localCursor, gScale, tIdx);

        localCursor.x += xAdv;
        localCursor.y += yAdv;
    }

    if (fullRange)
    {
        std::lock_guard lock(cacheMutex);
        if (vertexCache.size() > 512) vertexCache.clear();
        vertexCache.emplace(key, generated);
    }

    size_t base = out.size();
    out.insert(out.end(), generated.begin(), generated.end());
    for (size_t i = base; i < out.size(); ++i)
        out[i].pos.x += cursor.x;
}

int TextDrawer::allocateTransform(float r, float g, float b, float a, float x, float y)
{
    Ts.emplace_back(r, g, b, a, x, y);
    return (int)Ts.size() - 1;
}

std::vector<TextLineData> TextDrawer::WrapParagraph(const TextParagraphData& tod)
{
    WrapKey key{tod.text, tod.font, tod.fontSize, tod.w, tod.h, tod.ellipse, tod.align};

    {
        std::lock_guard lock(cacheMutex);
        auto it = wrapCache.find(key);
        if (it != wrapCache.end())
        {
            // Cached lines are stored position/color-neutral; patch in this call's
            // origin and color so a hit still reflects the caller's current state.
            std::vector<TextLineData> lines = it->second;
            for (auto& l : lines)
            {
                l.x += tod.x; l.y += tod.y;
                l.r = tod.r; l.g = tod.g; l.b = tod.b; l.a = tod.a;
            }
            return lines;
        }
    }

    std::vector<TextLineData> result;

    Font& font = am->getFont(tod.font, tod.fontSize);
    const float scale = tod.fontSize / (float)font.fontSize;
    const float lineHeight = (font.fontFace->size->metrics.ascender / 64.f) * scale;

    const float maxWidth  = tod.w;
    const float maxHeight = tod.h;

    bool cut_on_word = true; // doesn't work with arabic if false

    auto makeLine = [&](float x, float y, const std::u32string& text) -> TextLineData
    {
        TextLineData ld;
        ld.x = tod.x + x;
        ld.y = tod.y + y;
        ld.fontSize = tod.fontSize;
        ld.r = tod.r; ld.g = tod.g; ld.b = tod.b; ld.a = tod.a;
        ld.text = text;
        ld.font = tod.font;
        ld.ellipse = tod.ellipse;
        ld.align = LEFT;   // already resolved into x below
        ld.maxWidth = 0.f; // no further ellipsizing needed, text is already final
        return ld;
    };

    // No wrap box: one line per real paragraph, verbatim
    if (maxWidth == 0 || maxHeight == 0)
    {
        std::vector<std::u32string> input = split_u32string_on_newline_andtrim(tod.text);
        float y = 0;
        for (auto& l : input)
        {
            result.push_back(makeLine(0, y, l));
            y += lineHeight;
        }
        return result;
    }

    const int maxLines = (int)std::floor(maxHeight / lineHeight);
    if (maxLines <= 0)
    {
        LOG_DEBUG("Not formatting text because Height is too small for text size need at least: " << lineHeight);
        result.push_back(makeLine(0, 0, tod.text));
        return result;
    }

    std::vector<std::u32string> input = split_u32string_on_newline_andtrim(tod.text);
    std::vector<std::u32string> wrapped;
    wrapped.reserve(input.size());
    bool overflow = false;

    for (const auto& srcLine : input)
    {
        // ---- Shape ONCE per real source line ----
        std::vector<float> shaped = measureTextWidthCumulative(srcLine, font, tod.fontSize);

        std::u32string_view lineView(srcLine);
        size_t offset = 0; // offset inside original string

        auto widthRange = [&](size_t a, size_t b) { return shaped[b] - shaped[a]; };

        while (!lineView.empty())
        {
            if ((int)wrapped.size() >= maxLines)
            {
                overflow = true;
                break;
            }

            size_t remaining = lineView.size();

            if (widthRange(offset, offset + remaining) <= maxWidth)
            {
                wrapped.emplace_back(trim(lineView));
                break;
            }

            // ---- Binary search prefix ----
            size_t lo = 0;
            size_t hi = remaining;

            while (lo < hi)
            {
                size_t mid = (lo + hi + 1) / 2;
                if (widthRange(offset, offset + mid) <= maxWidth)
                    lo = mid;
                else
                    hi = mid - 1;
            }

            if (lo == 0)
                break;

            // Prefer word break
            size_t cut = lo;
            if (cut_on_word)
            {
                size_t space = lineView.substr(0, lo).find_last_of(U' ');
                if (space != std::u32string_view::npos && space > 0)
                    cut = space;
            }

            std::u32string piece = trim(lineView.substr(0, cut));
            wrapped.emplace_back(piece);

            // Advance
            size_t next = cut;
            while (next < lineView.size() && lineView[next] == U' ')
                ++next;

            lineView.remove_prefix(next);
            offset += next;
        }

        if (overflow)
            break;
    }

    // ---- Ellipsize / truncate the last line if we overflowed vertically ----
    if (overflow && !wrapped.empty())
    {
        std::u32string& last = wrapped.back();
        const std::u32string ellipsis = U"…";

        std::vector<float> shapedLast = measureTextWidthCumulative(last, font, tod.fontSize);
        auto widthLast = [&](size_t end) { return shapedLast[end]; };

        if (tod.ellipse == EllipsizeMode::ELLIPSE)
        {
            float ellWidth = measureTextWidth(ellipsis, font, tod.fontSize);

            size_t lo = 0;
            size_t hi = last.size();

            while (lo < hi)
            {
                size_t mid = (lo + hi + 1) / 2;
                if (widthLast(mid) + ellWidth <= maxWidth)
                    lo = mid;
                else
                    hi = mid - 1;
            }

            size_t cut = lo;
            if (cut_on_word)
            {
                size_t space = last.substr(0, cut).find_last_of(U' ');
                if (space != std::u32string::npos && space > 0)
                    cut = space;
            }

            last = last.substr(0, cut) + ellipsis;
        }
        else // LASTWORD
        {
            size_t lo = 0;
            size_t hi = last.size();

            while (lo < hi)
            {
                size_t mid = (lo + hi + 1) / 2;
                if (widthLast(mid) <= maxWidth)
                    lo = mid;
                else
                    hi = mid - 1;
            }

            size_t cut = lo;
            if (cut_on_word)
            {
                size_t space = last.substr(0, cut).find_last_of(U' ');
                if (space != std::u32string::npos && space > 0)
                    cut = space;
            }

            last = trim(last.substr(0, cut));
        }
    }

    // ---- Assign x (alignment) / y (line stacking) per final wrapped line ----
    float y = 0;
    result.reserve(wrapped.size());
    auto _par_dir = detectParagraphDirection(tod.text);

    for (const auto& txt : wrapped)
    {
        float width = measureTextWidth(txt, font, tod.fontSize);
        float x = 0;

        if (tod.align == AlignMode::CENTER)
            x = (maxWidth - width) * 0.5f;
        else if (tod.align == AlignMode::RIGHT || (tod.align == AlignMode::AUTO && _par_dir == RAQM_DIRECTION_RTL))
            x = maxWidth - width;

        result.push_back(makeLine(x, y, txt));
        y += lineHeight;
    }

    {
        std::lock_guard lock(cacheMutex);
        std::vector<TextLineData> cacheCopy = result;
        for (auto& l : cacheCopy) { l.x -= tod.x; l.y -= tod.y; } // store position-neutral

        if (wrapCache.size() > 256) wrapCache.clear(); // guard against unbounded growth from ever-changing dynamic text
        wrapCache.emplace(std::move(key), std::move(cacheCopy));
    }

    return result;
}

std::vector<TextLineData*> TextDrawer::addParagraph(const TextParagraphData& data, VkCommandBuffer cmd)
{
    std::vector<TextLineData> lines = WrapParagraph(data);
    std::vector<TextLineData*> refs;
    refs.resize(lines.size());
    
    for (int i = 0; i < lines.size(); i++)
        refs[i] = addLine(lines[i], cmd);

    return refs;
}

TextLineData* TextDrawer::addLine(const TextLineData& data, VkCommandBuffer cmd)
{
    pendingLines.emplace_back();
    TextLineData& h = pendingLines.back();
    h = data;

    Font& font = am->getFont(h.font, h.fontSize);
    const float scale = h.fontSize / (float)font.fontSize;
    const float lineHeight = (font.fontFace->size->metrics.ascender / 64.f) * scale;

    ensureGlyphsInAtlasFor(h.text, h.font, h.fontSize, cmd);
    h.transformIndex = allocateTransform(h.r, h.g, h.b, h.a, h.x, h.y);

    float cursorX = 0.f;
    std::u32string* drawText = &h.text;
    std::u32string ellipsizedText; // only populated if truncation actually happens

    if (h.maxWidth > 0.f)
    {
        // only pay for metrics/ellipsize work when a limit was actually requested
        auto lm = getTextMetrics(data);
        cursorX = lm.x - h.x;
        ellipsizedText = std::move(lm.text);
        drawText = &ellipsizedText;
    }

    glm::vec2 cursor(cursorX, lineHeight);
    shapeAndEmit(*drawText, font, 0, drawText->size(), cursor, scale, h.transformIndex, h.shapingV);

    return &h;
}

std::vector<LineMetric> TextDrawer::getTextMetrics(const TextParagraphData& data)
{
    std::vector<LineMetric> n;
    auto d = WrapParagraph(data);
    for (auto& m : d)
        n.emplace_back(getTextMetrics(m));
    return n;
}

LineMetric TextDrawer::getTextMetrics(const TextLineData& data)
{
    Font& font = am->getFont(data.font, data.fontSize);
    const float scale = data.fontSize / (float)font.fontSize;
    const float lineHeight = (font.fontFace->size->metrics.ascender / 64.f) * scale;
    float ascender  = (font.fontFace->size->metrics.ascender  / 64.f) * scale;
    float descender = (font.fontFace->size->metrics.descender / 64.f) * scale;

    // defensive: a "line" must never contain a paragraph separator
    std::u32string text = data.text;
    
    {
        for (auto& c : text) if (c == U'\n') c = U' ';
        auto widths = measureTextWidthCumulative(text, font, data.fontSize);
        float width = widths.back();

        if (width > data.maxWidth && data.maxWidth != 0)
        {
            if (data.ellipse == ELLIPSE)
            {
                std::u32string ellipsis = U"…";
                float el_width = measureTextWidth(ellipsis, font, data.fontSize);

                size_t lo = 0, hi = text.size();
                while (lo < hi)
                {
                    size_t mid = (lo + hi + 1) / 2;
                    if (widths[mid] + el_width <= data.maxWidth) lo = mid; else hi = mid - 1;
                }

                size_t cut = lo;
                size_t space = text.substr(0, cut).find_last_of(U' ');
                if (space != std::u32string::npos && space > 0)
                    cut = space;

                text = text.substr(0, cut) + ellipsis;
            }
            else if (data.ellipse == LASTWORD)
            {
                // largest prefix (by character count) whose shaped width fits
                size_t lo = 0, hi = text.size();
                while (lo < hi)
                {
                    size_t mid = (lo + hi + 1) / 2;
                    if (widths[mid] <= data.maxWidth) lo = mid; else hi = mid - 1;
                }

                size_t cut = lo;
                // only back up to a word boundary if one actually exists within the fit
                size_t space = text.substr(0, cut).find_last_of(U' ');
                if (space != std::u32string::npos && space > 0)
                    cut = space;

                text = text.substr(0, cut);
            }
        }
    }

    float width = measureTextWidth(text, font, data.fontSize);
    float x = 0;
    if (data.maxWidth > 0)
    {
        if (data.align == AlignMode::CENTER) x = (data.maxWidth - width) * 0.5f;
        else if (data.align == AlignMode::RIGHT) x = data.maxWidth - width;
    }

    LineMetric lm;
    lm.x = x + data.x; lm.y = data.y; lm.w = width; lm.h = lineHeight;
    lm.baseline_y = lm.y + lineHeight;
    lm.center_y = lm.y + lineHeight - (ascender + descender);
    lm.text = text;
    return lm;
}


void TextDrawer::preLoadText(TextLineData& data)
{
    Font& font = am->getFont(data.font, data.fontSize);
    const float scale = data.fontSize / (float)font.fontSize;
    const float lineHeight = (font.fontFace->size->metrics.ascender / 64.f) * scale;

    ensureGlyphsInAtlasFor(data.text, data.font, data.fontSize);
    data.transformIndex = allocateTransform(data.r, data.g, data.b, data.a, data.x, data.y);

    float cursorX = 0.f;
    std::u32string* drawText = &data.text;
    std::u32string ellipsizedText; // only populated if truncation actually happens

    if (data.maxWidth > 0.f)
    {
        // only pay for metrics/ellipsize work when a limit was actually requested
        auto lm = getTextMetrics(data);
        cursorX = lm.x - data.x;
        ellipsizedText = std::move(lm.text);
        drawText = &ellipsizedText;
    }

    glm::vec2 cursor(cursorX, lineHeight);
    shapeAndEmit(*drawText, font, 0, drawText->size(), cursor, scale, data.transformIndex, data.shapingV);
}

void TextDrawer::preLoadText(TextParagraphData& data)
{
    std::vector<TextLineData> lines = WrapParagraph(data);
    
    for (int i = 0; i < lines.size(); i++)
        preLoadText(lines[i]);
}
