set(tested_src_dir ${PROJECT_SOURCE_DIR}/src)

add_executable(dump)
target_sources(dump PRIVATE
        ./dump.c

        ${tested_src_dir}/FFTAnalyzer.c
        ${tested_src_dir}/core/History.c
        ${tested_src_dir}/dsp/window.c
        ${tested_src_dir}/dsp/filters.c
)

target_include_directories(dump PRIVATE
        ${tested_src_dir}
        ${PROJECT_SOURCE_DIR}/test/third_party/dr_libs
)

target_compile_options(dump PRIVATE ${SPECTRE_WARN_FLAGS})

target_link_libraries(dump PRIVATE
        kissfft
        LockFreeQueue
        dr_libs_interface
        m
)
