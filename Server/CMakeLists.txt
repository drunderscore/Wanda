add_executable(Server
        Client.cpp
        main.cpp
        Server.cpp
        )

target_include_directories(Server SYSTEM PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_BINARY_DIR}
        )

target_link_libraries(Server PRIVATE Lagom::Core Lagom::Main SourceEngine)
