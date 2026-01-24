cmake_minimum_required(VERSION 3.15)
project(XipherServer VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")

# Find required packages
find_package(Boost REQUIRED COMPONENTS system filesystem)
find_package(OpenSSL REQUIRED)
find_package(PostgreSQL REQUIRED)
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)
find_library(ARGON2_LIBRARY argon2)
if(NOT ARGON2_LIBRARY)
    message(FATAL_ERROR "libargon2 not found. Install libargon2-dev (or equivalent).")
endif()

# Boost Beast is header-only, just need to include it
set(BOOST_INCLUDE_DIRS ${Boost_INCLUDE_DIRS})

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${PostgreSQL_INCLUDE_DIRS})
include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/third_party/miniaudio)
include_directories(${CMAKE_SOURCE_DIR}/third_party/argon2)

# VoIP dependencies
# Uncomment and configure paths as needed

# Redis (for signaling sharding)
# find_path(HIREDIS_INCLUDE_DIR hiredis/hiredis.h)
# find_library(HIREDIS_LIBRARY hiredis)
# find_path(LIBEVENT_INCLUDE_DIR event2/event.h)
# find_library(LIBEVENT_LIBRARY event)

# Opus codec (for audio encoding/decoding)
find_path(OPUS_INCLUDE_DIR opus/opus.h
    PATHS
    /usr/include
    /usr/local/include
    /opt/homebrew/include
)
find_library(OPUS_LIBRARY opus
    PATHS
    /usr/lib
    /usr/local/lib
    /opt/homebrew/lib
)

find_library(OPUS_LIBRARY opus
    PATHS
    /usr/lib
    /usr/local/lib
    /opt/homebrew/lib
)

# miniaudio (header-only, can be included directly)
# Download from: https://github.com/mackron/miniaudio
# Place miniaudio.h in include/voip/ or set path below
# set(MINIAUDIO_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include/voip")

# libdatachannel (for WebRTC/NAT traversal)
# find_path(LIBDATACHANNEL_INCLUDE_DIR rtc/rtc.hpp
#     PATHS
#     /usr/include
#     /usr/local/include
# )
# find_library(LIBDATACHANNEL_LIBRARY datachannel
#     PATHS
#     /usr/lib
#     /usr/local/lib
# )

# Source files
set(SOURCES
    src/server/http_server.cpp
    src/server/admin_handler.cpp
    src/server/request_handler.cpp
    src/server/request_handler_marketplace.cpp
    src/bots/lite_bot_runtime.cpp
    src/bots/bot_scheduler.cpp
    src/bots/python_bot_executor.cpp
    src/database/db_connection.cpp
    src/database/db_manager.cpp
    src/database/db_manager_friends.cpp
    src/database/db_manager_groups.cpp
    src/database/db_manager_channels.cpp
    src/database/db_manager_topics.cpp
    src/database/db_manager_marketplace.cpp
    src/database/db_manager_integrations.cpp
    src/database/db_manager_bot_builder.cpp
    src/database/db_manager_bot_runtime.cpp
    src/database/db_manager_bot_scripts.cpp
    src/database/db_manager_bot_developers.cpp
    src/database/db_manager_bot_files.cpp
    src/database/db_manager_event_router.cpp
    src/server/request_handler_integrations.cpp
    src/server/request_handler_bot_builder.cpp
    src/server/request_handler_event_router.cpp
    src/server/request_handler_bot_api.cpp
    src/storage/in_memory_storage.cpp
    src/auth/auth_manager.cpp
    src/auth/password_hash.cpp
    src/security/admin_security.cpp
    src/crypto/e2ee.cpp
    src/crypto/e2ee_manager.cpp
    src/server/request_handler_e2ee.cpp
    src/server/request_handler_wallet.cpp
    src/server/request_handler_wallet_features.cpp
    src/utils/json_parser.cpp
    src/utils/logger.cpp
    src/notifications/fcm_client.cpp
    src/notifications/rustore_client.cpp
    # VoIP sources
    src/voip/voip_access_control.cpp
    src/voip/signaling_protocol.cpp
    src/voip/redis_signaling_bridge.cpp
    src/voip/jitter_buffer.cpp
    src/voip/audio_engine.cpp
    src/voip/signaling_manager.cpp
    src/voip/nat_traversal.cpp
    third_party/miniaudio/miniaudio.c
    src/main.cpp
)

# Header files
set(HEADERS
    include/server/http_server.hpp
    include/server/request_handler.hpp
    include/database/db_connection.hpp
    include/database/db_manager.hpp
    include/auth/auth_manager.hpp
    include/auth/password_hash.hpp
    include/utils/json_parser.hpp
    include/utils/logger.hpp
    include/notifications/fcm_client.hpp
    include/notifications/rustore_client.hpp
    # E2EE headers
    include/crypto/e2ee.hpp
    include/crypto/e2ee_manager.hpp
    # VoIP headers
    include/voip/voip_access_control.hpp
    include/voip/signaling_protocol.hpp
    include/voip/redis_signaling_bridge.hpp
    include/voip/jitter_buffer.hpp
    include/voip/lockfree_ring_buffer.hpp
    include/voip/audio_engine.hpp
    include/voip/nat_traversal.hpp
)

# Executable
add_executable(xipher_server ${SOURCES} ${HEADERS})

# Include directories for VoIP
if(OPUS_INCLUDE_DIR)
    include_directories(${OPUS_INCLUDE_DIR})
endif()

# Link libraries
target_link_libraries(xipher_server
    ${Boost_LIBRARIES}
    ${PostgreSQL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    CURL::libcurl
    Threads::Threads
    pthread
    ${ARGON2_LIBRARY}
    # VoIP libraries (uncomment when installed)
    # ${HIREDIS_LIBRARY}
    # ${LIBEVENT_LIBRARY}
    ${OPUS_LIBRARY}
    # ${LIBDATACHANNEL_LIBRARY}
)

# Installation
install(TARGETS xipher_server DESTINATION bin)
