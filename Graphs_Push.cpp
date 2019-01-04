#include <iostream>
#include "tbb/tbb.h"


tbb::atomic<size_t> g_cnt;

struct fn_body1 {
    tbb::atomic<size_t> &body_cnt;
    fn_body1(tbb::atomic<size_t> &b_cnt) : body_cnt(b_cnt) {}
    tbb::flow::continue_msg operator()( tbb::flow::continue_msg /*dont_care*/) {
        ++g_cnt;
        ++body_cnt;
        return tbb::flow::continue_msg();
    }
};

// Вывод:
// after single-push test, g_cnt == 3, b1==3, b2==0, b3==0
// after broadcast-push test, g_cnt == 9, b1==3, b2==3, b3==3
// Объяснение: все сообщения получила только одна нода в 1м случае
void run_example1() {
    tbb::flow::graph g;
    tbb::atomic<size_t> b1;  // local counts
    tbb::atomic<size_t> b2;  // for each function _node body
    tbb::atomic<size_t> b3;  //
    tbb::flow::function_node<tbb::flow::continue_msg> f1(g,tbb::flow::serial,fn_body1(b1));
    tbb::flow::function_node<tbb::flow::continue_msg> f2(g,tbb::flow::serial,fn_body1(b2));
    tbb::flow::function_node<tbb::flow::continue_msg> f3(g,tbb::flow::serial,fn_body1(b3));
    // гарантирует что полученное нодой сообщение не потеряется
    tbb::flow::buffer_node<tbb::flow::continue_msg> buf1(g);
    //
    // single-push policy
    //
    g_cnt = b1 = b2 = b3 = 0;
    make_edge(buf1,f1);
    make_edge(buf1,f2);
    make_edge(buf1,f3);
    buf1.try_put(tbb::flow::continue_msg());
    buf1.try_put(tbb::flow::continue_msg());
    buf1.try_put(tbb::flow::continue_msg());
    g.wait_for_all();
    printf( "after single-push test, g_cnt == %d, b1==%d, b2==%d, b3==%d\n", (int)g_cnt, (int)b1, (int)b2, (int)b3);
    remove_edge(buf1,f1);
    remove_edge(buf1,f2);
    remove_edge(buf1,f3);
    //
    // broadcast-push policy
    //
    tbb::flow::broadcast_node<tbb::flow::continue_msg> bn(g);
    g_cnt = b1 = b2 = b3 = 0;
    make_edge(bn,f1);
    make_edge(bn,f2);
    make_edge(bn,f3);
    bn.try_put(tbb::flow::continue_msg());
    bn.try_put(tbb::flow::continue_msg());
    bn.try_put(tbb::flow::continue_msg());
    g.wait_for_all();
    printf( "after broadcast-push test, g_cnt == %d, b1==%d, b2==%d, b3==%d\n", (int)g_cnt, (int)b1, (int)b2, (int)b3);
}

using namespace tbb::flow;
// пример join_node: такая нода, которая не передаст сообщение дальше
// пока не придут сообщения на все входы
// У join_node есть 4 политики: queueing, reserving, key_matching and tag_matching.
// join_nodes должны получить сообщение на каждый вход до того как смогут создать свое.
// join_node не имеет внутреннего буфера,
// и не спуливает сообщения с входящих нод, пока не получит сообщения на каждый вход.
// Чтобы создать выходное сообщение join_node временно хранит сообщения с каждого порта,
// и ТОЛЬКО если ВСЕ успешно пришли все будет создано выходное.

// Чтобы поддержать reserving(сохранение сообщений) join_node-ой
// некоторые ноды поддерживают сохранение их выходов. Это работает так:
// 1. Когда нода соединяется с принимающей join_node в push-state пытаясь отправить сообщение,
// join_node всегда его отвергает и переключает грань в pull-state.

// 2. Входной порт вызывает try_reserve на каждую грань в pull-state.
// Если вызов проваливается резервный input-port переключает грань в to push-state,
// и пытается перевести в reserve-state следующую ноду в pull state.
// Пока входящая в порт нода в reserved-state, никакая другая нода не
// может получить из нее значение.

// 3. Если каждый входной порт успешно зарезервит шрань из in pull-state-а,
// join_node создаст из выходное сообщение из входящих и передаст его дальше.
// Если сообщение успешно передано, ноды в  reserved-state получают сигнал что сообщение
// ушло (вызовом try_consume().)
// Сообщения будут ими сброшенны потому что оно было успешно запушено.
// Если сообщения не были переданны, нодам в reserved-state сигнализируют что
// сообщения не были использованны (by calling try_release().)
// В этой точке, сообщения могут быть полученны другими нодами.
void run_example2() {  // example for Flow_Graph_Reservation.xml
    graph g;
    broadcast_node<int> broadcast_n(g);
    buffer_node<int> buffer_node1(g);
    buffer_node<int> buffer_node2(g);
    // join_node не выдает output пока не получит сообщения со всех входов
    typedef join_node<tuple<int,int>, reserving> join_type;
    join_type jn(g);
    buffer_node<join_type::output_type> buf_out(g);
    join_type::output_type tuple_out;
    int icnt;

    // join_node predecessors are both reservable buffer_nodes
    make_edge(broadcast_n,  tbb::flow::input_port<0>(jn));      // attach a broadcast_node
    //make_edge(buffer_node1, tbb::flow::input_port<0>(jn));
    make_edge(buffer_node2, tbb::flow::input_port<1>(jn));
    make_edge(jn, buf_out);
    //
    // Это сообщение дропнется поскольку, запушить его не удастся (не все входы готовы)
    // а зарезервировать его нельзя (broadcast_node не имеет буфера)
    broadcast_n.try_put(2);
    buffer_node1.try_put(3);
    buffer_node2.try_put(4);
    buffer_node2.try_put(7);
    g.wait_for_all();
    while (buf_out.try_get(tuple_out)) {
        printf("join_node output == (%d,%d)\n",get<0>(tuple_out), get<1>(tuple_out) );
    }
    if(buffer_node1.try_get(icnt)) printf("buffer_node1 had %d\n", icnt);
    else printf("buffer_node1 was empty\n");
    if(buffer_node2.try_get(icnt)) printf("buffer_node2 had %d\n", icnt);
    else printf("buffer_node2 was empty\n");
}

int main()
{
    run_example2();

}