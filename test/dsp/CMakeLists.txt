set(tested_src_dir ${PROJECT_SOURCE_DIR}/src)

add_executable(test_dsp)
target_sources(test_dsp PRIVATE
        ./test_dsp.c

        ${tested_src_dir}/dsp/window.c
)

target_include_directories(test_dsp PRIVATE
        ${tested_src_dir}
)

target_link_libraries(test_dsp PRIVATE
        unity
)
