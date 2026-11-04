target_sources(LightnerX
    PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS "${PROJECT_SOURCE_DIR}"
        FILES
            event.cppm
            base_protocol.cppm
            mbt.cppm
            intention.cppm
            notification.cppm
            response.cppm
            message.cppm
)
