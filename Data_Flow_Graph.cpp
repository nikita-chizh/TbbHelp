#include <iostream>
#include "tbb/tbb.h"
// Концепция Data Flow графа весьма проста
// Это просто несколько нод, которые передают друг другу сообщения

int sum = 0;
tbb::flow::graph g;
tbb::flow::function_node< int, int > squarer( g, tbb::flow::unlimited, [](const int &v) {
    return v*v;
} );
tbb::flow::function_node< int, int > cuber( g, tbb::flow::unlimited, [](const int &v) {
    return v*v*v;
} );
// concurrency == 1, суматор должен быть потокобезопасным
tbb::flow::function_node< int, int > summer( g, 1, [](const int &v ) -> int {
    return sum += v;
} );


void first_flow()
{
    make_edge( squarer, summer );
    make_edge( cuber, summer );
    for ( int i = 1; i <= 10; ++i ) {
        squarer.try_put(i);
        cuber.try_put(i);
    }
    g.wait_for_all();
    std::cout << "Sum is " << sum << "\n";
}

// Теперь вход 1 это broadcast_node
void second_flow()
{
    tbb::flow::broadcast_node<int> b(g);
    make_edge( b, squarer );
    make_edge( b, cuber );
    //
    make_edge( squarer, summer );
    make_edge( cuber, summer );
    for ( int i = 1; i <= 10; ++i ) {
        b.try_put(i);
    }
    g.wait_for_all();
    g.wait_for_all();
    std::cout << "Sum is " << sum << "\n";

}

//
class src_body {
    const int my_limit;
    int my_next_value;
public:
    explicit src_body(int l) : my_limit(l), my_next_value(1) {}
    // Будет дергаться пока не вернет false
    bool operator()( int &v ) {
        if ( my_next_value <= my_limit ) {
            v = my_next_value++;
            return true;
        } else {
            return false;
        }
    }
};

//Теперь начало потока данныз это source_node
void third_flow()
{
    make_edge( squarer, summer );
    make_edge( cuber, summer );
    tbb::flow::source_node< int > src( g, src_body(10), false );
    make_edge( src, squarer );
    make_edge( src, cuber );
    src.activate();
    g.wait_for_all();
    std::cout << "Sum is " << sum << "\n";

}

int main()
{
    third_flow();
}