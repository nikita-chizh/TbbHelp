#include <iostream>
#include "tbb/tbb.h"
#include <mutex>
#include <limits>

using namespace tbb;

std::mutex m;

//// parallel_for различные варианты
void Foo(float& n)
{
    //std::lock_guard<std::mutex>lock(m);
    n = 10;
    std::cout<<n*3<<std::endl;
}

class ApplyFoo {
    float *const my_a;
public:
    // константный оператор чтобы не менять данные из разных потоков
    void operator()( const blocked_range<size_t>& r ) const {
        float *a = my_a;
        for( size_t i=r.begin(); i!=r.end(); ++i )
            Foo(a[i]);
    }
    explicit ApplyFoo( float a[] ) :
            my_a(a)
    {}
};

void ParallelApplyFooFunctor( float a[], size_t n ) {
    parallel_for(blocked_range<size_t>(0,n), ApplyFoo(a));
}


void ParallelApplyFoo( float a[], size_t n ) {
    // Положим ко-во итераций от 0 to n-1. The
    // функция tbb::parallel_for разбивает этот отрезок на куски и гоняет каждый,
    // в своем потоке
    parallel_for( size_t(0), n, [&]( size_t i ) {
        Foo(a[i]);
    } );
}

void ParallelApplyFoo_range( float* a, size_t n ) {
    // blocked_range<T>(begin,end,grainsize). T это тип ШАГА ИТЕРАЦИИ.
    // begin и end полуоткрытый интервал как обычно
    //  grainsize -> TODO, что то про разбиение
    parallel_for( blocked_range<size_t>(0,n),
                  [a](const blocked_range<size_t>& r) {
                      for(size_t i=r.begin(); i!=r.end(); ++i)
                          Foo(a[i]);
                  }
    );
}

// parallel_reduce
class SumFoo {
    float* array;

    template <typename T>
    T sumator(const T& element)
    {
        return element * 2;
    }
public:
    float result;
    void operator()( const blocked_range<size_t>& r ) {
        float *a = array;
        float sum = result;
        size_t end = r.end();
        for( size_t i=r.begin(); i!=end; ++i )
            sum += sumator(a[i]);
        result = sum;
    }

    // конструктор разбивающий для кишок библиотеки, tbb::split нужен
    // чтобы отличать его от копирующего конструктора

    // Работает, конкуретно, так что если в x есть какие то данные,
    // которые меняются в operator() и мы хотим использовать их тут
    // они должны быть потокобезопасны
    SumFoo( SumFoo& x, tbb::split ) : array(x.array), result(0) {}
    // обязательно для parallel_reduce
    // вызов происходит в вызывающем потоке после завершения operator() в паралельном потоке
    void join( const SumFoo& y ) {result+=y.result;}
    //
    explicit SumFoo(float* a ) :
            array(a), result(0)
    {}
};

float ParallelSumFoo( float a[], size_t n ) {
    SumFoo sf(a);
    parallel_reduce( blocked_range<size_t>(0,n), sf );
    return sf.result;
}
// Поиск минимального элемента массива
class MinIndexFoo {
    const float *const my_a;
public:
    float value_of_min;
    long index_of_min;
    void operator()( const blocked_range<size_t>& r ) {
        const float *a = my_a;
        for( size_t i=r.begin(); i!=r.end(); ++i ) {
            float value = a[i];
            if( value<value_of_min ) {
                value_of_min = value;
                index_of_min = i;
            }
        }
    }

    MinIndexFoo( MinIndexFoo& x, split ) :
            my_a(x.my_a),
            value_of_min(std::numeric_limits<float>::max()),
            index_of_min(-1)
    {}

    void join( const MinIndexFoo& y ) {
        if( y.value_of_min<value_of_min ) {
            value_of_min = y.value_of_min;
            index_of_min = y.index_of_min;
        }
    }

    MinIndexFoo( const float a[] ) :
            my_a(a),
            value_of_min(std::numeric_limits<float>::max()),    // FLT_MAX from <climits>
            index_of_min(-1)
    {}
};

float ParallelMinimum( float a[], size_t n ) {
    MinIndexFoo sf(a);
    parallel_reduce( blocked_range<size_t>(0,n), sf );
    return sf.index_of_min;
}



int main() {
    float a[10] {10400,2,3,4,-1,6,7,8,9,10};
    auto res = ParallelMinimum(a, 10);
    std::cout<<res;
    return 0;
}