add_executable(Test
        main.cpp
        )

target_include_directories(Test SYSTEM PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_BINARY_DIR}
        )

target_link_libraries(Test PRIVATE Lagom::Core Lagom::Main)
