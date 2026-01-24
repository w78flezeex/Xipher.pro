cmake_minimum_required(VERSION 3.21)

project(xipher_desktop VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Network WebSockets Widgets Test)

qt_standard_project_setup()

qt_add_executable(xipher_desktop WIN32
    src/main.cpp
    src/app/AppConfig.cpp
    src/app/FileSecureStorage.cpp
    src/app/SecureStorage.cpp
    src/app/SessionManager.cpp
    src/app/SettingsStore.cpp
    src/app/Session.cpp
    src/app/SessionStore.cpp
    src/logging/Log.cpp
    src/net/ApiClient.cpp
    src/net/CachedNetworkAccessManagerFactory.cpp
    src/net/CookieJar.cpp
    src/net/UploadManager.cpp
    src/net/WsClient.cpp
    src/models/Chat.cpp
    src/models/Message.cpp
    src/models/ApiDtos.cpp
    src/store/ChatListModel.cpp
    src/store/MessageListModel.cpp
    src/store/UploadListModel.cpp
    src/store/ChatStore.cpp
    src/services/FileDialogService.cpp
    src/vm/AuthViewModel.cpp
    src/vm/ShellViewModel.cpp
    src/vm/SettingsViewModel.cpp
)

target_sources(xipher_desktop PRIVATE
    $<$<PLATFORM_ID:Windows>:src/app/WinDpapiStorage.cpp>
    $<$<PLATFORM_ID:Darwin>:src/app/MacKeychainStorage.cpp>
    $<$<PLATFORM_ID:Linux>:src/app/LinuxSecretStorage.cpp>
)

qt_add_qml_module(xipher_desktop
    URI Xipher.Desktop
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/views/AuthView.qml
        qml/views/CallStub.qml
        qml/views/ShellView.qml
        qml/views/SettingsView.qml
        qml/views/NewChatDialog.qml
        qml/components/XPanel.qml
        qml/components/XButton.qml
        qml/components/XIconButton.qml
        qml/components/XTextField.qml
        qml/components/XToast.qml
        qml/components/XChatItem.qml
        qml/components/XMessageBubble.qml
        qml/components/XFeatureStub.qml
        qml/components/XUploadItem.qml
        qml/components/XBanner.qml
        qml/components/XAvatar.qml
        qml/components/XChatHeader.qml
        qml/components/XChatSidebar.qml
        qml/components/XInfoPanel.qml
        qml/components/XInfoActionButton.qml
        qml/components/XMessageInput.qml
        qml/i18n/Translations.js
    SINGLETONS
        qml/theme/Theme.qml
    RESOURCES
        assets/icons/xipher.ico
        assets/icons/eye.svg
        assets/icons/eye-off.svg
        assets/fonts/README.txt
)

target_include_directories(xipher_desktop PRIVATE src)

if(MSVC)
    target_compile_options(xipher_desktop PRIVATE /W4)
else()
    target_compile_options(xipher_desktop PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(xipher_desktop PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
    Qt6::WebSockets
    Qt6::Widgets
)

if(WIN32)
    target_link_libraries(xipher_desktop PRIVATE Crypt32)
elseif(APPLE)
    target_link_libraries(xipher_desktop PRIVATE "-framework Security")
elseif(UNIX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBSECRET REQUIRED libsecret-1)
    target_include_directories(xipher_desktop PRIVATE ${LIBSECRET_INCLUDE_DIRS})
    target_link_libraries(xipher_desktop PRIVATE ${LIBSECRET_LIBRARIES})
    target_compile_definitions(xipher_desktop PRIVATE HAVE_LIBSECRET=1)
endif()

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
