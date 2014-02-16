#include "SDL_gpu.h"
#include "SDL_platform.h"

#ifdef __ANDROID__
#include <android/log.h>
#endif

#include "stb_image.h"


#define CHECK_RENDERER (current_renderer != NULL)
#define CHECK_CONTEXT (current_renderer->current_context_target != NULL)
#define CHECK_FUNCTION_POINTER(fn) (current_renderer->fn != NULL)
#define RETURN_ERROR(code, details) do{ GPU_PushErrorCode(__func__, code, details); return; } while(0)

void GPU_InitRendererRegister(void);

static GPU_Renderer* current_renderer = NULL;

static GPU_DebugLevelEnum debug_level = GPU_DEBUG_LEVEL_0;

#define GPU_MAX_NUM_ERRORS 30
static GPU_ErrorObject error_code_stack[GPU_MAX_NUM_ERRORS];
static int num_error_codes = 0;

void GPU_SetCurrentRenderer(GPU_RendererID id)
{
	current_renderer = GPU_GetRendererByID(id);
	
	if(current_renderer != NULL && current_renderer->SetAsCurrent != NULL)
		current_renderer->SetAsCurrent(current_renderer);
}

GPU_Renderer* GPU_GetCurrentRenderer(void)
{
	return current_renderer;
}

Uint32 GPU_GetCurrentShaderProgram(void)
{
    if(current_renderer == NULL || current_renderer->current_context_target == NULL)
        return 0;
    
    return current_renderer->current_context_target->context->current_shader_program;
}



void GPU_LogInfo(const char* format, ...)
{
#ifdef SDL_GPU_ENABLE_LOG
	va_list args;
	va_start(args, format);
	#ifdef __ANDROID__
		__android_log_vprint((GPU_GetDebugLevel() >= GPU_DEBUG_LEVEL_3? ANDROID_LOG_ERROR : ANDROID_LOG_INFO), "APPLICATION", format, args);
	#else
		vfprintf((GPU_GetDebugLevel() >= GPU_DEBUG_LEVEL_3? stderr : stdout), format, args);
	#endif
	va_end(args);
#endif
}

void GPU_LogWarning(const char* format, ...)
{
#ifdef SDL_GPU_ENABLE_LOG
	va_list args;
	va_start(args, format);
	#ifdef __ANDROID__
		__android_log_vprint((GPU_GetDebugLevel() >= GPU_DEBUG_LEVEL_2? ANDROID_LOG_ERROR : ANDROID_LOG_WARN), "APPLICATION", format, args);
	#else
		vfprintf((GPU_GetDebugLevel() >= GPU_DEBUG_LEVEL_2? stderr : stdout), format, args);
	#endif
	va_end(args);
#endif
}

void GPU_LogError(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	#ifdef __ANDROID__
		__android_log_vprint(ANDROID_LOG_ERROR, "APPLICATION", format, args);
	#else
		vfprintf(stderr, format, args);
	#endif
	va_end(args);
}


static Uint8 init_SDL(void)
{
	if(GPU_GetNumActiveRenderers() == 0)
	{
	    Uint32 subsystems = SDL_WasInit(SDL_INIT_EVERYTHING);
	    if(!subsystems)
        {
            // Nothing has been set up, so init SDL and the video subsystem.
            if(SDL_Init(SDL_INIT_VIDEO) < 0)
            {
                GPU_PushErrorCode("GPU_Init", GPU_ERROR_BACKEND_ERROR, "Failed to initialize SDL video subsystem");
                return 0;
            }
        }
        else if(!(subsystems & SDL_INIT_VIDEO))
        {
            // Something already set up SDL, so just init video.
            if(SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
            {
                GPU_PushErrorCode("GPU_Init", GPU_ERROR_BACKEND_ERROR, "Failed to initialize SDL video subsystem");
                return 0;
            }
        }
	}
	return 1;
}

static Uint32 init_windowID = 0;

void GPU_SetInitWindow(Uint32 windowID)
{
    init_windowID = windowID;
}

Uint32 GPU_GetInitWindow(void)
{
    return init_windowID;
}

static GPU_InitFlagEnum preinit_flags = GPU_DEFAULT_INIT_FLAGS;

void GPU_SetPreInitFlags(GPU_InitFlagEnum GPU_flags)
{
    preinit_flags = GPU_flags;
}

GPU_InitFlagEnum GPU_GetPreInitFlags(void)
{
    return preinit_flags;
}


GPU_Target* GPU_Init(Uint16 w, Uint16 h, GPU_WindowFlagEnum SDL_flags)
{
	GPU_InitRendererRegister();
	
	if(!init_SDL())
        return NULL;
        
    int renderer_order_size = 0;
    GPU_RendererID renderer_order[GPU_RENDERER_ORDER_MAX];
    GPU_GetRendererOrder(&renderer_order_size, renderer_order);
	
    // Init the renderers in order
    int i;
    for(i = 0; i < renderer_order_size; i++)
    {
        GPU_Target* screen = GPU_InitRendererByID(renderer_order[i], w, h, SDL_flags);
        if(screen != NULL)
            return screen;
    }
    
    return NULL;
}

GPU_Target* GPU_InitRenderer(GPU_RendererEnum renderer_enum, Uint16 w, Uint16 h, GPU_WindowFlagEnum SDL_flags)
{
    return GPU_InitRendererByID(GPU_MakeRendererID(renderer_enum, 0, 0), w, h, SDL_flags);
}

GPU_Target* GPU_InitRendererByID(GPU_RendererID renderer_request, Uint16 w, Uint16 h, GPU_WindowFlagEnum SDL_flags)
{
	GPU_InitRendererRegister();
	
	if(!init_SDL())
        return NULL;
	
	GPU_Renderer* renderer = GPU_AddRenderer(renderer_request);
	if(renderer == NULL || renderer->Init == NULL)
		return NULL;
    
	GPU_SetCurrentRenderer(renderer->id);
	
	GPU_Target* screen = renderer->Init(renderer, renderer_request, w, h, SDL_flags);
	if(screen == NULL)
    {
        // Init failed, destroy the renderer...
        GPU_CloseCurrentRenderer();
    }
    else
        GPU_SetInitWindow(0);
    return screen;
}

Uint8 GPU_IsFeatureEnabled(GPU_FeatureEnum feature)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->IsFeatureEnabled == NULL)
		return 0;
	
	return current_renderer->IsFeatureEnabled(current_renderer, feature);
}

GPU_Target* GPU_CreateTargetFromWindow(Uint32 windowID)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->CreateTargetFromWindow == NULL)
		return NULL;
	
	return current_renderer->CreateTargetFromWindow(current_renderer, windowID, NULL);
}

GPU_Target* GPU_CreateAliasTarget(GPU_Target* target)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->CreateAliasTarget == NULL)
		return NULL;
	
	return current_renderer->CreateAliasTarget(current_renderer, target);
}

void GPU_MakeCurrent(GPU_Target* target, Uint32 windowID)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->MakeCurrent == NULL)
		return;
	
	current_renderer->MakeCurrent(current_renderer, target, windowID);
}

Uint8 GPU_ToggleFullscreen(Uint8 use_desktop_resolution)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->ToggleFullscreen == NULL)
		return 0;
	
	return current_renderer->ToggleFullscreen(current_renderer, use_desktop_resolution);
}

Uint8 GPU_SetWindowResolution(Uint16 w, Uint16 h)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->SetWindowResolution == NULL || w == 0 || h == 0)
		return 0;
	
	return current_renderer->SetWindowResolution(current_renderer, w, h);
}


void GPU_SetVirtualResolution(GPU_Target* target, Uint16 w, Uint16 h)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->SetVirtualResolution == NULL || w == 0 || h == 0)
		return;
	
	current_renderer->SetVirtualResolution(current_renderer, target, w, h);
}

void GPU_UnsetVirtualResolution(GPU_Target* target)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->UnsetVirtualResolution == NULL)
		return;
	
	current_renderer->UnsetVirtualResolution(current_renderer, target);
}

void GPU_CloseCurrentRenderer(void)
{
	if(current_renderer == NULL)
		return;
	
	if(current_renderer->Quit != NULL)
		current_renderer->Quit(current_renderer);
	GPU_RemoveRenderer(current_renderer->id);
	current_renderer = NULL;
}

void GPU_Quit(void)
{
    if(num_error_codes > 0 && GPU_GetDebugLevel() >= GPU_DEBUG_LEVEL_1)
        GPU_LogError("GPU_Quit: %d uncleared errors.\n", num_error_codes);
    
	// FIXME: Remove all renderers
	if(current_renderer == NULL)
		return;
	
	if(current_renderer->Quit != NULL)
		current_renderer->Quit(current_renderer);
	GPU_RemoveRenderer(current_renderer->id);
	
	if(GPU_GetNumActiveRenderers() == 0)
		SDL_Quit();
}

void GPU_SetDebugLevel(GPU_DebugLevelEnum level)
{
    if(level < 0)
        level = GPU_DEBUG_LEVEL_0;
    else if(level > GPU_DEBUG_LEVEL_MAX)
        level = GPU_DEBUG_LEVEL_MAX;
    debug_level = level;
}

GPU_DebugLevelEnum GPU_GetDebugLevel(void)
{
    return debug_level;
}

void GPU_PushErrorCode(const char* function, GPU_ErrorEnum error, const char* details)
{
    if(GPU_GetDebugLevel() >= GPU_DEBUG_LEVEL_1)
    {
        // Print the message
        if(details != NULL)
            GPU_LogError("%s: %s - %s\n", (function == NULL? "NULL" : function), GPU_GetErrorString(error), details);
        else
            GPU_LogError("%s: %s\n", (function == NULL? "NULL" : function), GPU_GetErrorString(error));
    }
    
    if(num_error_codes < GPU_MAX_NUM_ERRORS)
    {
        GPU_ErrorObject e = {function, error, details};
        error_code_stack[num_error_codes] = e;
        num_error_codes++;
    }
}

GPU_ErrorObject GPU_PopErrorCode(void)
{
    if(num_error_codes <= 0)
    {
        GPU_ErrorObject e = {NULL, GPU_ERROR_NONE, NULL};
        return e;
    }
    
    return error_code_stack[--num_error_codes];
}

const char* GPU_GetErrorString(GPU_ErrorEnum error)
{
    switch(error)
    {
        case GPU_ERROR_NONE:
            return "NO ERROR";
        case GPU_ERROR_BACKEND_ERROR:
            return "BACKEND ERROR";
        case GPU_ERROR_DATA_ERROR:
            return "DATA ERROR";
        case GPU_ERROR_USER_ERROR:
            return "USER ERROR";
        case GPU_ERROR_UNSUPPORTED_FUNCTION:
            return "UNSUPPORTED FUNCTION";
        case GPU_ERROR_NULL_ARGUMENT:
            return "NULL ARGUMENT";
        case GPU_ERROR_FILE_NOT_FOUND:
            return "FILE NOT FOUND";
    }
    return "UNKNOWN ERROR";
}


void GPU_GetVirtualCoords(GPU_Target* target, float* x, float* y, float displayX, float displayY)
{
	if(target == NULL)
		return;
	
	if(target->context != NULL)
    {
        if(x != NULL)
            *x = (displayX*target->w)/target->context->window_w;
        if(y != NULL)
            *y = (displayY*target->h)/target->context->window_h;
    }
	else if(target->image != NULL)
    {
        if(x != NULL)
            *x = (displayX*target->w)/target->image->w;
        if(y != NULL)
            *y = (displayY*target->h)/target->image->h;
    }
    else
    {
        if(x != NULL)
            *x = displayX;
        if(y != NULL)
            *y = displayY;
    }
}

GPU_Rect GPU_MakeRect(float x, float y, float w, float h)
{
    GPU_Rect r = {x, y, w, h};
    return r;
}

GPU_RendererID GPU_MakeRendererID(GPU_RendererEnum id, int major_version, int minor_version)
{
    GPU_RendererID r = {id, major_version, minor_version, -1};
    return r;
}

void GPU_SetViewport(GPU_Target* target, GPU_Rect viewport)
{
    if(target != NULL)
        target->viewport = viewport;
}

GPU_Camera GPU_GetDefaultCamera(void)
{
	GPU_Camera cam = {0.0f, 0.0f, -10.0f, 0.0f, 1.0f};
	return cam;
}

GPU_Camera GPU_GetCamera(void)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL)
		return GPU_GetDefaultCamera();
	return current_renderer->current_context_target->camera;
}

GPU_Camera GPU_SetCamera(GPU_Target* target, GPU_Camera* cam)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->SetCamera == NULL)
		return GPU_GetDefaultCamera();
	
	return current_renderer->SetCamera(current_renderer, target, cam);
}

GPU_Image* GPU_CreateImage(Uint16 w, Uint16 h, Uint8 channels)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->CreateImage == NULL)
		return NULL;
	
	return current_renderer->CreateImage(current_renderer, w, h, channels);
}

GPU_Image* GPU_LoadImage(const char* filename)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->LoadImage == NULL)
		return NULL;
	
	return current_renderer->LoadImage(current_renderer, filename);
}

GPU_Image* GPU_CreateAliasImage(GPU_Image* image)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->CreateAliasImage == NULL)
		return NULL;
	
	return current_renderer->CreateAliasImage(current_renderer, image);
}

Uint8 GPU_SaveImage(GPU_Image* image, const char* filename)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->SaveImage == NULL)
		return 0;
	
	return current_renderer->SaveImage(current_renderer, image, filename);
}

GPU_Image* GPU_CopyImage(GPU_Image* image)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->CopyImage == NULL)
		return NULL;
	
	return current_renderer->CopyImage(current_renderer, image);
}

void GPU_UpdateImage(GPU_Image* image, const GPU_Rect* rect, SDL_Surface* surface)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->UpdateImage == NULL)
		return;
	
	current_renderer->UpdateImage(current_renderer, image, rect, surface);
}

SDL_Surface* GPU_LoadSurface(const char* filename)
{
	int width, height, channels;
	Uint32 Rmask, Gmask, Bmask, Amask = 0;
	
	if(filename == NULL)
    {
        GPU_PushErrorCode("GPU_LoadSurface", GPU_ERROR_NULL_ARGUMENT, "filename");
        return NULL;
    }
	
	#ifdef __ANDROID__
	unsigned char* data;
	if(strlen(filename) > 0 && filename[0] != '/')
	{
        // Must use SDL_RWops to access the assets directory automatically
        SDL_RWops* rwops = SDL_RWFromFile(filename, "r");
        if(rwops == NULL)
            return NULL;
        int data_bytes = SDL_RWseek(rwops, 0, SEEK_END);
        SDL_RWseek(rwops, 0, SEEK_SET);
        unsigned char* c_data = (unsigned char*)malloc(data_bytes);
        SDL_RWread(rwops, c_data, 1, data_bytes);
        data = stbi_load_from_memory(c_data, data_bytes, &width, &height, &channels, 0);
        free(c_data);
        SDL_FreeRW(rwops);
	}
	else
    {
        // Absolute filename
        data = stbi_load(filename, &width, &height, &channels, 0);
    }
	#else
	unsigned char* data = stbi_load(filename, &width, &height, &channels, 0);
	#endif
	
	if(data == NULL)
	{
		GPU_PushErrorCode(__func__, GPU_ERROR_DATA_ERROR, stbi_failure_reason());
		return NULL;
	}
	if(channels < 3 || channels > 4)
	{
		GPU_PushErrorCode(__func__, GPU_ERROR_DATA_ERROR, "Unsupported pixel format");
		stbi_image_free(data);
		return NULL;
	}
	
	if(channels == 3)
	{
	    // These are reversed from what SDL_image uses...  That is bad. :(  Needs testing.
	    #if SDL_BYTEORDER == SDL_BIG_ENDIAN
		Rmask = 0xff0000;
		Gmask = 0x00ff00;
		Bmask = 0x0000ff;
		#else
		Rmask = 0x0000ff;
		Gmask = 0x00ff00;
		Bmask = 0xff0000;
		#endif
	}
	else
	{
		Rmask = 0x000000ff;
		Gmask = 0x0000ff00;
		Bmask = 0x00ff0000;
		Amask = 0xff000000;
	}
	
	SDL_Surface* result = SDL_CreateRGBSurfaceFrom(data, width, height, channels*8, width*channels, Rmask, Gmask, Bmask, Amask);
	
	return result;
}

#include "stb_image.h"
#include "stb_image_write.h"

// From http://stackoverflow.com/questions/5309471/getting-file-extension-in-c
static const char *get_filename_ext(const char *filename)
{
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename)
        return "";
    return dot + 1;
}

Uint8 GPU_SaveSurface(SDL_Surface* surface, const char* filename)
{
    const char* extension;
    Uint8 result;
    unsigned char* data;

    if(surface == NULL || filename == NULL ||
            surface->w < 1 || surface->h < 1)
    {
        return 0;
    }

    extension = get_filename_ext(filename);

    data = surface->pixels;

    if(SDL_strcasecmp(extension, "png") == 0)
        result = stbi_write_png(filename, surface->w, surface->h, surface->format->BytesPerPixel, (const unsigned char *const)data, 0);
    else if(SDL_strcasecmp(extension, "bmp") == 0)
        result = stbi_write_bmp(filename, surface->w, surface->h, surface->format->BytesPerPixel, (void*)data);
    else if(SDL_strcasecmp(extension, "tga") == 0)
        result = stbi_write_tga(filename, surface->w, surface->h, surface->format->BytesPerPixel, (void*)data);
    else
    {
        GPU_PushErrorCode(__func__, GPU_ERROR_DATA_ERROR, "Unsupported output file format");
        result = 0;
    }

    return result;
}

GPU_Image* GPU_CopyImageFromSurface(SDL_Surface* surface)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->CopyImageFromSurface == NULL)
		return NULL;
	
	return current_renderer->CopyImageFromSurface(current_renderer, surface);
}

GPU_Image* GPU_CopyImageFromTarget(GPU_Target* target)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->CopyImageFromTarget == NULL)
		return NULL;
	
	return current_renderer->CopyImageFromTarget(current_renderer, target);
}

SDL_Surface* GPU_CopySurfaceFromTarget(GPU_Target* target)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->CopySurfaceFromTarget == NULL)
		return NULL;
	
	return current_renderer->CopySurfaceFromTarget(current_renderer, target);
}

SDL_Surface* GPU_CopySurfaceFromImage(GPU_Image* image)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->CopySurfaceFromImage == NULL)
		return NULL;
	
	return current_renderer->CopySurfaceFromImage(current_renderer, image);
}

void GPU_FreeImage(GPU_Image* image)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->FreeImage == NULL)
		return;
	
	current_renderer->FreeImage(current_renderer, image);
}


void GPU_SubSurfaceCopy(SDL_Surface* src, GPU_Rect* srcrect, GPU_Target* dest, Sint16 x, Sint16 y)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->SubSurfaceCopy == NULL)
        return;
	
	current_renderer->SubSurfaceCopy(current_renderer, src, srcrect, dest, x, y);
}

GPU_Target* GPU_GetContextTarget(void)
{
	if(current_renderer == NULL)
		return NULL;
	
	return current_renderer->current_context_target;
}


GPU_Target* GPU_LoadTarget(GPU_Image* image)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->LoadTarget == NULL)
		return NULL;
	
	return current_renderer->LoadTarget(current_renderer, image);
}



void GPU_FreeTarget(GPU_Target* target)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->FreeTarget == NULL)
		return;
	
	current_renderer->FreeTarget(current_renderer, target);
}



void GPU_Blit(GPU_Image* image, GPU_Rect* src_rect, GPU_Target* target, float x, float y)
{
    if(!CHECK_RENDERER)
        RETURN_ERROR(GPU_ERROR_USER_ERROR, "NULL renderer");
    if(!CHECK_CONTEXT)
        RETURN_ERROR(GPU_ERROR_USER_ERROR, "NULL context");
    if(!CHECK_FUNCTION_POINTER(Blit))
        RETURN_ERROR(GPU_ERROR_UNSUPPORTED_FUNCTION, NULL);
    
	if(image == NULL)
        RETURN_ERROR(GPU_ERROR_NULL_ARGUMENT, "image");
	if(target == NULL)
        RETURN_ERROR(GPU_ERROR_NULL_ARGUMENT, "target");
	
	current_renderer->Blit(current_renderer, image, src_rect, target, x, y);
}


void GPU_BlitRotate(GPU_Image* image, GPU_Rect* src_rect, GPU_Target* target, float x, float y, float angle)
{
    if(!CHECK_RENDERER)
        RETURN_ERROR(GPU_ERROR_USER_ERROR, "NULL renderer");
    if(!CHECK_CONTEXT)
        RETURN_ERROR(GPU_ERROR_USER_ERROR, "NULL context");
    if(!CHECK_FUNCTION_POINTER(BlitRotate))
        RETURN_ERROR(GPU_ERROR_UNSUPPORTED_FUNCTION, NULL);
    
	if(image == NULL)
        RETURN_ERROR(GPU_ERROR_NULL_ARGUMENT, "image");
	if(target == NULL)
        RETURN_ERROR(GPU_ERROR_NULL_ARGUMENT, "target");
	
	current_renderer->BlitRotate(current_renderer, image, src_rect, target, x, y, angle);
}

void GPU_BlitScale(GPU_Image* image, GPU_Rect* src_rect, GPU_Target* target, float x, float y, float scaleX, float scaleY)
{
    if(!CHECK_RENDERER)
        RETURN_ERROR(GPU_ERROR_USER_ERROR, "NULL renderer");
    if(!CHECK_CONTEXT)
        RETURN_ERROR(GPU_ERROR_USER_ERROR, "NULL context");
    if(!CHECK_FUNCTION_POINTER(BlitScale))
        RETURN_ERROR(GPU_ERROR_UNSUPPORTED_FUNCTION, NULL);
    
	if(image == NULL)
        RETURN_ERROR(GPU_ERROR_NULL_ARGUMENT, "image");
	if(target == NULL)
        RETURN_ERROR(GPU_ERROR_NULL_ARGUMENT, "target");
	
	current_renderer->BlitScale(current_renderer, image, src_rect, target, x, y, scaleX, scaleY);
}

void GPU_BlitTransform(GPU_Image* image, GPU_Rect* src_rect, GPU_Target* target, float x, float y, float angle, float scaleX, float scaleY)
{
    if(!CHECK_RENDERER)
        RETURN_ERROR(GPU_ERROR_USER_ERROR, "NULL renderer");
    if(!CHECK_CONTEXT)
        RETURN_ERROR(GPU_ERROR_USER_ERROR, "NULL context");
    if(!CHECK_FUNCTION_POINTER(BlitTransform))
        RETURN_ERROR(GPU_ERROR_UNSUPPORTED_FUNCTION, NULL);
    
	if(image == NULL)
        RETURN_ERROR(GPU_ERROR_NULL_ARGUMENT, "image");
	if(target == NULL)
        RETURN_ERROR(GPU_ERROR_NULL_ARGUMENT, "target");
	
	current_renderer->BlitTransform(current_renderer, image, src_rect, target, x, y, angle, scaleX, scaleY);
}

void GPU_BlitTransformX(GPU_Image* image, GPU_Rect* src_rect, GPU_Target* target, float x, float y, float pivot_x, float pivot_y, float angle, float scaleX, float scaleY)
{
    if(!CHECK_RENDERER)
        RETURN_ERROR(GPU_ERROR_USER_ERROR, "NULL renderer");
    if(!CHECK_CONTEXT)
        RETURN_ERROR(GPU_ERROR_USER_ERROR, "NULL context");
    if(!CHECK_FUNCTION_POINTER(BlitTransformX))
        RETURN_ERROR(GPU_ERROR_UNSUPPORTED_FUNCTION, NULL);
    
	if(image == NULL)
        RETURN_ERROR(GPU_ERROR_NULL_ARGUMENT, "image");
	if(target == NULL)
        RETURN_ERROR(GPU_ERROR_NULL_ARGUMENT, "target");
	
	current_renderer->BlitTransformX(current_renderer, image, src_rect, target, x, y, pivot_x, pivot_y, angle, scaleX, scaleY);
}

void GPU_BlitTransformMatrix(GPU_Image* image, GPU_Rect* src_rect, GPU_Target* target, float x, float y, float* matrix3x3)
{
    if(!CHECK_RENDERER)
        RETURN_ERROR(GPU_ERROR_USER_ERROR, "NULL renderer");
    if(!CHECK_CONTEXT)
        RETURN_ERROR(GPU_ERROR_USER_ERROR, "NULL context");
    if(!CHECK_FUNCTION_POINTER(BlitTransformMatrix))
        RETURN_ERROR(GPU_ERROR_UNSUPPORTED_FUNCTION, NULL);
    
	if(image == NULL)
        RETURN_ERROR(GPU_ERROR_NULL_ARGUMENT, "image");
	if(target == NULL)
        RETURN_ERROR(GPU_ERROR_NULL_ARGUMENT, "target");
    
    if(matrix3x3 == NULL)
		return;
	
	current_renderer->BlitTransformMatrix(current_renderer, image, src_rect, target, x, y, matrix3x3);
}

void GPU_BlitBatch(GPU_Image* image, GPU_Target* target, unsigned int num_sprites, float* values, GPU_BlitFlagEnum flags)
{
    if(!CHECK_RENDERER)
        RETURN_ERROR(GPU_ERROR_USER_ERROR, "NULL renderer");
    if(!CHECK_CONTEXT)
        RETURN_ERROR(GPU_ERROR_USER_ERROR, "NULL context");
    if(!CHECK_FUNCTION_POINTER(BlitBatch))
        RETURN_ERROR(GPU_ERROR_UNSUPPORTED_FUNCTION, NULL);
    
	if(image == NULL)
        RETURN_ERROR(GPU_ERROR_NULL_ARGUMENT, "image");
	if(target == NULL)
        RETURN_ERROR(GPU_ERROR_NULL_ARGUMENT, "target");
    
    if(num_sprites == 0)
        return;
    
    // Is it already in the right format?
    if((flags & GPU_PASSTHROUGH_ALL) == GPU_PASSTHROUGH_ALL || values == NULL)
    {
        current_renderer->BlitBatch(current_renderer, image, target, num_sprites, values, flags);
        return;
    }
	
	// Conversion time...
	
	// Convert condensed interleaved format into full interleaved format for the renderer to use.
	// Condensed: Each vertex has 2 pos, 4 rect, 4 color
	
	// Default values: Each sprite is defined by a position, a rect, and a color.
	int src_position_floats_per_sprite = 2;
	int src_rect_floats_per_sprite = 4;
	int src_color_floats_per_sprite = 4;
	
	Uint8 no_positions = (flags & GPU_USE_DEFAULT_POSITIONS);
	Uint8 no_rects = (flags & GPU_USE_DEFAULT_SRC_RECTS);
	Uint8 no_colors = (flags & GPU_USE_DEFAULT_COLORS);
	Uint8 pass_vertices = (flags & GPU_PASSTHROUGH_VERTICES);
	Uint8 pass_texcoords = (flags & GPU_PASSTHROUGH_TEXCOORDS);
	Uint8 pass_colors = (flags & GPU_PASSTHROUGH_COLORS);
	
	// Passthrough data is per-vertex.  Non-passthrough is per-sprite.  They can't interleave cleanly.
	if(flags & GPU_PASSTHROUGH_ALL && (flags & GPU_PASSTHROUGH_ALL) != GPU_PASSTHROUGH_ALL)
    {
        GPU_PushErrorCode(__func__, GPU_ERROR_USER_ERROR, "Cannot interpret interleaved data using partial passthrough");
        return;
    }
	
	if(pass_vertices)
        src_position_floats_per_sprite = 8; // 4 vertices of x, y
	if(pass_texcoords)
        src_rect_floats_per_sprite = 8; // 4 vertices of s, t
	if(pass_colors)
        src_color_floats_per_sprite = 16; // 4 vertices of r, g, b, a
	if(no_positions)
        src_position_floats_per_sprite = 0;
	if(no_rects)
        src_rect_floats_per_sprite = 0;
	if(no_colors)
        src_color_floats_per_sprite = 0;
    
	int src_floats_per_sprite = src_position_floats_per_sprite + src_rect_floats_per_sprite + src_color_floats_per_sprite;
	
	int size = num_sprites*(8 + 8 + 16);
	float* new_values = (float*)malloc(sizeof(float)*size);
    
	int n;  // The sprite number iteration variable.
	// Source indices (per sprite)
	int pos_n = 0;
	int rect_n = src_position_floats_per_sprite;
	int color_n = src_position_floats_per_sprite + src_rect_floats_per_sprite;
	// Dest indices
	int vert_i = 0;
	int texcoord_i = 2;
	int color_i = 4;
	// Dest float stride
	int floats_per_vertex = 8;
	
	float w2 = 0.5f*image->w;  // texcoord helpers for position expansion
	float h2 = 0.5f*image->h;
	
	float tex_w = image->texture_w;
	float tex_h = image->texture_h;
	
    for(n = 0; n < num_sprites; n++)
    {
        if(no_rects)
        {
            new_values[texcoord_i] = 0.0f;
            new_values[texcoord_i+1] = 0.0f;
            texcoord_i += floats_per_vertex;
            new_values[texcoord_i] = 1.0f;
            new_values[texcoord_i+1] = 0.0f;
            texcoord_i += floats_per_vertex;
            new_values[texcoord_i] = 1.0f;
            new_values[texcoord_i+1] = 1.0f;
            texcoord_i += floats_per_vertex;
            new_values[texcoord_i] = 0.0f;
            new_values[texcoord_i+1] = 1.0f;
            texcoord_i += floats_per_vertex;
        }
        else
        {
            if(!pass_texcoords)
            {
                float s1 = values[rect_n]/tex_w;
                float t1 = values[rect_n+1]/tex_h;
                float s3 = s1 + values[rect_n+2]/tex_w;
                float t3 = t1 + values[rect_n+3]/tex_h;
                rect_n += src_floats_per_sprite;
                
                new_values[texcoord_i] = s1;
                new_values[texcoord_i+1] = t1;
                texcoord_i += floats_per_vertex;
                new_values[texcoord_i] = s3;
                new_values[texcoord_i+1] = t1;
                texcoord_i += floats_per_vertex;
                new_values[texcoord_i] = s3;
                new_values[texcoord_i+1] = t3;
                texcoord_i += floats_per_vertex;
                new_values[texcoord_i] = s1;
                new_values[texcoord_i+1] = t3;
                texcoord_i += floats_per_vertex;
            
                if(!pass_vertices)
                {
                    w2 = 0.5f*(s3-s1)*image->w;
                    h2 = 0.5f*(t3-t1)*image->h;
                }
            }
            else
            {
                // 4 vertices all in a row
                float s1 = new_values[texcoord_i] = values[rect_n];
                float t1 = new_values[texcoord_i+1] = values[rect_n+1];
                texcoord_i += floats_per_vertex;
                new_values[texcoord_i] = values[rect_n+2];
                new_values[texcoord_i+1] = values[rect_n+3];
                texcoord_i += floats_per_vertex;
                float s3 = new_values[texcoord_i] = values[rect_n+4];
                float t3 = new_values[texcoord_i+1] = values[rect_n+5];
                texcoord_i += floats_per_vertex;
                new_values[texcoord_i] = values[rect_n+6];
                new_values[texcoord_i+1] = values[rect_n+7];
                texcoord_i += floats_per_vertex;
                rect_n += src_floats_per_sprite;
            
                if(!pass_vertices)
                {
                    w2 = 0.5f*(s3-s1)*image->w;
                    h2 = 0.5f*(t3-t1)*image->h;
                }
            }
        }
        
        if(no_positions)
        {
            new_values[vert_i] = 0.0f;
            new_values[vert_i+1] = 0.0f;
            vert_i += floats_per_vertex;
            new_values[vert_i] = 0.0f;
            new_values[vert_i+1] = 0.0f;
            vert_i += floats_per_vertex;
            new_values[vert_i] = 0.0f;
            new_values[vert_i+1] = 0.0f;
            vert_i += floats_per_vertex;
            new_values[vert_i] = 0.0f;
            new_values[vert_i+1] = 0.0f;
            vert_i += floats_per_vertex;
        }
        else
        {
            if(!pass_vertices)
            {
                // Expand vertices from the position and dimensions
                float x = values[pos_n];
                float y = values[pos_n+1];
                pos_n += src_floats_per_sprite;
                
                new_values[vert_i] = x - w2;
                new_values[vert_i+1] = y - h2;
                vert_i += floats_per_vertex;
                new_values[vert_i] = x + w2;
                new_values[vert_i+1] = y - h2;
                vert_i += floats_per_vertex;
                new_values[vert_i] = x + w2;
                new_values[vert_i+1] = y + h2;
                vert_i += floats_per_vertex;
                new_values[vert_i] = x - w2;
                new_values[vert_i+1] = y + h2;
                vert_i += floats_per_vertex;
            }
            else
            {
                // 4 vertices all in a row
                new_values[vert_i] = values[pos_n];
                new_values[vert_i+1] = values[pos_n+1];
                vert_i += floats_per_vertex;
                new_values[vert_i] = values[pos_n+2];
                new_values[vert_i+1] = values[pos_n+3];
                vert_i += floats_per_vertex;
                new_values[vert_i] = values[pos_n+4];
                new_values[vert_i+1] = values[pos_n+5];
                vert_i += floats_per_vertex;
                new_values[vert_i] = values[pos_n+6];
                new_values[vert_i+1] = values[pos_n+7];
                vert_i += floats_per_vertex;
                pos_n += src_floats_per_sprite;
            }
        }
        
        if(no_colors)
        {
                new_values[color_i] = 1.0f;
                new_values[color_i+1] = 1.0f;
                new_values[color_i+2] = 1.0f;
                new_values[color_i+3] = 1.0f;
                color_i += floats_per_vertex;
                new_values[color_i] = 1.0f;
                new_values[color_i+1] = 1.0f;
                new_values[color_i+2] = 1.0f;
                new_values[color_i+3] = 1.0f;
                color_i += floats_per_vertex;
                new_values[color_i] = 1.0f;
                new_values[color_i+1] = 1.0f;
                new_values[color_i+2] = 1.0f;
                new_values[color_i+3] = 1.0f;
                color_i += floats_per_vertex;
                new_values[color_i] = 1.0f;
                new_values[color_i+1] = 1.0f;
                new_values[color_i+2] = 1.0f;
                new_values[color_i+3] = 1.0f;
                color_i += floats_per_vertex;
        }
        else
        {
            if(!pass_colors)
            {
                float r = values[color_n]/255.0f;
                float g = values[color_n+1]/255.0f;
                float b = values[color_n+2]/255.0f;
                float a = values[color_n+3]/255.0f;
                color_n += src_floats_per_sprite;
                
                new_values[color_i] = r;
                new_values[color_i+1] = g;
                new_values[color_i+2] = b;
                new_values[color_i+3] = a;
                color_i += floats_per_vertex;
                new_values[color_i] = r;
                new_values[color_i+1] = g;
                new_values[color_i+2] = b;
                new_values[color_i+3] = a;
                color_i += floats_per_vertex;
                new_values[color_i] = r;
                new_values[color_i+1] = g;
                new_values[color_i+2] = b;
                new_values[color_i+3] = a;
                color_i += floats_per_vertex;
                new_values[color_i] = r;
                new_values[color_i+1] = g;
                new_values[color_i+2] = b;
                new_values[color_i+3] = a;
                color_i += floats_per_vertex;
            }
            else
            {
                // 4 vertices all in a row
                new_values[color_i] = values[color_n];
                new_values[color_i+1] = values[color_n+1];
                new_values[color_i+2] = values[color_n+2];
                new_values[color_i+3] = values[color_n+3];
                color_i += floats_per_vertex;
                new_values[color_i] = values[color_n+4];
                new_values[color_i+1] = values[color_n+5];
                new_values[color_i+2] = values[color_n+6];
                new_values[color_i+3] = values[color_n+7];
                color_i += floats_per_vertex;
                new_values[color_i] = values[color_n+8];
                new_values[color_i+1] = values[color_n+9];
                new_values[color_i+2] = values[color_n+10];
                new_values[color_i+3] = values[color_n+11];
                color_i += floats_per_vertex;
                new_values[color_i] = values[color_n+12];
                new_values[color_i+1] = values[color_n+13];
                new_values[color_i+2] = values[color_n+14];
                new_values[color_i+3] = values[color_n+15];
                color_i += floats_per_vertex;
                color_n += src_floats_per_sprite;
            }
        }
    }
    
	current_renderer->BlitBatch(current_renderer, image, target, num_sprites, new_values, flags | GPU_PASSTHROUGH_ALL);
	
	free(new_values);
}

void GPU_BlitBatchSeparate(GPU_Image* image, GPU_Target* target, unsigned int num_sprites, float* positions, float* src_rects, float* colors, GPU_BlitFlagEnum flags)
{
    if(!CHECK_RENDERER)
        RETURN_ERROR(GPU_ERROR_USER_ERROR, "NULL renderer");
    if(!CHECK_CONTEXT)
        RETURN_ERROR(GPU_ERROR_USER_ERROR, "NULL context");
    if(!CHECK_FUNCTION_POINTER(BlitBatch))
        RETURN_ERROR(GPU_ERROR_UNSUPPORTED_FUNCTION, NULL);
    
	if(image == NULL)
        RETURN_ERROR(GPU_ERROR_NULL_ARGUMENT, "image");
	if(target == NULL)
        RETURN_ERROR(GPU_ERROR_NULL_ARGUMENT, "target");
    
    if(num_sprites == 0)
        return;
    
    // No data to repack?  Skip it.
    if(positions == NULL && src_rects == NULL && colors == NULL)
    {
        current_renderer->BlitBatch(current_renderer, image, target, num_sprites, NULL, flags);
        return;
    }
	
	// Repack the given arrays into an interleaved array for more efficient access
	// Default values: Each sprite is defined by a position, a rect, and a color.
	
	Uint8 pass_vertices = (flags & GPU_PASSTHROUGH_VERTICES);
	Uint8 pass_texcoords = (flags & GPU_PASSTHROUGH_TEXCOORDS);
	Uint8 pass_colors = (flags & GPU_PASSTHROUGH_COLORS);
	
	int size = num_sprites*(8 + 8 + 16);  // 4 vertices of x, y...  s, t...  r, g, b, a
	float* values = (float*)malloc(sizeof(float)*size);
	
	int n;  // The sprite number iteration variable.
	// Source indices
	int pos_n = 0;
	int rect_n = 0;
	int color_n = 0;
	// Dest indices
	int vert_i = 0;
	int texcoord_i = 2;
	int color_i = 4;
	// Dest float stride
	int floats_per_vertex = 8;
	
	float w2 = 0.5f*image->w;  // texcoord helpers for position expansion
	float h2 = 0.5f*image->h;
	
	float tex_w = image->texture_w;
	float tex_h = image->texture_h;
    
	for(n = 0; n < num_sprites; n++)
    {
        // Unpack the arrays
        
        if(src_rects == NULL)
        {
            values[texcoord_i] = 0.0f;
            values[texcoord_i+1] = 0.0f;
            texcoord_i += floats_per_vertex;
            values[texcoord_i] = 1.0f;
            values[texcoord_i+1] = 0.0f;
            texcoord_i += floats_per_vertex;
            values[texcoord_i] = 1.0f;
            values[texcoord_i+1] = 1.0f;
            texcoord_i += floats_per_vertex;
            values[texcoord_i] = 0.0f;
            values[texcoord_i+1] = 1.0f;
            texcoord_i += floats_per_vertex;
        }
        else
        {
            if(!pass_texcoords)
            {
                float s1 = src_rects[rect_n++]/tex_w;
                float t1 = src_rects[rect_n++]/tex_h;
                float s3 = s1 + src_rects[rect_n++]/tex_w;
                float t3 = t1 + src_rects[rect_n++]/tex_h;
                
                values[texcoord_i] = s1;
                values[texcoord_i+1] = t1;
                texcoord_i += floats_per_vertex;
                values[texcoord_i] = s3;
                values[texcoord_i+1] = t1;
                texcoord_i += floats_per_vertex;
                values[texcoord_i] = s3;
                values[texcoord_i+1] = t3;
                texcoord_i += floats_per_vertex;
                values[texcoord_i] = s1;
                values[texcoord_i+1] = t3;
                texcoord_i += floats_per_vertex;
            
                if(!pass_vertices)
                {
                    w2 = 0.5f*(s3-s1)*image->w;
                    h2 = 0.5f*(t3-t1)*image->h;
                }
            }
            else
            {
                // 4 vertices all in a row
                float s1 = values[texcoord_i] = src_rects[rect_n++];
                float t1 = values[texcoord_i+1] = src_rects[rect_n++];
                texcoord_i += floats_per_vertex;
                values[texcoord_i] = src_rects[rect_n++];
                values[texcoord_i+1] = src_rects[rect_n++];
                texcoord_i += floats_per_vertex;
                float s3 = values[texcoord_i] = src_rects[rect_n++];
                float t3 = values[texcoord_i+1] = src_rects[rect_n++];
                texcoord_i += floats_per_vertex;
                values[texcoord_i] = src_rects[rect_n++];
                values[texcoord_i+1] = src_rects[rect_n++];
                texcoord_i += floats_per_vertex;
            
                if(!pass_vertices)
                {
                    w2 = 0.5f*(s3-s1)*image->w;
                    h2 = 0.5f*(t3-t1)*image->h;
                }
            }
        }
        
        if(positions == NULL)
        {
            values[vert_i] = 0.0f;
            values[vert_i+1] = 0.0f;
            vert_i += floats_per_vertex;
            values[vert_i] = 0.0f;
            values[vert_i+1] = 0.0f;
            vert_i += floats_per_vertex;
            values[vert_i] = 0.0f;
            values[vert_i+1] = 0.0f;
            vert_i += floats_per_vertex;
            values[vert_i] = 0.0f;
            values[vert_i+1] = 0.0f;
            vert_i += floats_per_vertex;
        }
        else
        {
            if(!pass_vertices)
            {
                // Expand vertices from the position and dimensions
                float x = positions[pos_n++];
                float y = positions[pos_n++];
                values[vert_i] = x - w2;
                values[vert_i+1] = y - h2;
                vert_i += floats_per_vertex;
                values[vert_i] = x + w2;
                values[vert_i+1] = y - h2;
                vert_i += floats_per_vertex;
                values[vert_i] = x + w2;
                values[vert_i+1] = y + h2;
                vert_i += floats_per_vertex;
                values[vert_i] = x - w2;
                values[vert_i+1] = y + h2;
                vert_i += floats_per_vertex;
            }
            else
            {
                // 4 vertices all in a row
                values[vert_i] = positions[pos_n++];
                values[vert_i+1] = positions[pos_n++];
                vert_i += floats_per_vertex;
                values[vert_i] = positions[pos_n++];
                values[vert_i+1] = positions[pos_n++];
                vert_i += floats_per_vertex;
                values[vert_i] = positions[pos_n++];
                values[vert_i+1] = positions[pos_n++];
                vert_i += floats_per_vertex;
                values[vert_i] = positions[pos_n++];
                values[vert_i+1] = positions[pos_n++];
                vert_i += floats_per_vertex;
            }
        }
        
        if(colors == NULL)
        {
                values[color_i] = 1.0f;
                values[color_i+1] = 1.0f;
                values[color_i+2] = 1.0f;
                values[color_i+3] = 1.0f;
                color_i += floats_per_vertex;
                values[color_i] = 1.0f;
                values[color_i+1] = 1.0f;
                values[color_i+2] = 1.0f;
                values[color_i+3] = 1.0f;
                color_i += floats_per_vertex;
                values[color_i] = 1.0f;
                values[color_i+1] = 1.0f;
                values[color_i+2] = 1.0f;
                values[color_i+3] = 1.0f;
                color_i += floats_per_vertex;
                values[color_i] = 1.0f;
                values[color_i+1] = 1.0f;
                values[color_i+2] = 1.0f;
                values[color_i+3] = 1.0f;
                color_i += floats_per_vertex;
        }
        else
        {
            if(!pass_colors)
            {
                float r = colors[color_n++]/255.0f;
                float g = colors[color_n++]/255.0f;
                float b = colors[color_n++]/255.0f;
                float a = colors[color_n++]/255.0f;
                
                values[color_i] = r;
                values[color_i+1] = g;
                values[color_i+2] = b;
                values[color_i+3] = a;
                color_i += floats_per_vertex;
                values[color_i] = r;
                values[color_i+1] = g;
                values[color_i+2] = b;
                values[color_i+3] = a;
                color_i += floats_per_vertex;
                values[color_i] = r;
                values[color_i+1] = g;
                values[color_i+2] = b;
                values[color_i+3] = a;
                color_i += floats_per_vertex;
                values[color_i] = r;
                values[color_i+1] = g;
                values[color_i+2] = b;
                values[color_i+3] = a;
                color_i += floats_per_vertex;
            }
            else
            {
                // 4 vertices all in a row
                values[color_i] = colors[color_n++];
                values[color_i+1] = colors[color_n++];
                values[color_i+2] = colors[color_n++];
                values[color_i+3] = colors[color_n++];
                color_i += floats_per_vertex;
                values[color_i] = colors[color_n++];
                values[color_i+1] = colors[color_n++];
                values[color_i+2] = colors[color_n++];
                values[color_i+3] = colors[color_n++];
                color_i += floats_per_vertex;
                values[color_i] = colors[color_n++];
                values[color_i+1] = colors[color_n++];
                values[color_i+2] = colors[color_n++];
                values[color_i+3] = colors[color_n++];
                color_i += floats_per_vertex;
                values[color_i] = colors[color_n++];
                values[color_i+1] = colors[color_n++];
                values[color_i+2] = colors[color_n++];
                values[color_i+3] = colors[color_n++];
                color_i += floats_per_vertex;
            }
        }
    }
	
	current_renderer->BlitBatch(current_renderer, image, target, num_sprites, values, flags | GPU_PASSTHROUGH_ALL);
	free(values);
}

void GPU_TriangleBatch(GPU_Image* image, GPU_Target* target, int num_vertices, float* values, int num_indices, unsigned short* indices, GPU_BlitFlagEnum flags)
{
    if(!CHECK_RENDERER)
        RETURN_ERROR(GPU_ERROR_USER_ERROR, "NULL renderer");
    if(!CHECK_CONTEXT)
        RETURN_ERROR(GPU_ERROR_USER_ERROR, "NULL context");
    if(!CHECK_FUNCTION_POINTER(TriangleBatch))
        RETURN_ERROR(GPU_ERROR_UNSUPPORTED_FUNCTION, NULL);
    
	if(image == NULL)
        RETURN_ERROR(GPU_ERROR_NULL_ARGUMENT, "image");
	if(target == NULL)
        RETURN_ERROR(GPU_ERROR_NULL_ARGUMENT, "target");
    
    if(num_vertices == 0)
        return;
    
    // Is it already in the right format?
    if((flags & GPU_PASSTHROUGH_ALL) == GPU_PASSTHROUGH_ALL || values == NULL)
    {
        current_renderer->TriangleBatch(current_renderer, image, target, num_vertices, values, num_indices, indices, flags);
        return;
    }
	
	// Conversion time...
	
	// Convert texcoords and colors for the renderer to use.
	// Condensed: Each vertex has 2 pos, 2 texcoords, 4 color components
	
	// Default values: Each vertex is defined by a position, texcoords, and a color.
	int src_position_floats_per_vertex = 2;
	int src_texcoord_floats_per_vertex = 2;
	int src_color_floats_per_vertex = 4;
	
	Uint8 no_positions = (flags & GPU_USE_DEFAULT_POSITIONS);
	Uint8 no_texcoords = (flags & GPU_USE_DEFAULT_SRC_RECTS);
	Uint8 no_colors = (flags & GPU_USE_DEFAULT_COLORS);
	Uint8 pass_texcoords = (flags & GPU_PASSTHROUGH_TEXCOORDS);
	Uint8 pass_colors = (flags & GPU_PASSTHROUGH_COLORS);
	
	// Vertex position passthrough is ignored (we're not positioning triangles, we're positioning vertices already)
	src_position_floats_per_vertex = 2; // x, y
	if(pass_texcoords)
        src_texcoord_floats_per_vertex = 2; // s, t
	if(pass_colors)
        src_color_floats_per_vertex = 4; // r, g, b, a
	if(no_positions)
        src_position_floats_per_vertex = 0;
	if(no_texcoords)
        src_texcoord_floats_per_vertex = 0;
	if(no_colors)
        src_color_floats_per_vertex = 0;
    
	int src_floats_per_vertex = src_position_floats_per_vertex + src_texcoord_floats_per_vertex + src_color_floats_per_vertex;
	
	int size = num_vertices*(2 + 2 + 4);
	float* new_values = (float*)malloc(sizeof(float)*size);
    
	int n; // Vertex number iteration variable
	// Source indices
	int pos_n = 0;
	int texcoord_n = src_position_floats_per_vertex;
	int color_n = src_position_floats_per_vertex + src_texcoord_floats_per_vertex;
	// Dest indices
	int vert_i = 0;
	
	float tex_w = image->texture_w;
	float tex_h = image->texture_h;
	
    for(n = 0; n < num_vertices; n++)
    {
        // 2 floats from position
        if(no_positions)
        {
            new_values[vert_i++] = 0.0f;
            new_values[vert_i++] = 0.0f;
        }
        else
        {
            new_values[vert_i++] = values[pos_n];
            new_values[vert_i++] = values[pos_n+1];
            pos_n += src_floats_per_vertex;
        }
        
        // 2 floats from texcoords
        if(no_texcoords)
        {
            new_values[vert_i++] = 0.0f;
            new_values[vert_i++] = 0.0f;
        }
        else
        {
            if(!pass_texcoords)
            {
                new_values[vert_i++] = values[texcoord_n]/tex_w;
                new_values[vert_i++] = values[texcoord_n+1]/tex_h;
                texcoord_n += src_floats_per_vertex;
            }
            else
            {
                new_values[vert_i++] = values[texcoord_n];
                new_values[vert_i++] = values[texcoord_n+1];
            }
        }
        
        if(no_colors)
        {
                new_values[vert_i++] = 1.0f;
                new_values[vert_i++] = 1.0f;
                new_values[vert_i++] = 1.0f;
                new_values[vert_i++] = 1.0f;
        }
        else
        {
            if(!pass_colors)
            {
                new_values[vert_i++] = values[color_n]/255.0f;
                new_values[vert_i++] = values[color_n+1]/255.0f;
                new_values[vert_i++] = values[color_n+2]/255.0f;
                new_values[vert_i++] = values[color_n+3]/255.0f;
                color_n += src_floats_per_vertex;
            }
            else
            {
                new_values[vert_i++] = values[color_n];
                new_values[vert_i++] = values[color_n+1];
                new_values[vert_i++] = values[color_n+2];
                new_values[vert_i++] = values[color_n+3];
                color_n += src_floats_per_vertex;
            }
        }
    }
    
	current_renderer->TriangleBatch(current_renderer, image, target, num_vertices, new_values, num_indices, indices, flags | GPU_PASSTHROUGH_ALL);
	
	free(new_values);
}




void GPU_GenerateMipmaps(GPU_Image* image)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->GenerateMipmaps == NULL)
		return;
	
	current_renderer->GenerateMipmaps(current_renderer, image);
}




GPU_Rect GPU_SetClipRect(GPU_Target* target, GPU_Rect rect)
{
	if(target == NULL || current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->SetClip == NULL)
	{
		GPU_Rect r = {0,0,0,0};
		return r;
	}
	
	return current_renderer->SetClip(current_renderer, target, rect.x, rect.y, rect.w, rect.h);
}

GPU_Rect GPU_SetClip(GPU_Target* target, Sint16 x, Sint16 y, Uint16 w, Uint16 h)
{
	if(target == NULL || current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->SetClip == NULL)
	{
		GPU_Rect r = {0,0,0,0};
		return r;
	}
	
	return current_renderer->SetClip(current_renderer, target, x, y, w, h);
}

void GPU_UnsetClip(GPU_Target* target)
{
	if(target == NULL || current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->UnsetClip == NULL)
        return;
	
	current_renderer->UnsetClip(current_renderer, target);
}




void GPU_SetColor(GPU_Image* image, SDL_Color* color)
{
	if(image == NULL)
		return;
	
	if(color == NULL)
    {
        SDL_Color c = {255, 255, 255, 255};
        image->color = c;
    }
    else
        image->color = *color;
}

void GPU_SetRGB(GPU_Image* image, Uint8 r, Uint8 g, Uint8 b)
{
	if(image == NULL)
		return;
	
	SDL_Color c = {r, g, b, 255};
	image->color = c;
}

void GPU_SetRGBA(GPU_Image* image, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	if(image == NULL)
		return;
	
	SDL_Color c = {r, g, b, a};
	image->color = c;
}

void GPU_SetTargetColor(GPU_Target* target, SDL_Color* color)
{
	if(target == NULL)
		return;
	
	if(color == NULL)
        target->use_color = 0;
    else
    {
        target->use_color = 1;
        target->color = *color;
    }
}

void GPU_SetTargetRGB(GPU_Target* target, Uint8 r, Uint8 g, Uint8 b)
{
	if(target == NULL)
		return;
	
	if(r == 255 && g == 255 && b == 255)
        target->use_color = 0;
    else
    {
        target->use_color = 1;
        SDL_Color c = {r, g, b, 255};
        target->color = c;
    }
}

void GPU_SetTargetRGBA(GPU_Target* target, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	if(target == NULL)
		return;
	
	if(r == 255 && g == 255 && b == 255 && a == 255)
        target->use_color = 0;
    else
    {
        target->use_color = 1;
        SDL_Color c = {r, g, b, a};
        target->color = c;
    }
}

Uint8 GPU_GetBlending(GPU_Image* image)
{
	if(image == NULL)
		return 0;
	
	return image->use_blending;
}


void GPU_SetBlending(GPU_Image* image, Uint8 enable)
{
	if(image == NULL)
		return;
	
	image->use_blending = enable;
}

void GPU_SetShapeBlending(Uint8 enable)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL)
		return;
	
	current_renderer->current_context_target->context->shapes_use_blending = enable;
}

void GPU_SetBlendMode(GPU_Image* image, GPU_BlendEnum mode)
{
	if(image == NULL)
		return;
	
	image->blend_mode = mode;
}

void GPU_SetShapeBlendMode(GPU_BlendEnum mode)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL)
		return;
	
	current_renderer->current_context_target->context->shapes_blend_mode = mode;
}

void GPU_SetImageFilter(GPU_Image* image, GPU_FilterEnum filter)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->SetImageFilter == NULL)
		return;
	if(image == NULL)
		return;
	
	image->filter_mode = filter;
	current_renderer->SetImageFilter(current_renderer, image, filter);
}


SDL_Color GPU_GetPixel(GPU_Target* target, Sint16 x, Sint16 y)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->GetPixel == NULL)
	{
		SDL_Color c = {0,0,0,0};
		return c;
	}
	
	return current_renderer->GetPixel(current_renderer, target, x, y);
}







void GPU_Clear(GPU_Target* target)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->Clear == NULL)
		return;
	
	current_renderer->Clear(current_renderer, target);
}

void GPU_ClearRGBA(GPU_Target* target, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->ClearRGBA == NULL)
		return;
	
	current_renderer->ClearRGBA(current_renderer, target, r, g, b, a);
}

void GPU_FlushBlitBuffer(void)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->FlushBlitBuffer == NULL)
		return;
	
	current_renderer->FlushBlitBuffer(current_renderer);
}

void GPU_Flip(GPU_Target* target)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->Flip == NULL)
		return;
	
	current_renderer->Flip(current_renderer, target);
}





// Shader API


Uint32 GPU_CompileShader_RW(int shader_type, SDL_RWops* shader_source)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->CompileShader_RW == NULL)
		return 0;
	
	return current_renderer->CompileShader_RW(current_renderer, shader_type, shader_source);
}

Uint32 GPU_LoadShader(int shader_type, const char* filename)
{
    if(filename == NULL)
    {
        GPU_PushErrorCode(__func__, GPU_ERROR_NULL_ARGUMENT, "filename");
        return 0;
    }
    SDL_RWops* rwops = SDL_RWFromFile(filename, "r");
    if(rwops == NULL)
    {
        GPU_PushErrorCode(__func__, GPU_ERROR_FILE_NOT_FOUND, filename);
        return 0;
    }
    Uint32 result = GPU_CompileShader_RW(shader_type, rwops);
    SDL_RWclose(rwops);
    return result;
}

Uint32 GPU_CompileShader(int shader_type, const char* shader_source)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->CompileShader == NULL)
		return 0;
	
	return current_renderer->CompileShader(current_renderer, shader_type, shader_source);
}

Uint32 GPU_LinkShaderProgram(Uint32 program_object)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->LinkShaderProgram == NULL)
		return 0;
	
	return current_renderer->LinkShaderProgram(current_renderer, program_object);
}

Uint32 GPU_LinkShaders(Uint32 shader_object1, Uint32 shader_object2)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->LinkShaders == NULL)
		return 0;
	
	return current_renderer->LinkShaders(current_renderer, shader_object1, shader_object2);
}

void GPU_FreeShader(Uint32 shader_object)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->FreeShader == NULL)
		return;
	
	current_renderer->FreeShader(current_renderer, shader_object);
}

void GPU_FreeShaderProgram(Uint32 program_object)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->FreeShaderProgram == NULL)
		return;
	
	current_renderer->FreeShaderProgram(current_renderer, program_object);
}

void GPU_AttachShader(Uint32 program_object, Uint32 shader_object)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->AttachShader == NULL)
		return;
	
	current_renderer->AttachShader(current_renderer, program_object, shader_object);
}

void GPU_DetachShader(Uint32 program_object, Uint32 shader_object)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->DetachShader == NULL)
		return;
	
	current_renderer->DetachShader(current_renderer, program_object, shader_object);
}

Uint8 GPU_IsDefaultShaderProgram(Uint32 program_object)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->IsDefaultShaderProgram == NULL)
		return 0;
		
	return current_renderer->IsDefaultShaderProgram(current_renderer, program_object);
}

void GPU_ActivateShaderProgram(Uint32 program_object, GPU_ShaderBlock* block)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->ActivateShaderProgram == NULL)
		return;
	
	current_renderer->ActivateShaderProgram(current_renderer, program_object, block);
}

void GPU_DeactivateShaderProgram(void)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->DeactivateShaderProgram == NULL)
		return;
	
	current_renderer->DeactivateShaderProgram(current_renderer);
}

const char* GPU_GetShaderMessage(void)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->GetShaderMessage == NULL)
		return NULL;
	
	return current_renderer->GetShaderMessage(current_renderer);
}

int GPU_GetAttributeLocation(Uint32 program_object, const char* attrib_name)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->GetAttributeLocation == NULL)
		return 0;
	
	return current_renderer->GetAttributeLocation(current_renderer, program_object, attrib_name);
}

GPU_AttributeFormat GPU_MakeAttributeFormat(int num_elems_per_vertex, GPU_TypeEnum type, Uint8 normalize, int stride_bytes, int offset_bytes)
{
    GPU_AttributeFormat f = {0, num_elems_per_vertex, type, normalize, stride_bytes, offset_bytes};
    return f;
}

GPU_Attribute GPU_MakeAttribute(int location, void* values, GPU_AttributeFormat format)
{
    GPU_Attribute a = {location, values, format};
    return a;
}

int GPU_GetUniformLocation(Uint32 program_object, const char* uniform_name)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->GetUniformLocation == NULL)
		return 0;
	
	return current_renderer->GetUniformLocation(current_renderer, program_object, uniform_name);
}

GPU_ShaderBlock GPU_LoadShaderBlock(Uint32 program_object, const char* position_name, const char* texcoord_name, const char* color_name, const char* modelViewMatrix_name)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->LoadShaderBlock == NULL)
    {
        GPU_ShaderBlock b;
        b.position_loc = -1;
        b.texcoord_loc = -1;
        b.color_loc = -1;
        b.modelViewProjection_loc = -1;
		return b;
    }
	
	return current_renderer->LoadShaderBlock(current_renderer, program_object, position_name, texcoord_name, color_name, modelViewMatrix_name);
}

void GPU_SetShaderBlock(GPU_ShaderBlock block)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->SetShaderBlock == NULL)
		return;
	
	current_renderer->SetShaderBlock(current_renderer, block);
}

void GPU_SetShaderImage(GPU_Image* image, int location, int image_unit)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->SetShaderImage == NULL)
		return;
	
	current_renderer->SetShaderImage(current_renderer, image, location, image_unit);
}

void GPU_GetUniformiv(Uint32 program_object, int location, int* values)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->GetUniformiv == NULL)
		return;
	
	current_renderer->GetUniformiv(current_renderer, program_object, location, values);
}

void GPU_SetUniformi(int location, int value)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->SetUniformi == NULL)
		return;
	
	current_renderer->SetUniformi(current_renderer, location, value);
}

void GPU_SetUniformiv(int location, int num_elements_per_value, int num_values, int* values)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->SetUniformiv == NULL)
		return;
	
	current_renderer->SetUniformiv(current_renderer, location, num_elements_per_value, num_values, values);
}


void GPU_GetUniformuiv(Uint32 program_object, int location, unsigned int* values)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->GetUniformuiv == NULL)
		return;
	
	current_renderer->GetUniformuiv(current_renderer, program_object, location, values);
}

void GPU_SetUniformui(int location, unsigned int value)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->SetUniformui == NULL)
		return;
	
	current_renderer->SetUniformui(current_renderer, location, value);
}

void GPU_SetUniformuiv(int location, int num_elements_per_value, int num_values, unsigned int* values)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->SetUniformuiv == NULL)
		return;
	
	current_renderer->SetUniformuiv(current_renderer, location, num_elements_per_value, num_values, values);
}


void GPU_GetUniformfv(Uint32 program_object, int location, float* values)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->GetUniformfv == NULL)
		return;
	
	current_renderer->GetUniformfv(current_renderer, program_object, location, values);
}

void GPU_SetUniformf(int location, float value)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->SetUniformf == NULL)
		return;
	
	current_renderer->SetUniformf(current_renderer, location, value);
}

void GPU_SetUniformfv(int location, int num_elements_per_value, int num_values, float* values)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->SetUniformfv == NULL)
		return;
	
	current_renderer->SetUniformfv(current_renderer, location, num_elements_per_value, num_values, values);
}

// Same as GPU_GetUniformfv()
void GPU_GetUniformMatrixfv(Uint32 program_object, int location, float* values)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->GetUniformfv == NULL)
		return;
	
	current_renderer->GetUniformfv(current_renderer, program_object, location, values);
}

void GPU_SetUniformMatrixfv(int location, int num_matrices, int num_rows, int num_columns, Uint8 transpose, float* values)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->SetUniformMatrixfv == NULL)
		return;
	
	current_renderer->SetUniformMatrixfv(current_renderer, location, num_matrices, num_rows, num_columns, transpose, values);
}


void GPU_SetAttributef(int location, float value)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->SetAttributef == NULL)
		return;
	
	current_renderer->SetAttributef(current_renderer, location, value);
}

void GPU_SetAttributei(int location, int value)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->SetAttributei == NULL)
		return;
	
	current_renderer->SetAttributei(current_renderer, location, value);
}

void GPU_SetAttributeui(int location, unsigned int value)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->SetAttributeui == NULL)
		return;
	
	current_renderer->SetAttributeui(current_renderer, location, value);
}

void GPU_SetAttributefv(int location, int num_elements, float* value)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->SetAttributefv == NULL)
		return;
	
	current_renderer->SetAttributefv(current_renderer, location, num_elements, value);
}

void GPU_SetAttributeiv(int location, int num_elements, int* value)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->SetAttributeiv == NULL)
		return;
	
	current_renderer->SetAttributeiv(current_renderer, location, num_elements, value);
}

void GPU_SetAttributeuiv(int location, int num_elements, unsigned int* value)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->SetAttributeuiv == NULL)
		return;
	
	current_renderer->SetAttributeuiv(current_renderer, location, num_elements, value);
}

void GPU_SetAttributeSource(int num_values, GPU_Attribute source)
{
	if(current_renderer == NULL || current_renderer->current_context_target == NULL || current_renderer->SetAttributeSource == NULL)
		return;
	
	current_renderer->SetAttributeSource(current_renderer, num_values, source);
}

