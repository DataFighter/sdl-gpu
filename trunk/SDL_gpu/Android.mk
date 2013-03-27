LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := SDL2_gpu

SDL_GPU_DIR := ./
RENDERER_DIR := $(SDL_GPU_DIR)/OpenGL_common
STB_IMAGE_DIR := $(SDL_GPU_DIR)/externals/stb_image

LOCAL_CFLAGS := -I$(LOCAL_PATH)/../SDL/include -I$(LOCAL_PATH)/$(SDL_GPU_DIR) -I$(LOCAL_PATH)/$(RENDERER_DIR) -I$(LOCAL_PATH)/$(STB_IMAGE_DIR)

LOCAL_SRC_FILES := $(SDL_GPU_DIR)/SDL_gpu.c \
				   $(SDL_GPU_DIR)/SDL_gpu_Renderer.c \
				   $(SDL_GPU_DIR)/SDL_gpuShapes.c \
				   $(RENDERER_DIR)/SDL_gpu_OpenGL.c \
				   $(RENDERER_DIR)/SDL_gpuShapes_OpenGL.c \
				   $(STB_IMAGE_DIR)/stb_image.c \
				   $(STB_IMAGE_DIR)/stb_image_write.c


LOCAL_CFLAGS += -DSDL_GPU_USE_OPENGLES -DSTBI_FAILURE_USERMSG -O3
LOCAL_LDLIBS += -llog -lGLESv1_CM

LOCAL_SHARED_LIBRARIES := SDL2

include $(BUILD_SHARED_LIBRARY)
