# engine_core.h icin Qt'siz saf C++ test - herhangi bir Qt kutuphanesine baglanmiyor,
# sadece proje kok dizinini include path'e ekliyor (NFR-05, engine_core.h Qt'den bagimsiz olmali).
add_executable(test_engine_core test_engine_core.cpp)
target_include_directories(test_engine_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_test(NAME test_engine_core COMMAND test_engine_core)

# EngineModel (Qt koprusu) icin QtTest tabanli entegrasyon testi.
add_executable(test_enginemodel test_enginemodel.cpp)
target_link_libraries(test_enginemodel PRIVATE enginecore Qt5::Test)
add_test(NAME test_enginemodel COMMAND test_enginemodel)
