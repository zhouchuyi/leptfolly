#ifndef LEPTJSON_H_
#define LEPTJSON_H_
#include<stdlib.h>
typedef enum {LEPT_NULL,LEPT_FALSE,LEPT_TRUE,LEPT_NUMBER,LEPT_STRING,LEPT_ARRAY,LEPT_OBJECT} lept_type;

typedef struct lept_value lept_value; 
typedef struct lept_member lept_member;

struct lept_value
{
    lept_type type;
    union 
    {
        struct {char*s; size_t len; }string;
        struct {lept_value* e;size_t size;}array;
        struct { lept_member* m;size_t size; }object;
        double n;
    };
};

struct lept_member
{
    char* k; size_t klen;
    lept_value v; 
};


typedef struct 
{
    const char* json;
    char* stack;
    size_t size,top;

} lept_context;

enum{
    LEPT_PARSE_OK=0,
    LEPT_PARSE_EXPECT_VALUE,
    LEPT_PARSE_INVALID_VALUE,
    LEPT_PARSE_ROOT_NOT_SINGULAR,
    LEPT_PARSE_MISS_QUOTATION_MARK,
    LEPT_PARSE_INVALID_STRING_CHAR,
    LEPT_PARSE_INVALID_STRING_ESCAPE,
    LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET,
    LEPT_PARSE_MISS_KEY,
    LEPT_PARSE_MISS_COLON
};

int lept_parse(lept_value* v,const char* json);
lept_type lept_get_type(const lept_value* v);

double lept_get_number(const lept_value* v);
void lept_set_number(lept_value* v,double n);

void lept_free(lept_value* v);

#define lept_set_null(v) lept_free(v);
#define lept_init(v) do{ (v)->type=LEPT_NULL; } while(0)

void lept_set_string(lept_value* v,const char*s,size_t len);
const char* lept_get_string(const lept_value* v);
size_t lept_get_string_length(const lept_value* v);

int lept_get_boolean(const lept_value* v);
void lept_set_boolean(lept_value* v,int b);

size_t lept_get_array_size(const lept_value* v);

lept_value* lept_get_array_element(const lept_value* v,size_t index);

size_t lept_get_object_size(const lept_value* v);
const char* lept_get_object_key(const lept_value* v,size_t index);
size_t lept_get_object_key_length(const lept_value* v,size_t index);
lept_value* lept_get_object_value(const lept_value* v,size_t index);

#endif