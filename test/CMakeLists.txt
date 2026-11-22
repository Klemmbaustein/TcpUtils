add_executable(TcpTest "test.cpp")
target_link_libraries(TcpTest PUBLIC TcpUtils)

set_property(TARGET TcpTest PROPERTY CXX_STANDARD 23)