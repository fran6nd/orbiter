project(sdl3-extern)

# SDL3 is not used on Windows in the current build.
# It is fetched here so it is available when cross-platform targets are enabled.
if(NOT WIN32)
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
