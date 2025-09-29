cmake_minimum_required(VERSION 4.0)
project(L3 C)

set(CMAKE_C_STANDARD 11)

add_executable(publisher_tcp src/publisher/publisher_tcp.c)
add_executable(subscriber_tcp src/subscriber/subscriber_tcp.c)
add_executable(broker_tcp src/broker/broker_tcp.c)

add_executable(publisher_udp src/publisher/publisher_udp.c)
add_executable(subscriber_udp src/subscriber/subscriber_udp.c)
add_executable(broker_udp src/broker/broker_udp.c)