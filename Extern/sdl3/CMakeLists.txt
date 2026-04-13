project(sdl3-extern)

# SDL3 is fetched when ORBITER_USE_SDL3 is enabled.
# On Windows the default is OFF; the D3D9/Win32 path is used instead.
# Enable this option when targeting a platform without DirectX or when
# building the future cross-platform Vulkan client.
if(ORBITER_USE_SDL3)
    Include(FetchContent)

    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        release-3.2.16
        UPDATE_DISCONNECTED 1
    )

    set(SDL_SHARED ON  CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(SDL_TEST   OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(SDL3)
endif()
