target_sources(LightnerX
    PRIVATE
        client.cpp
        connection_manager.cpp
        message_batch.cpp
        network_manager.cpp
        server.cpp
        target.cpp
        transport.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
        FILES
            "${PROJECT_SOURCE_DIR}/include/lightnerx/network/client.hpp"
            "${PROJECT_SOURCE_DIR}/include/lightnerx/network/connection_manager.hpp"
            "${PROJECT_SOURCE_DIR}/include/lightnerx/network/message_batch.hpp"
            "${PROJECT_SOURCE_DIR}/include/lightnerx/network/network_manager.hpp"
            "${PROJECT_SOURCE_DIR}/include/lightnerx/network/server.hpp"
            "${PROJECT_SOURCE_DIR}/include/lightnerx/network/target.hpp"
            "${PROJECT_SOURCE_DIR}/include/lightnerx/network/transport.hpp"
)
