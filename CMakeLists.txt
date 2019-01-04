cmake_minimum_required(VERSION 3.11)
project(INTEL_TBB_TUTORIALS)

include_directories(/usr/local/include)
link_directories(/usr/local/lib)
add_executable(Parallelizing_Simple_Loops Parallelizing_Simple_Loops.cpp)
target_link_libraries(Parallelizing_Simple_Loops tbb)
####
add_executable(Parallelizing_Complex_Loops Parallelizing_Complex_Loops.cpp)
target_link_libraries(Parallelizing_Complex_Loops tbb )
####
add_executable(Dependency_Graphs_Basic Dependency_Graphs_Basic.cpp)
target_link_libraries(Dependency_Graphs_Basic tbb )
####
add_executable(Graphs_Push Graphs_Push.cpp)
target_link_libraries(Graphs_Push tbb )
####
add_executable(Data_Flow_Graph Data_Flow_Graph.cpp)
target_link_libraries(Data_Flow_Graph tbb )
####
add_executable(Dependence_Graph Dependence_Graph.cpp)
target_link_libraries(Dependence_Graph tbb )

