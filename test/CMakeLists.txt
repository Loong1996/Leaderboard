add_executable(test_leaderboard TestLeaderboard.cpp)
target_include_directories(test_leaderboard PRIVATE ${PROJECT_SOURCE_DIR}/include/)

add_test(NAME TestLeaderboard COMMAND test_leaderboard)
