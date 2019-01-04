// Нужно для частичного упорядовачивания вызовов нод
#include <iostream>
#include "tbb/tbb.h"
// порядок исполнения надо посмотреть тут:
// https://software.intel.com/en-us/node/517349
// порядок исполнения задаваемый через continue_msg достаточно понятен

typedef tbb::flow::continue_node< tbb::flow::continue_msg > node_t;
typedef const tbb::flow::continue_msg & msg_t;
void a(){}
void b(){}
void c(){}
void d(){}
void e(){}
void f(){}


int main() {
    tbb::flow::graph g;
    node_t A(g, [](msg_t){ a(); } );
    node_t B(g, [](msg_t){ b(); } );
    node_t C(g, [](msg_t){ c(); } );
    node_t D(g, [](msg_t){ d(); } );
    node_t E(g, [](msg_t){ e(); } );
    node_t F(g, [](msg_t){ f(); } );
    make_edge(A, B);
    make_edge(B, C);
    make_edge(B, D);
    make_edge(A, E);
    make_edge(E, D);
    make_edge(E, F);
    A.try_put( tbb::flow::continue_msg() );
    g.wait_for_all();
    return 0;
}
