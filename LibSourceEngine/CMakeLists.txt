add_library(SourceEngine SHARED
        BitStream.cpp
        Packet.cpp
        )

target_include_directories(SourceEngine SYSTEM PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_BINARY_DIR}
        )

target_link_libraries(SourceEngine PRIVATE Lagom::Core Lagom::Crypto)
