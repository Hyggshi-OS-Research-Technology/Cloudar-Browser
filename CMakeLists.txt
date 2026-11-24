cmake_minimum_required(VERSION 3.16)
project(cloudar-browser VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    WebEngineWidgets
    WebEngineCore
    Network
    WebChannel
)

# CrxInstaller uses zlib directly for raw-DEFLATE. On Windows there is no
# system zlib, so the Windows build job installs one via vcpkg; everywhere else
# CMake's FindZLIB picks up the distro package.
find_package(ZLIB REQUIRED)

set(SOURCES
    src/main.cpp
    src/AdBlock.cpp
    src/AISidebar.cpp
    src/BookmarkBar.cpp
    src/BookmarkManager.cpp
    src/BrowserResources.cpp
    src/BrowserSidebar.cpp
    src/BrowserTabWidget.cpp
    src/ChromeExtensionAPI.cpp
    src/ClassicTheme.cpp
    src/CrxInstaller.cpp
    src/DownloadManager.cpp
    src/DevToolsWindow.cpp
    src/ExtensionManager.cpp
    src/ExtensionsDialog.cpp
    src/FaviconStore.cpp
    src/HistoryManager.cpp
    src/InternalBridge.cpp
    src/NewTabBridge.cpp
    src/PermissionStore.cpp
    src/PageContextMenu.cpp
    src/IdleEasterEgg.cpp
    src/Language.cpp
    src/LightSchemeHandler.cpp
    src/MainWindow.cpp
    src/MemorySaver.cpp
    src/PerformanceProfile.cpp
    src/ReaderMode.cpp
    src/SessionManager.cpp
    src/SettingsBridge.cpp
    src/SettingsDialog.cpp
    src/SpinnerWidget.cpp
    src/SplitView.cpp
    src/TabPreviewPopup.cpp
    src/ThemeManager.cpp
    src/TitleBar.cpp
    src/TorrentDownloader.cpp
    src/WebExtensionAPIBridge.cpp
    src/WebExtensionManager.cpp
    src/WebExtensionManifest.cpp
    src/YouTubeDownloader.cpp
)

set(HEADERS
    src/AdBlock.h
    src/AISidebar.h
    src/BookmarkBar.h
    src/BookmarkManager.h
    src/BrowserResources.h
    src/BrowserSidebar.h
    src/BrowserTabWidget.h
    src/ChromeExtensionAPI.h
    src/ClassicTheme.h
    src/CrxInstaller.h
    src/DownloadManager.h
    src/DevToolsWindow.h
    src/ExtensionManager.h
    src/ExtensionsDialog.h
    src/FaviconStore.h
    src/HistoryManager.h
    src/InternalBridge.h
    src/NewTabBridge.h
    src/PermissionStore.h
    src/PageContextMenu.h
    src/IdleEasterEgg.h
    src/Language.h
    src/LightSchemeHandler.h
    src/MainWindow.h
    src/MemorySaver.h
    src/PerformanceProfile.h
    src/NativeBridge.h
    src/ReaderMode.h
    src/SessionManager.h
    src/SettingsBridge.h
    src/SettingsDialog.h
    src/SpinnerWidget.h
    src/SplitView.h
    src/TabPreviewPopup.h
    src/ThemeManager.h
    src/TitleBar.h
    src/TorrentDownloader.h
    src/WebExtensionAPIBridge.h
    src/WebExtensionManager.h
    src/WebExtensionManifest.h
    src/YouTubeDownloader.h
)

add_executable(cloudar-browser
    ${SOURCES}
    ${HEADERS}
    resources/resources.qrc
)

# The version is defined in exactly one place: the project() line above. Both
# --version and QApplication::applicationVersion() read it from here.
target_compile_definitions(cloudar-browser PRIVATE
    CLOUDAR_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    CLOUDAR_VERSION="${PROJECT_VERSION}"
)

target_include_directories(cloudar-browser PRIVATE
    src
)

target_link_libraries(cloudar-browser PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::WebEngineWidgets
    Qt6::WebEngineCore
    Qt6::Network
    Qt6::WebChannel
    ZLIB::ZLIB
)

install(TARGETS cloudar-browser RUNTIME DESTINATION bin)
install(FILES resources/cloudar-browser.desktop DESTINATION share/applications)
install(FILES resources/icon.png DESTINATION share/icons/hicolor/256x256/apps RENAME cloudar-browser.png)
