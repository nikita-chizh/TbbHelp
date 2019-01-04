
#include <iostream>
#include "tbb/tbb.h"
#include "tbb/pipeline.h"
#include <mutex>
#include <list>
#include <limits>
#include <fstream>

// parallel_do
// ключевое отличие от parallel_for и parallel_reduce это неизвестное количество итераций в цикле
// parallel_do просто берет итератор и выности в отдельный поток
void ParallelApplyFooToList( const std::list<double>& list ) {
    tbb::parallel_do( list.begin(), list.end(), [](double d){std::cout<<d<<std::endl;} );
}

// pipeline
// Задача:
// чтение текстового файла кусками, возведение в квадрат цифирок
// запись в другой файл, момент с обработкой куска и возведением в квадрат будет работать в паралель

/** Instances *must* be allocated/freed using methods herein, because the C++ declaration
   represents only the header of a much larger object in memory. */
class TextSlice {
    // Pointer to one past last character in sequence
    char* logical_end;
    // Pointer to one past last available byte in sequence.
    char* physical_end;
public:
    // Allocate a TextSlice object that can hold up to max_size characters.
    static TextSlice* allocate( size_t max_size ) {
        // +1 leaves room for a terminating null character.
        TextSlice* t = (TextSlice*)tbb::tbb_allocator<char>().allocate( sizeof(TextSlice)+max_size+1 );
        t->logical_end = t->begin();
        t->physical_end = t->begin()+max_size;
        return t;
    }
    // Free this TextSlice object
    void free() {
        tbb::tbb_allocator<char>().deallocate((char*)this, sizeof(TextSlice)+(physical_end-begin())+1);
    }
    // Pointer to beginning of sequence
    char* begin() {return (char*)(this+1);}
    // Pointer to one past last character in sequence
    char* end() {return logical_end;}
    // Length of sequence
    size_t size() const {return logical_end-(char*)(this+1);}
    // Maximum number of characters that can be appended to sequence
    size_t avail() const {return physical_end-logical_end;}
    // Append sequence [first,last) to this sequence.
    void append( char* first, char* last ) {
        memcpy( logical_end, first, last-first );
        logical_end += last-first;
    }
    // Set end() to given value.
    void set_end( char* p ) {logical_end=p;}
};

TextSlice* next_slice = NULL;

const unsigned int MAX_CHAR_PER_INPUT_SLICE = 4096;

class MyInputFunc {
public:
    explicit MyInputFunc( FILE* input_file_ ) :
            input_file(input_file_) { }

    MyInputFunc( const MyInputFunc& f ) = default;

    ~MyInputFunc() = default;
    TextSlice* operator()( tbb::flow_control& fc ) const
    {
        // Read characters into space that is available in the next slice.
        if( !next_slice )
            next_slice = TextSlice::allocate( MAX_CHAR_PER_INPUT_SLICE );
        size_t m = next_slice->avail();
        size_t n = fread( next_slice->end(), 1, m, input_file );
        if( !n && next_slice->size()==0 ) {
            // No more characters to process
            fc.stop();
            return NULL;
        } else {
            // Have more characters to process.
            TextSlice* t = next_slice;
            next_slice = TextSlice::allocate( MAX_CHAR_PER_INPUT_SLICE );
            char* p = t->end()+n;
            if( n==m ) {
                // Might have read partial number.
                // If so, transfer characters of partial number to next slice.
                while( p>t->begin() && isdigit(p[-1]) )
                    --p;
                next_slice->append( p, t->end()+n );
            }
            t->set_end(p);
            return t;
        }
    }

private:
    FILE* input_file;
};

// Functor that changes each decimal number to its square.
class MyTransformFunc {
public:
    TextSlice* operator()( TextSlice* input ) const {
        // Add terminating null so that strtol works right even if number is at end of the input.
        *input->end() = '\0';
        char* p = input->begin();
        TextSlice* out = TextSlice::allocate( 2*MAX_CHAR_PER_INPUT_SLICE );
        char* q = out->begin();
        for(;;) {
            while( p<input->end() && !isdigit(*p) )
                *q++ = *p++;
            if( p==input->end() )
                break;
            long x = strtol( p, &p, 10 );
            // Note: no overflow checking is needed here, as we have twice the
            // input string length, but the square of a non-negative integer n
            // cannot have more than twice as many digits as n.
            long y = x*x;
            sprintf(q,"%ld",y);
            q = strchr(q,0);
        }
        out->set_end(q);
        input->free();
        return out;
    }
};

// Below is the top-level code for building and running the pipeline.
// TextSlice objects are passed between filters using pointers to avoid the overhead of copying a TextSlice.
// Functor that writes a TextSlice to a file.
class MyOutputFunc {
    FILE* my_output_file;
public:
    MyOutputFunc( FILE* output_file ) :
            my_output_file(output_file)
    {
    }

    void operator()( TextSlice* out ) const {
        size_t n = fwrite( out->begin(), 1, out->size(), my_output_file );
        if( n!=out->size() ) {
            fprintf(stderr,"Can't write into file '%s'\n", "blyat");
            exit(1);
        }
        out->free();
    }
};

// Идея очень простая
// Запуск нескольких функций(фильтров) часть из которых(помеченные как tbb::filter::parallel)
// может выполняться паралельно
// Фильтры передают друг другу аргументы по цепочке
void RunPipeline(int ntoken, FILE *input_file, FILE *output_file) {
    // The filters are concatenated with operator&.
    // When concatenating two filters, the outputType of the first filter
    // must match the inputType of the second filter.
    // make_filter<T, U> вход и выход фильтра

    tbb::parallel_pipeline(
            ntoken,
            tbb::make_filter<void, TextSlice*>(
                    tbb::filter::serial_in_order, MyInputFunc(input_file))
            &
            tbb::make_filter<TextSlice *, TextSlice *>(
                    tbb::filter::parallel, MyTransformFunc())
            &
            tbb::make_filter<TextSlice *, void>(
                    tbb::filter::serial_in_order, MyOutputFunc(output_file)));
}


int main() {
//    std::list<double> list = {10400,2,3,4,-1,6,7,8,9,10};
//    ParallelApplyFooToList(list);
    FILE *input_file = fopen("/Users/nikita/CLionProjects/TbbTest/in_pipe.txt", "r");
    FILE *output_file = fopen("/Users/nikita/CLionProjects/TbbTest/out_pipe.txt", "w");
    RunPipeline(1, input_file, output_file);
    return 0;
}