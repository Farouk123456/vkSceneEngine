#define VMA_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_RECT_PACK_IMPLEMENTATION
#include "common.h"
#include <format> 

// imagecount can change mid application
// window too huge of a class shader class -> pipeline class
// dangling pointer window 
// LRU bounded cache for text caching
// Same TextDrawer for entire application
// Track last used buffers pipelines sets so we dont have to call vkCmdBind every frame for graphics and compute
// Branch descriptorsets into new per layer stack object
// add all draw needed data to DrawData struct
// Layer remove win class dependency
// give Buffers uuids or use buffer handle to differentiate between them
// change input system to events to layers insteadof accessing Window

constexpr float CAMERA_MOVEMENT_SPEED = 3.f;
constexpr float CAMERA_PAN_SPEED      = 2.f;
constexpr float CAMERA_MOUSE_SENSITIVITY = 0.1f;

struct Camera
{
    glm::vec3 position;
    glm::vec3 target;
    glm::vec3 up;

    float yaw = 90;
    float pitch = 0;

    bool firstMouseSample = true;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;

    void init(glm::vec3 position, glm::vec3 target, glm::vec3 up)
    {
        this->position = position; this->target = target; this->up = up;
    
        glm::vec3 dir = glm::normalize(target - position);
        pitch = glm::degrees(glm::asin(glm::clamp(dir.y, -1.f, 1.f)));
        yaw   = glm::degrees(glm::atan(dir.z, dir.x));
    }

    void updateCamera(Window * win, float frametime)
    {
        if (glfwGetInputMode(win->winHandle, GLFW_CURSOR) == GLFW_CURSOR_DISABLED)
        {
            glm::vec3 prevPos = position;
            float prevYaw = yaw;
            float prevPitch = pitch;

            glm::vec3 moveInput = {
                (float)(win->keyboardState[GLFW_KEY_A] - win->keyboardState[GLFW_KEY_D]),
                (float)(win->keyboardState[GLFW_KEY_LEFT_CONTROL] - win->keyboardState[GLFW_KEY_SPACE]),
                (float)(win->keyboardState[GLFW_KEY_W] - win->keyboardState[GLFW_KEY_S])
            };

            glm::vec2 lookInput = {
                (float)(win->keyboardState[GLFW_KEY_RIGHT] - win->keyboardState[GLFW_KEY_LEFT]),
                (float)(win->keyboardState[GLFW_KEY_DOWN] - win->keyboardState[GLFW_KEY_UP])
            };

            const glm::vec3 worldUp = up;

            glm::vec3 forward;
            forward.x = glm::cos(glm::radians(yaw)) * glm::cos(glm::radians(pitch));
            forward.y = glm::sin(glm::radians(pitch));
            forward.z = glm::sin(glm::radians(yaw)) * glm::cos(glm::radians(pitch));
            forward = glm::normalize(forward);

            glm::vec3 fwFlat(glm::cos(glm::radians(yaw)), 0, glm::sin(glm::radians(yaw)));
            glm::vec3 right = glm::normalize(glm::cross(fwFlat, worldUp));

            // --- movement: combine all axes, normalize so diagonals aren't faster ---
            glm::vec3 moveDir = right * moveInput.x + worldUp * moveInput.y + fwFlat * moveInput.z;
            if (glm::length(moveDir) > 1e-5f)
                moveDir = glm::normalize(moveDir);

            float moveScale = 30.f * frametime * CAMERA_MOVEMENT_SPEED;
            position += moveDir * moveScale;

            // --- look: mouse (when cursor disabled) falls back to arrow keys otherwise ---
            double dX = 0.0, dY = 0.0;

            if (firstMouseSample)
            {
                lastMouseX = win->mouseState.x;
                lastMouseY = win->mouseState.y;
                firstMouseSample = false;
            }

            dX = (win->mouseState.x - lastMouseX) * CAMERA_MOUSE_SENSITIVITY;
            dY = (win->mouseState.y - lastMouseY) * CAMERA_MOUSE_SENSITIVITY;

            lastMouseX = win->mouseState.x;
            lastMouseY = win->mouseState.y;

            if (dX == 0.0 && dY == 0.0)
            {
                float panScale = 30.f * frametime * CAMERA_PAN_SPEED;
                dX = lookInput.x * panScale;
                dY = lookInput.y * panScale;
            }

            yaw -= (float)dX;
            pitch += (float)dY;

            pitch = glm::clamp(pitch, -89.f, 89.f);
            yaw = glm::mod(yaw, 360.f);
        
            target = position + forward;
        }
    }

    glm::mat4 getView()
    {
        return glm::lookAt(position, target, up);
    }
};

class BasicDrawer
{
    AssetManager * am   = nullptr;
    Window* win         = nullptr;
    VulkanHandler* Vk   = nullptr;

    Shader shader;
    GPUBuffer VBOs[settings::maxFramesInFlight];
    GPUBuffer UBOs[settings::maxFramesInFlight];
    GPUBuffer IBOs[settings::maxFramesInFlight];
    
public:
    struct Vertex
    {
        float x;
        float y;
        float z;
        glm::vec4 col;
        glm::vec2 uv;
        int tex_idx;
    };

    struct Uniform
    {
        glm::mat4 proj;
        glm::mat4 view;
        int window_width;
        int window_height;
        float t;
    } u;

    std::vector<Vertex> vertices  = {};
    std::vector<uint32_t> indices = {};
    TextDrawer * td     = nullptr;

    void init(VulkanHandler* VKH, AssetManager* a, TextDrawer* t, Window* win)
    {
        this->Vk = VKH; am = a; td = t; this->win = win;

        shader.init(VKH, win);

        shader.pushInputLayoutBinding(VertexInputBindingelement{VK_FORMAT_R32G32B32_SFLOAT, 1, sizeof(float)*3});
        shader.pushInputLayoutBinding(VertexInputBindingelement{VK_FORMAT_R32G32B32A32_SFLOAT, 1, sizeof(float)*4});
        shader.pushInputLayoutBinding(VertexInputBindingelement{VK_FORMAT_R32G32_SFLOAT, 1, sizeof(float)*2});
        shader.pushInputLayoutBinding(VertexInputBindingelement{VK_FORMAT_R32_SINT, 1, sizeof(int)});
        shader.setupInputLayout();

        shader.pushUnfiormLayoutBinding(UniformLayoutBindingelement{1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT});
        shader.pushUnfiormLayoutBinding(UniformLayoutBindingelement{1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT});
        shader.pushUnfiormLayoutBinding(UniformLayoutBindingelement{1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT});
        shader.setupUnfiormLayout();

        shader.createGraphicsPipeline("shaders/basic_vert.spv", "shaders/basic_frag.spv");
    
        for (uint i = 0; i < settings::maxFramesInFlight; i++)
        {
            VBOs[i].createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof(Vertex) * 128, win->graphicsQueue, win->Vk, win->commandPool);
            IBOs[i].createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, sizeof(uint32_t) * 256, win->graphicsQueue, win->Vk, win->commandPool);
            UBOs[i].createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(Uniform), win->graphicsQueue, win->Vk, win->commandPool);
        }

        setUniform(0);
        shader.updateUniformUBOs(UBOs, sizeof(Uniform), settings::maxFramesInFlight);
    }

    void bindTexture(Texture t, int index_slot)
    {
        if (index_slot < 0) return;
        shader.updateUniformTexture(t.ImgView, t.layout, t.sampler, index_slot, settings::maxFramesInFlight, 2);
    }

    void setUniform(float t = -1)
    {
        u = Uniform{ glm::ortho(0.f, (float)win->currWinW, 0.f, (float)win->currWinH), glm::mat4(1), win->currWinW, win->currWinH, u.t};
        if (t >= 0) u.t = t;
    }

    void pushVertices(std::vector<Vertex>& v, std::vector<uint32_t>& i)
    {
        int k = vertices.size();
        vertices.append_range(v);
        for (uint32_t& t : i)
            indices.emplace_back(t + k);
    }

    // warning td.writeGPU should be called explicitly after this to render widget text 
    void writeGPU(int currentFrameIndex)
    {
        VBOs[currentFrameIndex].writeToBuffer(vertices.data(), sizeof(Vertex) * vertices.size(), win->commandBuffers[currentFrameIndex]);
        IBOs[currentFrameIndex].writeToBuffer(indices.data(), sizeof(uint32_t) * indices.size(), win->commandBuffers[currentFrameIndex]);
        UBOs[currentFrameIndex].writeToBuffer(&u, sizeof(Uniform), win->commandBuffers[currentFrameIndex]);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo.imageView = win->FB_ImgViews[win->currentFrameIndex];
        imageInfo.sampler = win->FB_sampler;

        VkWriteDescriptorSet ds;
        ds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        ds.descriptorCount = 1;
        ds.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        ds.pImageInfo = &imageInfo;
        ds.dstSet = shader.getDescriptorSets()[win->currentFrameIndex];
        ds.dstArrayElement = 0;
        ds.pNext = nullptr;
        ds.dstBinding = 1;

        shader.updateUniform(ds);
    }

    void clearDrawQueue()
    {
        vertices.clear();
        indices.clear();
    }

    void drawCall(uint32_t imageIndex, int currentFrameIndex)
    {
        VkBuffer vbo[] = {VBOs[currentFrameIndex].getHandle()};
        VkDeviceSize offsets[] = {0};

        vkCmdBindIndexBuffer(win->commandBuffers[currentFrameIndex], IBOs[currentFrameIndex].getHandle(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindVertexBuffers(win->commandBuffers[currentFrameIndex], 0, 1, vbo, offsets);
        vkCmdBindPipeline(win->commandBuffers[currentFrameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, shader.getPipeline());
        vkCmdBindDescriptorSets(win->commandBuffers[currentFrameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, shader.getPipelineLayout(), 0, 1, &shader.getDescriptorSets()[currentFrameIndex], 0, nullptr);
        vkCmdDrawIndexed(win->commandBuffers[currentFrameIndex], indices.size(), 1, 0, 0, 0);
        td->DrawCallInRenderPass(currentFrameIndex);
        vertices.clear();
        indices.clear();
    }

    void destroy()
    {
        for (uint i = 0; i < settings::maxFramesInFlight; i++)
        {
            VBOs[i].destroyBuffer();
            IBOs[i].destroyBuffer();
            UBOs[i].destroyBuffer();
        }

        shader.destroy();
    }
};

class RadioButton
{
    BasicDrawer * bd = nullptr;
    float x,y,w;
    bool state = false;
    bool hover = false;

    bool press[2] = {true, true};
public:
    void init(BasicDrawer * bd, float x, float y, float w)
    {
        this->bd = bd;
        this->x = x;
        this->y = y;
        this->w = w;
    }

    void write()
    {
        float b = 3;
        float b2 = w*0.5 - 3;

        glm::vec4 col = (hover) ? glm::vec4(0.5, 0.95,1, 1) : glm::vec4(1);

        std::vector<BasicDrawer::Vertex> v = {
            {x, y, 0, col, glm::vec2(0,0), -2},
            {x+w, y, 0, col, glm::vec2(0,0), -2},
            {x+w, y+w, 0, col, glm::vec2(0,0), -2},
            {x, y+w, 0, col, glm::vec2(0,0), -2},

            {x+b, y+b, 0, glm::vec4(0.07,0.07,0.13,1), glm::vec2(0,0), -2},
            {x+w-b, y+b, 0, glm::vec4(0.07,0.07,0.13,1), glm::vec2(0,0), -2},
            {x+w-b, y+w-b, 0, glm::vec4(0.07,0.07,0.13,1), glm::vec2(0,0), -2},
            {x+b, y+w-b, 0, glm::vec4(0.07,0.07,0.13,1), glm::vec2(0,0), -2},

            {x+b2, y+b2, 0, col, glm::vec2(0,0), -2},
            {x+w-b2, y+b2, 0, col, glm::vec2(0,0), -2},
            {x+w-b2, y+w-b2, 0, col, glm::vec2(0,0), -2},
            {x+b2, y+w-b2, 0, col, glm::vec2(0,0), -2}
        };

        std::vector<uint32_t> ind;

        if (state)
        {
            ind = {
                0,1,2, 2,3,0, 4,5,6, 6,7,4, 8,9,10,10,11,8
            };
        } else {
            ind = {
                0,1,2, 2,3,0, 4,5,6, 6,7,4
            };
        }
        

        bd->pushVertices(v, ind);
    }

    bool getState()
    {
        return state;
    }

    void setState(bool st)
    {
        state = st;
    }

    void update(Window * win)
    {
        hover = false;

        if (win->mouseState.x >= x && win->mouseState.x <= x + w && win->mouseState.y >= y && win->mouseState.y <= y + w)
        {
            hover = true;
        }

        press[1] = press[0];
        press[0] = win->mouseState.btnState[GLFW_MOUSE_BUTTON_LEFT];

        if (press[0] && !press[1] && hover)
        {
            state = !state;
        }
    }
};

class Slider
{
    BasicDrawer * bd = nullptr;
    float x,y,w,h,min,max,val;

    bool press[2] = {true, true};
    bool hover = false;
    bool hoverVal = false;
    bool manipulating = false;

    TextLineData toh;

public:
    void init(BasicDrawer * bd, float x, float y, float w, float h, float min, float max, float val)
    {
        this->bd = bd; this->x = x; this->y = y + h * 0.5; this->w = w*0.8; this->h = h;
        this->min = min; this->max = max; this->val = val;
    
        std::string t = std::format("{:.3f}", val);
        toh = TextLineData{x+w*0.85f+15,y, (int)(2*h), 1,1,1,1, std::u32string(t.begin(), t.end()), "CascadiaCode.ttf", LASTWORD, RIGHT, w*0.15f-15};
        bd->td->preLoadText(toh);
    }

    void write(VkCommandBuffer cmd = VK_NULL_HANDLE, int frameIndex = -1)
    {
        bd->td->addLine(toh);
        glm::vec4 valLinecol = (hover) ? glm::vec4(0.5, 0.95,1, 1) : glm::vec4(1);
        glm::vec4 linecol = (hover) ? glm::vec4(0.2, 0.2,0.2, 1) : glm::vec4(0.4,0.4,0.4, 1);
        glm::vec4 Valcol = (hoverVal) ? glm::vec4(0.5, 0.95,1, 1) : glm::vec4(1);

        float vx = ((val - min) / (max - min)) * w + x;

        std::vector<BasicDrawer::Vertex> v = {
            {x, y, 0, linecol, glm::vec2(0), -2},
            {x + w, y, 0, linecol, glm::vec2(0), -2},
            {x + w, y + h, 0, linecol, glm::vec2(0), -2},
            {x, y + h, 0, linecol, glm::vec2(0), -2},
        
            {x, y, 0, valLinecol, glm::vec2(0), -2},
            {vx, y, 0, valLinecol, glm::vec2(0), -2},
            {vx, y + h, 0, valLinecol, glm::vec2(0), -2},
            {x, y + h, 0, valLinecol, glm::vec2(0), -2},
        
            {vx - h, y - 0.5f * h, 0, Valcol, glm::vec2(0), -2},
            {vx + h, y - 0.5f * h, 0, Valcol, glm::vec2(0), -2},
            {vx + h, y + 1.5f * h, 0, Valcol, glm::vec2(0), -2},
            {vx - h, y + 1.5f * h, 0, Valcol, glm::vec2(0), -2},
        };

        std::vector<uint32_t> ind = {0,1,2,2,3,0,  4,5,6,6,7,4, 8,9,10,10,11,8};
        bd->pushVertices(v, ind);
    }

    float getVal()
    {
        return val;
    }

    void setVal(float val)
    {
        this->val = val;
    }

    void update(Window * win)
    {
        hover = false;
        hoverVal = false;

        if (win->mouseState.x >= x && win->mouseState.x <= x + w && win->mouseState.y >= y && win->mouseState.y <= y + h && !manipulating)
        {
            hover = true;
        }

        float v = ((val - min) / (max - min)) * w + x;
        if (win->mouseState.x >= v - h && win->mouseState.x <= v + h && win->mouseState.y >= y - 0.5*h && win->mouseState.y <= y + 1.5*h && !manipulating)
        {    
            hoverVal = true;
            hover = true;
        }

        press[1] = press[0];
        press[0] = win->mouseState.btnState[GLFW_MOUSE_BUTTON_LEFT];

        if (press[0] && !press[1] && hover)
        {
            manipulating = true;
        }

        if (!press[0] && press[1] && manipulating)
        {
            manipulating = false;
        }

        if (manipulating)
        {
            float t = (glm::clamp((float)win->mouseState.x, x, x+w) - x) / w;
            val = min + t * (max - min);
        }

        std::string t = std::format("{:.3f}", val);
        toh.text = (std::u32string(t.begin(), t.end()));
    }
};

class Button
{
    BasicDrawer * bd = nullptr;
    float x,y,w,h;
    bool hover = false;
    bool btn_press = false;
    bool press[2] = {true, true};
    TextLineData toh;
public:
    void init(BasicDrawer * bd, float x, float y, float w, float h, std::string lbl)
    {
        this->bd = bd;
        this->x = x;
        this->y = y;
        this->w = w;
        this->h = h;

        toh = TextLineData{x,y + 0.333f * (h- (int)(1.5*w / lbl.length())), (int)(1.5*w / lbl.length()), 1,1,1,1, std::u32string(lbl.begin(), lbl.end()), "CascadiaCode.ttf", LASTWORD, CENTER, w};
        bd->td->preLoadText(toh);
    }

    void setLabel(std::string lbl)
    {
        toh.text = (std::u32string(lbl.begin(), lbl.end()));
    }

    void write(VkCommandBuffer cmd = VK_NULL_HANDLE, int frameIndex = -1)
    {
        float b = 3;
        float b2 = w*0.5 - 3;

        glm::vec4 col = (hover) ? glm::vec4(0.5, 0.95,1, 1) : glm::vec4(1);
        glm::vec4 bg = (press[0] && hover) ? glm::vec4(0.03,0.03,0.07,1) : glm::vec4(0.07,0.07,0.13,1);

        std::vector<BasicDrawer::Vertex> v = {
            {x, y, 0, col, glm::vec2(0,0), -2},
            {x+w, y, 0, col, glm::vec2(0,0), -2},
            {x+w, y+h, 0, col, glm::vec2(0,0), -2},
            {x, y+h, 0, col, glm::vec2(0,0), -2},

            {x + b, y + b, 0, bg, glm::vec2(0,0), -2},
            {x+w-b, y + b, 0, bg, glm::vec2(0,0), -2},
            {x+w-b, y+h-b, 0, bg, glm::vec2(0,0), -2},
            {x + b, y+h-b, 0, bg, glm::vec2(0,0), -2},
        };

        std::vector<uint32_t> ind = {
            0,1,2,2,3,0,
            4,5,6,6,7,4
        };

        bd->pushVertices(v, ind);
        bd->td->addLine(toh);
    }

    bool btnPressed()
    {
        return btn_press;
    }

    void update(Window * win)
    {
        btn_press = false;
        hover = false;

        if (win->mouseState.x >= x && win->mouseState.x <= x + w && win->mouseState.y >= y && win->mouseState.y <= y + h)
        {
            hover = true;
        }

        press[1] = press[0];
        press[0] = win->mouseState.btnState[GLFW_MOUSE_BUTTON_LEFT];

        if (press[0] && !press[1] && hover)
        {
            btn_press = true;
        }
    }
};

struct WaveSim : Layer
{
    Shader shader;
    Shader comp_shader;
    GPUBuffer VBOs[settings::maxFramesInFlight];
    GPUBuffer IBOs[settings::maxFramesInFlight];
    GPUBuffer UBOs[settings::maxFramesInFlight];

    struct Vertex
    {
        float x;
        float y;
        float z;
        float r;
        float g;
        float b;
        float uvx;
        float uvy;
        int texIdx;
    };

    struct Uniform
    {
        int window_width;
        int window_height;

        float source_freq;
        float freq;
        float c;
        float dt;
        int32_t sw;
        int32_t pw;
        int32_t ds;
        int32_t wred;
        int32_t par;
        int32_t circ;
    } uniform;

    Vertex v[4];
    uint32_t ind[6] = { 0,1,2,2,3,0 };

    glm::ivec2 wavePicSize;
    WindowRescources * res;

    void init(VulkanHandler * vk, Window * w, LayerEventHandler * EH, WindowRescources * res) override
    {
        win = w; this->updateFrequency = updateFrequency; VKH=vk; this->res = res;
        
        auto assets = res->assets.getAllAssets();
        wavePicSize = 2 * glm::ivec2(win->currWinW, win->currWinH);

        float* buf = new float[wavePicSize.x * wavePicSize.y * 4];
        memset(buf,0,sizeof(float)*4*wavePicSize.x * wavePicSize.y);

        for(int i=0;i<2;i++)
        {
            wave_images[i] = res->assets.createStorageImage(
                wavePicSize.x, wavePicSize.y,
                VK_FORMAT_R32G32B32A32_SFLOAT,
                buf,
                sizeof(float)*4*wavePicSize.x*wavePicSize.y
            );
        }

        delete[] buf;

        Vertex vv[] = {
            {-1,-1,0, 1,1,1, (wavePicSize.x - win->currWinW) * 0.5f / wavePicSize.x, (wavePicSize.y - win->currWinH) * 0.5f / wavePicSize.y, 0},
            {-1,+1,0, 1,1,1, (wavePicSize.x - win->currWinW) * 0.5f / wavePicSize.x, 1.f - ((wavePicSize.y - win->currWinH) * 0.5f / wavePicSize.y), 0},
            {+1,+1,0, 1,1,1, 1.f - ((wavePicSize.x - win->currWinW) * 0.5f / wavePicSize.x),1.f - ((wavePicSize.y - win->currWinH) * 0.5f / wavePicSize.y), 0},
            {+1,-1,0, 1,1,1, 1.f - ((wavePicSize.x - win->currWinW) * 0.5f / wavePicSize.x),(wavePicSize.y - win->currWinH) * 0.5f / wavePicSize.y, 0}
        };
        
        memcpy(v, vv, sizeof(v));

        uniform = {win->currWinW, win->currWinH, 0.1, 6, 100, 0.005, true, false, true, false, false, false};

        shader.init(VKH, win);
        shader.pushInputLayoutBinding(VertexInputBindingelement{VK_FORMAT_R32G32B32_SFLOAT, 1, sizeof(float)*3});
        shader.pushInputLayoutBinding(VertexInputBindingelement{VK_FORMAT_R32G32B32_SFLOAT, 1, sizeof(float)*3});
        shader.pushInputLayoutBinding(VertexInputBindingelement{VK_FORMAT_R32G32_SFLOAT, 1, sizeof(float)*2});
        shader.pushInputLayoutBinding(VertexInputBindingelement{VK_FORMAT_R32_SINT, 1, sizeof(int)});
        shader.setupInputLayout();

        shader.pushUnfiormLayoutBinding(UniformLayoutBindingelement{1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT});
        shader.pushUnfiormLayoutBinding(UniformLayoutBindingelement{(uint)assets.size() + 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT});
        shader.setupUnfiormLayout();
        shader.createGraphicsPipeline("shaders/wave_vert.spv", "shaders/wave_frag.spv");
    
        for (uint i = 0; i < settings::maxFramesInFlight; i++)
        {
            VBOs[i].createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof(v), win->graphicsQueue, win->Vk, win->commandPool);
            IBOs[i].createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, sizeof(ind), win->graphicsQueue, win->Vk, win->commandPool);
            UBOs[i].createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(uniform), win->graphicsQueue, win->Vk, win->commandPool);

            VBOs[i].writeToBuffer(v, sizeof(v));
            IBOs[i].writeToBuffer(ind, sizeof(ind));
            UBOs[i].writeToBuffer(&uniform, sizeof(uniform));
        }

        shader.updateUniformUBOs(UBOs, sizeof(uniform), settings::maxFramesInFlight);
        
        int i = 1;

        for (auto& ac : assets)
        {
            shader.updateUniformTexture(ac.ImgView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, ac.sampler, i, settings::maxFramesInFlight);
            i++;
        }

        // ============================================================
        // COMPUTE
        // ============================================================

        comp_shader.init(win->Vk,win,{
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,settings::maxFramesInFlight*2},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,settings::maxFramesInFlight}
        });

        comp_shader.pushUnfiormLayoutBinding({1,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,VK_SHADER_STAGE_COMPUTE_BIT});
        comp_shader.pushUnfiormLayoutBinding({1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,VK_SHADER_STAGE_COMPUTE_BIT});
        comp_shader.pushUnfiormLayoutBinding({1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,VK_SHADER_STAGE_COMPUTE_BIT});
        comp_shader.setupUnfiormLayout();
        comp_shader.createComputePipeline("shaders/wave_comp.spv");
        comp_shader.updateUniformUBOs(UBOs,sizeof(uniform),settings::maxFramesInFlight);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = win->commandPool;
        allocInfo.commandBufferCount = 1;

        vkAllocateCommandBuffers(win->Vk->device, &allocInfo, &compCmdBuff);
        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        vkCreateFence(win->Vk->device, &fenceInfo, nullptr, &compFence);
        
        VkSemaphoreCreateInfo ci {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        vkCreateSemaphore(win->Vk->device, &ci, nullptr, &graphicsDoneSem);
        vkCreateSemaphore(win->Vk->device, &ci, nullptr, &computeDoneSem);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 0;
        submitInfo.pCommandBuffers = nullptr;
        submitInfo.pSignalSemaphores = &computeDoneSem;
        submitInfo.signalSemaphoreCount = 1;
        vkQueueSubmit(win->graphicsQueue,  1, &submitInfo, nullptr);

        EH->events.push(LayerEvent{ADD_SIGNAL_SEMAPHORE, this, 0, graphicsDoneSem});
        EH->events.push(LayerEvent{ADD_WAIT_SEMAPHORE, this, 0, computeDoneSem});
    }

    Texture wave_images[2];
    int currentPing = 0;

    VkCommandBuffer compCmdBuff;
    VkFence compFence;
    VkSemaphore graphicsDoneSem;
    VkSemaphore computeDoneSem;

    void onrecreate_swapchain() override
    {

    }

    void destroy() override
    {
        res->assets.destroyTexture(wave_images[0]);
        res->assets.destroyTexture(wave_images[1]);
        comp_shader.destroy();

        for (uint i = 0; i < settings::maxFramesInFlight; i++)
        {
            VBOs[i].destroyBuffer();
            IBOs[i].destroyBuffer();
            UBOs[i].destroyBuffer();
        }

        shader.destroy();

        vkDestroyFence(win->Vk->device, compFence, nullptr);
        vkDestroySemaphore(win->Vk->device, graphicsDoneSem, nullptr);
        vkDestroySemaphore(win->Vk->device, computeDoneSem, nullptr);
    }

    void computeCall(bool actuallyCompute, bool& clearImg)
    {
        int readIdx  = this->currentPing;
        int writeIdx = 1-this->currentPing;

        vkWaitForFences(win->Vk->device, 1, &compFence, VK_TRUE, UINT64_MAX);
        vkResetFences(win->Vk->device, 1, &compFence);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(compCmdBuff, &beginInfo);

        if (clearImg)
        {
            VkClearColorValue color = {0, 0, 0, 0};
            VkImageSubresourceRange sub {};
            sub.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            sub.baseMipLevel = 0;
            sub.levelCount = 1;
            sub.baseArrayLayer = 0;
            sub.layerCount = 1;

            vkCmdClearColorImage(compCmdBuff, wave_images[0].image, VK_IMAGE_LAYOUT_GENERAL, &color, 1, &sub);
            vkCmdClearColorImage(compCmdBuff, wave_images[1].image, VK_IMAGE_LAYOUT_GENERAL, &color, 1, &sub);
            clearImg = false;
        }

        if (actuallyCompute)
        {
            VkDescriptorImageInfo readInfo{};
            readInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            readInfo.imageView   = this->wave_images[readIdx].ImgView;

            VkDescriptorImageInfo writeInfo{};
            writeInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            writeInfo.imageView   = this->wave_images[writeIdx].ImgView;

            VkWriteDescriptorSet ds[2]{};

            ds[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            ds[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            ds[0].descriptorCount = 1;
            ds[0].pImageInfo      = &readInfo;
            ds[0].dstSet          = this->comp_shader.getDescriptorSets()[0];
            ds[0].dstBinding      = 1;

            ds[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            ds[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            ds[1].descriptorCount = 1;
            ds[1].pImageInfo      = &writeInfo;
            ds[1].dstSet          = this->comp_shader.getDescriptorSets()[0];
            ds[1].dstBinding      = 2;

            vkUpdateDescriptorSets(this->win->Vk->device,2,ds, 0, nullptr);

            vkCmdBindPipeline(compCmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, this->comp_shader.getPipeline());
            vkCmdBindDescriptorSets(compCmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, this->comp_shader.getPipelineLayout(), 0, 1,  &this->comp_shader.getDescriptorSets()[0], 0, 0);
        
            uint32_t groupSize = 16;
            uint32_t groupCountX = (this->wave_images[0].size.width  + groupSize - 1) / groupSize;
            uint32_t groupCountY = (this->wave_images[0].size.height + groupSize - 1) / groupSize;

            vkCmdDispatch(compCmdBuff, groupCountX, groupCountY, 1);
            this->currentPing = writeIdx;
        }

        vkEndCommandBuffer(compCmdBuff);
        
        VkPipelineStageFlags st = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

        VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &compCmdBuff;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &graphicsDoneSem;
        submitInfo.pWaitDstStageMask = &st;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &computeDoneSem;

        vkQueueSubmit(this->win->computeQueue, 1, &submitInfo, compFence);
    }

    void setUpdateFrequency(int freq) override
    {
        updateFrequency = freq;
    }

    void draw(uint32_t imageIndex, int currentFrameIndex) override
    {
        UBOs[currentFrameIndex].writeToBuffer(&uniform, sizeof(Uniform), win->commandBuffers[currentFrameIndex]);

        int readIdx  = currentPing;
        int writeIdx = 1-currentPing;

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo.imageView   = wave_images[readIdx].ImgView;
        imageInfo.sampler     = wave_images[readIdx].sampler;
        VkWriteDescriptorSet ds2{};
        ds2.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        ds2.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        ds2.descriptorCount = 1;
        ds2.pImageInfo      = &imageInfo;
        ds2.dstSet          = shader.getDescriptorSets()[currentFrameIndex];
        ds2.dstBinding      = 1;
        vkUpdateDescriptorSets(win->Vk->device,1,&ds2,0,nullptr);
        
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.05, 0.05f, 0.05f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        VkViewport viewport{};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = win->swapchainExtent.width;
        viewport.height = win->swapchainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {win->swapchainExtent.width, win->swapchainExtent.height};

        BeginRenderPass(win, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, clearValues, scissor, viewport);
        
        vkCmdSetViewport(win->commandBuffers[currentFrameIndex], 0, 1, &viewport);
        vkCmdSetScissor(win->commandBuffers[currentFrameIndex], 0, 1, &scissor);

        VkBuffer vbo[] = {VBOs[currentFrameIndex].getHandle()};
        VkDeviceSize offsets[] = {0};

        vkCmdBindIndexBuffer(win->commandBuffers[currentFrameIndex], IBOs[currentFrameIndex].getHandle(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindVertexBuffers(win->commandBuffers[currentFrameIndex], 0, 1, vbo, offsets);
        vkCmdBindPipeline(win->commandBuffers[currentFrameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, shader.getPipeline());
        vkCmdBindDescriptorSets(win->commandBuffers[currentFrameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, shader.getPipelineLayout(), 0, 1, &shader.getDescriptorSets()[currentFrameIndex], 0, nullptr);
        vkCmdDrawIndexed(win->commandBuffers[currentFrameIndex], IBOs[currentFrameIndex].getByteSize() / sizeof(uint32_t), 1, 0, 0, 0);
        
        EndRenderPass(win);
    }

    float lastComp = 0.0f;
    bool tab_state[2] = {true, true};
    bool sft_state[2] = {true, true};
    bool clearImg = false;

    void clearImage() { clearImg = true; }

    void update(LayerEventHandler * EH, float t, float dt) override
    {
        tab_state[1] = tab_state[0];
        tab_state[0] = win->keyboardState[GLFW_KEY_TAB];

        if (tab_state[0] && !tab_state[1])
        {
            EH->events.push(LayerEvent{SWITCH_TO_NEXT_STACK, this});
        }

        sft_state[1] = sft_state[0];
        sft_state[0] = win->keyboardState[GLFW_KEY_RIGHT_SHIFT];

        if (sft_state[0] && !sft_state[1])
        {
            EH->events.push(LayerEvent{INTERLAYER_EVENT, this, 1, "TOGGLE", sizeof(char)*7});
            EH->events.push(LayerEvent{INTERLAYER_EVENT, this, 1, uniform, sizeof(uniform)});
        }

        bool work = false;
        
        if (t - lastComp > uniform.dt)
        {
            lastComp = t;
            work = true;
        }
        
        computeCall(work, clearImg);
    }

    void handle_event(LayerEvent ev) override
    {
        if (ev.Type == INTERLAYER_EVENT && ev.data.type() == typeid(Uniform))
        {
            clearImage();
            uniform = std::any_cast<Uniform>(ev.data);
        }
    }
};

struct WaveSimController : Layer
{
    WindowRescources * res;
    TextDrawer text_drawer;
    BasicDrawer bd;

    bool loop = false;
    int state = 0;

    std::vector<BasicDrawer::Vertex> v;
    std::vector<uint32_t> ind;

    TextLineData tohs[11];

    Slider c_sl; Slider dt_sl;
    Slider wm_freq_sl;
    Slider sw_freq_sl;
    RadioButton pw; RadioButton sw;
    RadioButton ds; RadioButton wr; RadioButton par; RadioButton circ;

    Button btn;

    void init(VulkanHandler* VKH, Window * w, LayerEventHandler * EH, WindowRescources * res) override
    {
        this->VKH = VKH; win = w; this->updateFrequency = updateFrequency; this->res = res;
        text_drawer.init(win, &res->assets, {"shaders/text_vert.spv", "shaders/text_frag.spv"});
        setUpdateFrequency(0);

        bd.init(VKH, &res->assets, &text_drawer, w);
        bd.setUniform();

        v = {
            {50,50,0, glm::vec4(0.1,0.1,0.1, 0.8), glm::vec2(0, 0), -2},
            {(float)win->currWinW - 50,50,0, glm::vec4(0.1,0.1,0.1, 0.8), glm::vec2(0, +1), -2},
            {(float)win->currWinW - 50,(float)win->currWinH - 50,0, glm::vec4(0.1,0.1,0.1, 0.8), glm::vec2(+1, +1), -2},
            {50,(float)win->currWinH - 50,0, glm::vec4(0.1,0.1,0.1, 0.8), glm::vec2(+1, 0), -2},
        };

        ind = {0,1,2, 2,3,0 };
        
        tohs[0] = TextLineData{100,100, 36, 1,1,1,1,U"Wave Simultion configuration", "CascadiaCode.ttf", LASTWORD, LEFT, 0};
        tohs[1] = TextLineData{100,250, 30, 1,1,1,1,U"c:", "CascadiaCode.ttf", LASTWORD, LEFT, 0};
        tohs[2] = TextLineData{win->currWinW * 0.5f,250, 30, 1,1,1,1, U"dt:", "CascadiaCode.ttf", LASTWORD, LEFT, 0};
        tohs[3] = TextLineData{100,300, 30, 1,1,1,1, U"Wave Movement Frequency:", "CascadiaCode.ttf", LASTWORD, LEFT, 0};
        tohs[4] = TextLineData{100,350, 30, 1,1,1,1, U"Wave Frequency:", "CascadiaCode.ttf", LASTWORD, LEFT, 0};
        tohs[5] = TextLineData{100,400, 30, 1,1,1,1, U"Wave Type:  Plane wave", "CascadiaCode.ttf", LASTWORD, LEFT, 0};
        tohs[6] = TextLineData{win->currWinW * 0.5f,400, 30, 1,1,1,1, U"Spherical wave", "CascadiaCode.ttf", LASTWORD, LEFT, 0};
        tohs[7] = TextLineData{100,450, 30, 1,1,1,1, U"Scenes: Double Slit", "CascadiaCode.ttf", LASTWORD, LEFT, 0};
        tohs[8] = TextLineData{win->currWinW * 0.25f,450, 30, 1,1,1,1, U"Wave Redirector", "CascadiaCode.ttf", LASTWORD, LEFT, 0};
        tohs[9] = TextLineData{win->currWinW * 0.5f,450, 30, 1,1,1,1, U"Parabola", "CascadiaCode.ttf", LASTWORD, LEFT, 0};
        tohs[10] = TextLineData{win->currWinW * 0.75f,450, 30, 1,1,1,1, U"Circle", "CascadiaCode.ttf", LASTWORD, LEFT, 0};

        LineMetric mets[11];

        for (int i = 0; i < 11; i++)
            mets[i] = bd.td->getTextMetrics(tohs[i]);

        tohs[6].x = mets[5].x + mets[5].w + 100; mets[6].x = tohs[6].x; 
        tohs[8].x = mets[7].x + mets[7].w + 100; mets[8].x = tohs[8].x;
        tohs[9].x = mets[8].x + mets[8].w + 100; mets[9].x = tohs[9].x;
        tohs[10].x = mets[9].x + mets[9].w + 100; mets[10].x = tohs[10].x;
        
        //issue here
        c_sl.init(&bd, mets[1].x + mets[1].w + 25, mets[1].center_y, mets[2].x - mets[1].x - mets[1].w - 50, 10, 1, 500, 100);
        dt_sl.init(&bd, mets[2].x + mets[2].w + 25, mets[2].center_y, win->currWinW - mets[2].x - mets[2].w - 125, 10, 0.001, 0.1, 0.005);
        wm_freq_sl.init(&bd, mets[3].x + mets[3].w + 25, mets[3].center_y, win->currWinW - mets[3].x - mets[3].w - 125, 10, 0, 10, 0.1);
        sw_freq_sl.init(&bd, mets[4].x + mets[4].w + 25, mets[4].center_y, win->currWinW - mets[4].x - mets[4].w - 125, 10, 0, 10, 6);
        pw.init(&bd, mets[5].x + mets[5].w + 15, mets[5].center_y, 25);
        sw.init(&bd, mets[6].x + mets[6].w + 15, mets[6].center_y, 25);
        ds.init(&bd, mets[7].x + mets[7].w + 15, mets[7].center_y, 25);
        wr.init(&bd, mets[8].x + mets[8].w + 15, mets[8].center_y, 25);
        par.init(&bd, mets[9].x + mets[9].w + 15,mets[9].center_y, 25);
        circ.init(&bd, mets[10].x + mets[10].w + 15, mets[10].center_y, 25);
        
        par.setState(true);
        pw.setState(true);

        btn.init(&bd, win->currWinW * 0.5f - 150, win->currWinH - 200, 300, 100, "Restart Simulation");

        for (int i = 0; i < 11; i++)
            text_drawer.preLoadText(tohs[i]);
    }
    
    void destroy() override
    {
        bd.destroy();
        text_drawer.destroy();
    }

    void handle_event(LayerEvent ev) override
    {
        if (ev.data.type() == typeid(const char*))
        {
            if (strcmp(std::any_cast<const char*>(ev.data), "TOGGLE") == 0)
            {
                loop = !loop;
                state = (!loop) * -1 + (loop) * 1;
            }
        } else
        {
            WaveSim::Uniform u = std::any_cast<WaveSim::Uniform>(ev.data);
            c_sl.setVal(u.c);
            dt_sl.setVal(u.dt);
            wm_freq_sl.setVal(u.source_freq);
            sw_freq_sl.setVal(u.freq);
            pw.setState(u.pw);
            sw.setState(u.sw);
            ds.setState(u.ds);
            wr.setState(u.wred);
            par.setState(u.par);
            circ.setState(u.circ);
        }
    }

    void onrecreate_swapchain() override
    {
        bd.setUniform();
    }

    void setUpdateFrequency(int freq) override { updateFrequency = freq; }

    void draw(uint32_t imageIndex, int currentFrameIndex) override
    {
        if (bd.u.t == 0) return;

        bd.pushVertices(v, ind);

        c_sl.write(win->commandBuffers[currentFrameIndex], currentFrameIndex);
        dt_sl.write(win->commandBuffers[currentFrameIndex], currentFrameIndex);
        wm_freq_sl.write(win->commandBuffers[currentFrameIndex], currentFrameIndex);
        sw_freq_sl.write(win->commandBuffers[currentFrameIndex], currentFrameIndex);
        pw.write();
        sw.write();
        ds.write();
        wr.write();
        par.write();
        circ.write();
        btn.write(win->commandBuffers[currentFrameIndex], currentFrameIndex);

        for (int i = 0; i < 11; i++)
            text_drawer.addLine(tohs[i], win->commandBuffers[currentFrameIndex]);

        bd.writeGPU(currentFrameIndex);
        text_drawer.writeToGPU(win->commandBuffers[currentFrameIndex], currentFrameIndex);
            
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.05, 0.05, 0.05f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        VkViewport viewport{};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = win->swapchainExtent.width;
        viewport.height = win->swapchainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {win->swapchainExtent.width, win->swapchainExtent.height};

        BeginRenderPass(win, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, clearValues, scissor, viewport);
        vkCmdSetViewport(win->commandBuffers[currentFrameIndex], 0, 1, &viewport);
        vkCmdSetScissor(win->commandBuffers[currentFrameIndex], 0, 1, &scissor);

        text_drawer.DrawCallInRenderPass(currentFrameIndex);
        bd.drawCall(imageIndex, currentFrameIndex);

        EndRenderPass(win);
    }


    void update(LayerEventHandler * EH, float time, float dt) override
    {
        float anim_duration = 0.1; // s

        bd.u.t += state * dt * (1.f / anim_duration);

        if (bd.u.t > 1)
        {
            bd.u.t = 1;
            state = 0;
        } else if (bd.u.t < 0)
        {
            bd.u.t = 0;
            state = 0;
        }

        if (bd.u.t == 1)
        {
            btn.update(win);

            if(btn.btnPressed())
            {
                WaveSim::Uniform uni = {win->currWinW, win->currWinH, wm_freq_sl.getVal(), sw_freq_sl.getVal(), c_sl.getVal(), dt_sl.getVal(), sw.getState(), pw.getState(), ds.getState(), wr.getState(), par.getState(), circ.getState()};
                EH->events.push(LayerEvent{INTERLAYER_EVENT, this, -1, uni, sizeof(WaveSim::Uniform)});
            }

            bool vpw = pw.getState();
            bool vsw = sw.getState();
            
            bool vds = ds.getState();
            bool vwr = wr.getState();
            bool vpar = par.getState();
            bool vcirc = circ.getState();

            c_sl.update(win);
            dt_sl.update(win);
            wm_freq_sl.update(win);
            sw_freq_sl.update(win);
            pw.update(win);
            sw.update(win);
            
            ds.update(win);
            wr.update(win);
            par.update(win);
            circ.update(win);

            if (!ds.getState() && !wr.getState() && !par.getState() && !circ.getState())
            {
                ds.setState(vds);
                wr.setState(vwr);
                par.setState(vpar);
                circ.setState(vcirc);
            }

            if (ds.getState() + wr.getState() + par.getState() + circ.getState() > 1)
            {
                ds.setState(ds.getState() - vds);
                wr.setState(wr.getState() - vwr);
                par.setState(par.getState() - vpar);
                circ.setState(circ.getState() - vcirc);
            }
            
            if (pw.getState() && sw.getState() || !pw.getState() && !sw.getState())
            {
                pw.setState(!vpw);
                sw.setState(!vsw);
            }
        }
    }
};

struct FluidSim : Layer
{
    float r;

    void init(VulkanHandler* VKH, Window * w, LayerEventHandler * EH, WindowRescources * res) override
    {
        this->VKH = VKH; win = w; this->updateFrequency = updateFrequency;
        r = 0;
    }
    
    void destroy() override {}

    void handle_event(LayerEvent ev) override {}

    void onrecreate_swapchain() override {}

    void setUpdateFrequency(int freq) override { updateFrequency = freq; }

    void draw(uint32_t imageIndex, int currentFrameIndex) override
    {
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.3, r, 0.05f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        VkViewport viewport{};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = win->swapchainExtent.width;
        viewport.height = win->swapchainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {win->swapchainExtent.width, win->swapchainExtent.height};

        BeginRenderPass(win, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, clearValues, scissor, viewport);
        vkCmdSetViewport(win->commandBuffers[currentFrameIndex], 0, 1, &viewport);
        vkCmdSetScissor(win->commandBuffers[currentFrameIndex], 0, 1, &scissor);
        EndRenderPass(win);
    }

    bool tab_state[2] = {true, true};

    void update(LayerEventHandler * EH, float t, float dt) override
    {
        tab_state[1] = tab_state[0];
        tab_state[0] = win->keyboardState[GLFW_KEY_TAB];

        if (tab_state[0] && !tab_state[1])
        {
            EH->events.push(LayerEvent{SWITCH_TO_NEXT_STACK, this});
        }
    
        r = (glm::sin(t) + 1) / 2;
    }
};

struct Triangle
{
    glm::vec3 a = {0,0,0};
    glm::vec3 b = {0,0,0};
    glm::vec3 c = {0,0,0};
    glm::vec3 color = {1,1,1}; // also light col if luminance > 0
    glm::vec2 uva = {-1, -1};
    glm::vec2 uvb = {-1, -1};
    glm::vec2 uvc = {-1, -1};
    float luminance = 0;
};

struct Mesh
{
    int startTriangles = -1;
    int numTriangles = 0;
    int materialIndex = -1;
    glm::vec3 BoundingVol_min = {0, 0, 0};
    glm::vec3 BoundingVol_max = {0, 0, 0}; // x = width y = height z = length
};

struct SceneDescription 
{
    std::vector<Triangle> triangles = {};
    std::vector<Mesh> meshes = {};
};

static void addBox(SceneDescription& sd, glm::vec3 center, glm::vec3 halfExtents, float rotationYDeg, glm::vec3 color)
{
    float rad = glm::radians(rotationYDeg);
    float cs = glm::cos(rad);
    float sn = glm::sin(rad);

    auto rotate = [&](glm::vec3 local) -> glm::vec3
    {
        glm::vec3 r;
        r.x = local.x * cs - local.z * sn;
        r.y = local.y;
        r.z = local.x * sn + local.z * cs;
        return center + r;
    };

    glm::vec3 c000 = rotate({-halfExtents.x, -halfExtents.y, -halfExtents.z});
    glm::vec3 c100 = rotate({+halfExtents.x, -halfExtents.y, -halfExtents.z});
    glm::vec3 c110 = rotate({+halfExtents.x, +halfExtents.y, -halfExtents.z});
    glm::vec3 c010 = rotate({-halfExtents.x, +halfExtents.y, -halfExtents.z});
    glm::vec3 c001 = rotate({-halfExtents.x, -halfExtents.y, +halfExtents.z});
    glm::vec3 c101 = rotate({+halfExtents.x, -halfExtents.y, +halfExtents.z});
    glm::vec3 c111 = rotate({+halfExtents.x, +halfExtents.y, +halfExtents.z});
    glm::vec3 c011 = rotate({-halfExtents.x, +halfExtents.y, +halfExtents.z});

    int start = (int)sd.triangles.size();

    // Winding below is authored for a scene where visual "up" is -Y
    // (see getScene() for why) — faces point outward under that convention.
    // +X
    sd.triangles.emplace_back(c100, c111, c110, color);
    sd.triangles.emplace_back(c111, c100, c101, color);
    // -X
    sd.triangles.emplace_back(c000, c011, c001, color);
    sd.triangles.emplace_back(c011, c000, c010, color);
    // top (local +Y, halfExtents.y direction)
    sd.triangles.emplace_back(c010, c111, c011, color);
    sd.triangles.emplace_back(c111, c010, c110, color);
    // bottom (local -Y)
    sd.triangles.emplace_back(c000, c101, c100, color);
    sd.triangles.emplace_back(c101, c000, c001, color);
    // +Z
    sd.triangles.emplace_back(c001, c111, c101, color);
    sd.triangles.emplace_back(c111, c001, c011, color);
    // -Z
    sd.triangles.emplace_back(c000, c110, c010, color);
    sd.triangles.emplace_back(c110, c000, c100, color);

    glm::vec3 mn = glm::min(glm::min(glm::min(c000, c100), glm::min(c110, c010)),
                             glm::min(glm::min(c001, c101), glm::min(c111, c011)));
    glm::vec3 mx = glm::max(glm::max(glm::max(c000, c100), glm::max(c110, c010)),
                             glm::max(glm::max(c001, c101), glm::max(c111, c011)));

    sd.meshes.emplace_back(start, (int)sd.triangles.size() - start, -1, mn, mx);
}

static SceneDescription getScene()
{
    SceneDescription sd;

    float scale = 100;

    // Authored directly for Vulkan's Y-down clip space: visual "up" is -Y here,
    // so the floor sits at +scale and the ceiling at -scale (swapped vs. a
    // Y-up scene), with winding corrected face-by-face to match.
    glm::vec3 p0 = glm::vec3(-1.f, +1.f, -1.f) * scale; // floor
    glm::vec3 p1 = glm::vec3(+1.f, +1.f, -1.f) * scale;
    glm::vec3 p2 = glm::vec3(+1.f, +1.f, +1.f) * scale;
    glm::vec3 p3 = glm::vec3(-1.f, +1.f, +1.f) * scale;
    glm::vec3 p4 = glm::vec3(-1.f, -1.f, -1.f) * scale; // ceiling
    glm::vec3 p5 = glm::vec3(+1.f, -1.f, -1.f) * scale;
    glm::vec3 p6 = glm::vec3(+1.f, -1.f, +1.f) * scale;
    glm::vec3 p7 = glm::vec3(-1.f, -1.f, +1.f) * scale;

    glm::vec3 leftWallColor  = glm::vec3(.611, .0555, .062);
    glm::vec3 rightWallColor = glm::vec3(.117, .4125, .115);
    glm::vec3 white          = glm::vec3(.7295, .7355, .729);

    // floor
    sd.triangles.emplace_back(p0, p2, p1, white);
    sd.triangles.emplace_back(p2, p0, p3, white);
    // ceiling
    sd.triangles.emplace_back(p4, p6, p5, white);
    sd.triangles.emplace_back(p6, p4, p7, white);
    // left wall (red)
    sd.triangles.emplace_back(p0, p7, p3, leftWallColor);
    sd.triangles.emplace_back(p7, p0, p4, leftWallColor);
    // right wall (green)
    sd.triangles.emplace_back(p1, p6, p2, rightWallColor);
    sd.triangles.emplace_back(p6, p1, p5, rightWallColor);
    // front wall intentionally omitted — open side the camera looks through
    // back wall
    sd.triangles.emplace_back(p2, p7, p3, white);
    sd.triangles.emplace_back(p7, p2, p6, white);

    sd.meshes.emplace_back(0, (int)sd.triangles.size(), -1, glm::vec3(-1) * scale, glm::vec3(1) * scale);

    // --- ceiling light --- (sits just below the ceiling plane at y = -scale)
    float lightHalfX = scale * 0.25f;
    float lightHalfZ = scale * 0.25f;
    float lightY = -scale + 0.5f; // offset toward room center to avoid z-fighting
    glm::vec3 lightColor = glm::vec3(1.f, 0.92f, 0.75f);
    float lightLuminance = 15.f;

    glm::vec3 l0 = {-lightHalfX, lightY, -lightHalfZ};
    glm::vec3 l1 = {+lightHalfX, lightY, -lightHalfZ};
    glm::vec3 l2 = {+lightHalfX, lightY, +lightHalfZ};
    glm::vec3 l3 = {-lightHalfX, lightY, +lightHalfZ};

    int lightStart = (int)sd.triangles.size();
    sd.triangles.emplace_back(l0, l2, l1, lightColor, glm::vec2(-1), glm::vec2(-1), glm::vec2(-1), lightLuminance);
    sd.triangles.emplace_back(l2, l0, l3, lightColor, glm::vec2(-1), glm::vec2(-1), glm::vec2(-1), lightLuminance);
    sd.meshes.emplace_back(lightStart, (int)sd.triangles.size() - lightStart, -1,
                            glm::vec3(-lightHalfX, lightY - 1.f, -lightHalfZ),
                            glm::vec3(+lightHalfX, lightY + 1.f, +lightHalfZ));

    // --- short box (front-right) --- sits on the floor (y = +scale), extends toward the ceiling (-Y)
    addBox(sd, glm::vec3(30.f, scale - 35.f, -20.f), glm::vec3(35.f, 35.f, 35.f), -18.f, white);

    // --- tall box (back-left) ---
    addBox(sd, glm::vec3(-30.f, scale - 65.f, 25.f), glm::vec3(35.f, 65.f, 35.f), 18.f, white);

    return sd;
}

struct TradRenderer : Layer
{
    Shader shader;
    GPUBuffer VBOs[settings::maxFramesInFlight];
    GPUBuffer UBOs[settings::maxFramesInFlight];
    GPUBuffer IBOs[settings::maxFramesInFlight];
    
    struct Vertex
    {
        float x;
        float y;
        float z;
        glm::vec4 col;
        glm::vec2 uv;
        int tex_idx;
    };

    struct Uniform
    {
        glm::mat4 proj;
        glm::mat4 view;
        int window_width;
        int window_height;
        float t;
    } u;

    std::vector<Vertex> vertices  = {};
    std::vector<uint32_t> indices = {};

    SceneDescription * scene    = VK_NULL_HANDLE;
    Camera * cam                = nullptr; 

    void init(VulkanHandler* VKH, Window * w, LayerEventHandler * EH, WindowRescources * res) override
    {
        this->VKH = VKH; win = w; this->updateFrequency = updateFrequency;

        shader.init(VKH, win);

        shader.pushInputLayoutBinding(VertexInputBindingelement{VK_FORMAT_R32G32B32_SFLOAT, 1, sizeof(float)*3});
        shader.pushInputLayoutBinding(VertexInputBindingelement{VK_FORMAT_R32G32B32A32_SFLOAT, 1, sizeof(float)*4});
        shader.pushInputLayoutBinding(VertexInputBindingelement{VK_FORMAT_R32G32_SFLOAT, 1, sizeof(float)*2});
        shader.pushInputLayoutBinding(VertexInputBindingelement{VK_FORMAT_R32_SINT, 1, sizeof(int)});
        shader.setupInputLayout();

        shader.pushUnfiormLayoutBinding(UniformLayoutBindingelement{1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT});
        shader.pushUnfiormLayoutBinding(UniformLayoutBindingelement{1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT});
        shader.pushUnfiormLayoutBinding(UniformLayoutBindingelement{1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT});
        shader.setupUnfiormLayout();

        shader.createGraphicsPipeline("shaders/basic_vert.spv", "shaders/basic_frag.spv");
    
        for (uint i = 0; i < settings::maxFramesInFlight; i++)
        {
            VBOs[i].createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof(Vertex) * 128, win->graphicsQueue, win->Vk, win->commandPool);
            IBOs[i].createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, sizeof(uint32_t) * 256, win->graphicsQueue, win->Vk, win->commandPool);
            UBOs[i].createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(Uniform), win->graphicsQueue, win->Vk, win->commandPool);
        }

        u = Uniform{glm::perspective(45.f, (float)win->currWinW / (float)win->currWinH, 0.1f, 1000.f), cam->getView(), win->currWinW, win->currWinH, 1.f};
        shader.updateUniformUBOs(UBOs, sizeof(Uniform), settings::maxFramesInFlight);
    }
    
    void destroy() override
    {
        for (uint i = 0; i < settings::maxFramesInFlight; i++)
        {
            VBOs[i].destroyBuffer();
            IBOs[i].destroyBuffer();
            UBOs[i].destroyBuffer();
        }

        shader.destroy();
    }

    void setContext(SceneDescription * scene, Camera * cam)
    {
        this->scene = scene;
        this->cam = cam;
    }

    void handle_event(LayerEvent ev) override {}

    void onrecreate_swapchain() override {}

    void setUpdateFrequency(int freq) override { updateFrequency = freq; }

    void draw(uint32_t imageIndex, int currentFrameIndex) override
    {
        // add vertices
        int k = 0;
        for (Mesh& mesh : scene->meshes)
        {
            for (int i = 0; i < mesh.numTriangles; i++)
            {
                Triangle& t = scene->triangles[mesh.startTriangles + i];
                vertices.emplace_back(t.a.x, t.a.y, t.a.z, glm::vec4(t.color, 1.f), t.uva, -2);
                vertices.emplace_back(t.b.x, t.b.y, t.b.z, glm::vec4(t.color, 1.f), t.uvb, -2);
                vertices.emplace_back(t.c.x, t.c.y, t.c.z, glm::vec4(t.color, 1.f), t.uvc, -2);
            
                indices.emplace_back(0 + 3*k);
                indices.emplace_back(1 + 3*k);
                indices.emplace_back(2 + 3*k);
                k++;
            }
        }

        // writeGPU
        VBOs[currentFrameIndex].writeToBuffer(vertices.data(), sizeof(Vertex) * vertices.size(), win->commandBuffers[currentFrameIndex]);
        IBOs[currentFrameIndex].writeToBuffer(indices.data(), sizeof(uint32_t) * indices.size(), win->commandBuffers[currentFrameIndex]);
        UBOs[currentFrameIndex].writeToBuffer(&u, sizeof(Uniform), win->commandBuffers[currentFrameIndex]);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo.imageView = win->FB_ImgViews[win->currentFrameIndex];
        imageInfo.sampler = win->FB_sampler;

        VkWriteDescriptorSet ds;
        ds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        ds.descriptorCount = 1;
        ds.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        ds.pImageInfo = &imageInfo;
        ds.dstSet = shader.getDescriptorSets()[win->currentFrameIndex];
        ds.dstArrayElement = 0;
        ds.pNext = nullptr;
        ds.dstBinding = 1;

        shader.updateUniform(ds);

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.05, 0.05, 0.05f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        VkViewport viewport{};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = win->swapchainExtent.width;
        viewport.height = win->swapchainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {win->swapchainExtent.width, win->swapchainExtent.height};

        BeginRenderPass(win, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, clearValues, scissor, viewport);
        vkCmdSetViewport(win->commandBuffers[currentFrameIndex], 0, 1, &viewport);
        vkCmdSetScissor(win->commandBuffers[currentFrameIndex], 0, 1, &scissor);

        VkBuffer vbo[] = {VBOs[currentFrameIndex].getHandle()};
        VkDeviceSize offsets[] = {0};

        vkCmdBindIndexBuffer(win->commandBuffers[currentFrameIndex], IBOs[currentFrameIndex].getHandle(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindVertexBuffers(win->commandBuffers[currentFrameIndex], 0, 1, vbo, offsets);
        vkCmdBindPipeline(win->commandBuffers[currentFrameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, shader.getPipeline());
        vkCmdBindDescriptorSets(win->commandBuffers[currentFrameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, shader.getPipelineLayout(), 0, 1, &shader.getDescriptorSets()[currentFrameIndex], 0, nullptr);
        vkCmdDrawIndexed(win->commandBuffers[currentFrameIndex], indices.size(), 1, 0, 0, 0);

        EndRenderPass(win);

        vertices.clear();
        indices.clear(); 
    }

    bool tab_state[2] = {true, true};
    bool enter_state[2] = {true, true};

    void update(LayerEventHandler * EH, float t, float dt) override
    {
        cam->updateCamera(win, dt);
        u = Uniform{glm::perspective(45.f, (float)win->currWinW / (float)win->currWinH, 0.1f, 1000.f), cam->getView(), win->currWinW, win->currWinH, 1.f};

        enter_state[1] = enter_state[0];
        enter_state[0] = win->keyboardState[GLFW_KEY_ENTER];

        if (enter_state[0] && !enter_state[1])
        {
            if (glfwGetInputMode(win->winHandle, GLFW_CURSOR) == GLFW_CURSOR_DISABLED)
                glfwSetInputMode(win->winHandle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            else
                glfwSetInputMode(win->winHandle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }

        tab_state[1] = tab_state[0];
        tab_state[0] = win->keyboardState[GLFW_KEY_TAB];

        if (tab_state[0] && !tab_state[1])
        {
            glfwSetInputMode(win->winHandle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            EH->events.push(LayerEvent{SWITCH_TO_NEXT_STACK, this});
        }
    }
};


int main()
{
    GLFWHandler WH;
    WH.init();
    
    VulkanHandler VKH;
    VKH.init();

    // lifetime manged by LayerHandler i. e. it must be created on the heap or at leaast thats the cleanest solution i found
    auto unit = WH.getNewWindowUnit(VKH);
    unit->first->create(settings::width, settings::height, "Solver", false, &VKH, 0, "imgs/vulkan-icon.png", true, -1, -1);

    SceneDescription scene = getScene();

    Camera cam;
    cam.init(glm::vec3(-78, -34, -284), glm::vec3(0,0,0), glm::vec3(0,1,0));
        
    TradRenderer * trad = new TradRenderer();
    trad->setContext(&scene, &cam);

    LayerStack stack[2];
    stack[0].addLayer(trad);
    stack[1].addLayer(new WaveSim());
    stack[1].addLayer(new WaveSimController());

    unit->second.addStack(stack[0]);
    unit->second.addStack(stack[1]);

    WH.prepareForMainLoop(VKH);
    
    while (!WH.shouldClose())
    {
        WH.handle(VKH);
    }

    vkDeviceWaitIdle(VKH.device);
    WH.destroy();
    VKH.destroy();
}
