add_subdirectory(third_party/Unity-2.6.1)

set(DR_LIBS_SANITIZE_ADDRESS ON CACHE BOOL "Enable AddressSanitizer" FORCE)
add_subdirectory(third_party/dr_libs)

add_subdirectory(dsp)
add_subdirectory(dump)
