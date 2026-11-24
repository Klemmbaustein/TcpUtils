add_executable(HttpTest "TestHttp.cpp")
target_link_libraries(HttpTest PUBLIC TcpUtils)

set_property(TARGET HttpTest PROPERTY CXX_STANDARD 23)

add_executable(WebSocketTest "TestSocket.cpp")
target_link_libraries(WebSocketTest PUBLIC TcpUtils)

set_property(TARGET WebSocketTest PROPERTY CXX_STANDARD 23)
