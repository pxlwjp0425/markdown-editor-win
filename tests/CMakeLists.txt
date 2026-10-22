find_package(Qt6 REQUIRED COMPONENTS Test)

# Qt 运行库目录：测试是独立的 exe，ctest 直接跑时找不到 Qt6Core.dll
get_target_property(_qmake_exe Qt6::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_dir "${_qmake_exe}" DIRECTORY)

# 新增一个单元测试：参数是测试目标名（同时是源文件名）
function(add_md_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE mdcore Qt6::Test)
    # 必须是控制台子系统：qt_standard_project_setup() 会把目标设成
    # WIN32_EXECUTABLE（GUI），那样 QTest 的输出就无处可去了 ——
    # 测试失败时只看到一个退出码，看不到是哪一条断言挂了。
    set_target_properties(${name} PROPERTIES WIN32_EXECUTABLE FALSE)
    if(MSVC)
        target_compile_options(${name} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${name} PRIVATE -Wall -Wextra)
    endif()
    add_test(NAME ${name} COMMAND ${name})
    if(WIN32)
        set_tests_properties(${name} PROPERTIES
            ENVIRONMENT "PATH=${_qt_bin_dir};$ENV{PATH}")
    endif()
endfunction()

add_md_test(tst_docmodel)
add_md_test(tst_blockeditor)
add_md_test(tst_inlineformatter)
add_md_test(tst_smartpunctuation)
