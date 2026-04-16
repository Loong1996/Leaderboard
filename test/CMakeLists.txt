add_executable(benchmark_leaderboard BenchmarkLeaderboard.cpp)
target_include_directories(benchmark_leaderboard PRIVATE ${PROJECT_SOURCE_DIR}/include/)

add_test(
	NAME TestLeaderboard
	COMMAND $<TARGET_FILE:benchmark_leaderboard> "${PROJECT_BINARY_DIR}/report.html")
