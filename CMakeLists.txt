cmake_minimum_required(VERSION 4.0)
project(Lab3 C)

set(CMAKE_C_STANDARD 11)

add_executable(Lab3 main.c
        broker/broker_tcp.c
        broker/broker_udp.c
        publisher/publisher_tcp.c
        publisher/publisher_udp.c
        subscriber/subscriber_tcp.c
        subscriber/subscriber.udp.c)
