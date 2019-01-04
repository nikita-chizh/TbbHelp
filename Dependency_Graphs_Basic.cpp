#include <iostream>
#include "tbb/tbb.h"
#include <unistd.h>


void spin_for(unsigned int msecs)
{
    usleep(msecs);
}
int main()
{
    //
    tbb::flow::graph g;
    tbb::flow::function_node< int, int > n( g, 10, []( int v ) -> int {
        std::cout<<"N1="<<v<<std::endl;
        spin_for(v);
        std::cout<<"N2"<<v<<std::endl;
        return v;
    } );
    //
    tbb::flow::function_node< int, int > m( g, 1, []( int v ) -> int {
        v *= v;
        std::cout<<"M1="<<v<<std::endl;
        spin_for(v);
        std::cout<<"M2"<<v<<std::endl;
        return v;
    } );
    // edge соединяет 2 функции, первая передает результат во 2ю
    // 2 аргумент, максимальное кол-во потоков для обрадотки ноды
    // ВСЕ m-ноды будут выполняться последовательно тк concurrency = 1
    // Грани работают по push-pull протоколу,
    // Если m не занята, она запросит сообщение у n (спулит)
    // если у n есть сообщение для n оно его передаст и грань останется в pull-state
    // если сообщения нет грань окажется в push-state (то есть в любой момент n может послать в m)
    make_edge( n, m );
    for(int i = 0; i < 10; ++i){
        n.try_put( i );
    }
    g.wait_for_all();

}