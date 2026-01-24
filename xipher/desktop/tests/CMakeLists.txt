set(TEST_SUPPORT_SOURCES
    ${CMAKE_SOURCE_DIR}/src/app/AppConfig.cpp
    ${CMAKE_SOURCE_DIR}/src/app/Session.cpp
    ${CMAKE_SOURCE_DIR}/src/app/SessionStore.cpp
    ${CMAKE_SOURCE_DIR}/src/app/FileSecureStorage.cpp
    ${CMAKE_SOURCE_DIR}/src/models/ApiDtos.cpp
    ${CMAKE_SOURCE_DIR}/src/models/Chat.cpp
    ${CMAKE_SOURCE_DIR}/src/models/Message.cpp
    ${CMAKE_SOURCE_DIR}/src/logging/Log.cpp
    ${CMAKE_SOURCE_DIR}/src/net/ApiClient.cpp
    ${CMAKE_SOURCE_DIR}/src/net/CookieJar.cpp
    ${CMAKE_SOURCE_DIR}/src/net/UploadManager.cpp
    ${CMAKE_SOURCE_DIR}/src/net/WsClient.cpp
    ${CMAKE_SOURCE_DIR}/src/services/FileDialogService.cpp
)

add_library(xipher_desktop_test_support STATIC ${TEST_SUPPORT_SOURCES})
target_include_directories(xipher_desktop_test_support PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(xipher_desktop_test_support PUBLIC Qt6::Core Qt6::Network Qt6::WebSockets Qt6::Widgets)
target_compile_features(xipher_desktop_test_support PRIVATE cxx_std_17)

function(add_xipher_test name file)
    add_executable(${name} ${file})
    target_link_libraries(${name} PRIVATE xipher_desktop_test_support Qt6::Test)
    target_compile_features(${name} PRIVATE cxx_std_17)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_xipher_test(test_json_parsing ${CMAKE_CURRENT_SOURCE_DIR}/test_json_parsing.cpp)
add_xipher_test(test_url_building ${CMAKE_CURRENT_SOURCE_DIR}/test_url_building.cpp)
add_xipher_test(test_cookie_storage ${CMAKE_CURRENT_SOURCE_DIR}/test_cookie_storage.cpp)
add_xipher_test(test_integration ${CMAKE_CURRENT_SOURCE_DIR}/test_integration.cpp)
add_xipher_test(test_secure_storage ${CMAKE_CURRENT_SOURCE_DIR}/test_secure_storage.cpp)
add_xipher_test(test_dialog_service ${CMAKE_CURRENT_SOURCE_DIR}/test_dialog_service.cpp)
add_xipher_test(test_upload_limits ${CMAKE_CURRENT_SOURCE_DIR}/test_upload_limits.cpp)
