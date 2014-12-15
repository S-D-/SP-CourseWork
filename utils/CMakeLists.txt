add_library(utils utils.c usersxmlparser.c usersinfo.c usersconfig.c)
target_include_directories(utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
