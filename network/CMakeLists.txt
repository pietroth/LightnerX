set(NETWORK_SOURCES)
set(NETWORK_MODULES
    server.cppm
    client.cppm
    connection_manager.cppm
    transport.cppm
    network_manager.cppm
    message_batch.cppm
)

if(NETWORK_SOURCES)
    target_sources(LightnerX PUBLIC ${NETWORK_SOURCES})
endif()

if(NETWORK_MODULES)
    target_sources(LightnerX
        PUBLIC
            FILE_SET CXX_MODULES
        BASE_DIRS "${PROJECT_SOURCE_DIR}"
        FILES ${NETWORK_MODULES}
    )
endif()
