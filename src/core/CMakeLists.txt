add_library(Core
    AppleExclusive.c
    AppleExclusive.h
    Betacraft.c
    Betacraft.h
    Instance.c
    Instance.h
    Game.c
    Game.h
    Network.c
    Network.h
    FileSystem.c
    FileSystem.h
    Version.c
    Version.h
    AssetIndex.c
    AssetIndex.h
    VersionList.c
    VersionList.h
    Settings.c
    Settings.h
    StringUtils.c
    StringUtils.h
    JsonExtension.c
    JsonExtension.h
    AuthMicrosoft.c
    AuthMicrosoft.h
    AuthMojang.c
    AuthMojang.h
    Account.c
    Account.h
    JavaInstallations.c
    JavaInstallations.h
    Update.c
    Update.h
    Logger.c
    Logger.h
    Discord.c
    Discord.h
    Mod.c
    Mod.h
    ProcessHandler.c
    ProcessHandler.h
    WindowsProcessHandler.c
    WindowsProcessHandler.h
    Constants.h
    ../../lib/discord_game_sdk/discord_game_sdk.h
    ../../lib/libtar/libtar.h
)

find_package(json-c CONFIG REQUIRED)
find_package(libzip CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)

if (WIN32)
    target_link_libraries(Core PUBLIC ${CMAKE_SOURCE_DIR}/lib/discord_game_sdk/x86_64/discord_game_sdk.dll.lib)
elseif (APPLE)
    set(CMAKE_APPLE_SILICON_PROCESSOR "x86_64")
    #set(CMAKE_APPLE_SILICON_PROCESSOR "arm64")
    if(CMAKE_APPLE_SILICON_PROCESSOR STREQUAL "arm64")
        target_link_libraries(Core PUBLIC ${CMAKE_SOURCE_DIR}/lib/discord_game_sdk/aarch64/discord_game_sdk.bundle)
        target_link_libraries(Core PUBLIC ${CMAKE_SOURCE_DIR}/lib/libtar/libtar-arm64.dylib)
    elseif(CMAKE_APPLE_SILICON_PROCESSOR STREQUAL "x86_64")
        target_link_libraries(Core PUBLIC ${CMAKE_SOURCE_DIR}/lib/discord_game_sdk/x86_64/discord_game_sdk.dylib)
        target_link_libraries(Core PUBLIC ${CMAKE_SOURCE_DIR}/lib/libtar/libtar-x86_64.dylib)
    endif()
elseif (UNIX AND NOT APPLE) # Linux
    target_link_libraries(Core PUBLIC tar)

    add_library(discordgamesdk SHARED IMPORTED)
    set_target_properties(discordgamesdk PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/discord_game_sdk/x86_64/discord_game_sdk.so)

    target_link_libraries(Core PUBLIC discordgamesdk)
endif()

target_link_libraries(Core PUBLIC
    CURL::libcurl
    json-c::json-c
    libzip::zip
)
