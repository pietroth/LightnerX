set(NETWORK_SOURCES)
set(NETWORK_MODULES
    server.cppm
    client.cppm
    send.cppm
    client_manager.cppm
)

if(NETWORK_SOURCES)
    target_sources(LightnerX PRIVATE ${NETWORK_SOURCES})
endif()

if(NETWORK_MODULES)
    target_sources(LightnerX
        PRIVATE
            FILE_SET CXX_MODULES
        BASE_DIRS "${PROJECT_SOURCE_DIR}"
        FILES ${NETWORK_MODULES}
    )
endif()
