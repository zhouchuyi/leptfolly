#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include"leptjson.h"

static int main_ret=0;
static int test_count=0;
static int test_pass=0;


#define EXPECT_EQ_BASE(equality,expect,actual,format) \
    do{\
        test_count++;\
        if(equality) \
            test_pass++; \
        else{ \
            fprintf(stderr,"%s:%d: expect: "format" actual: "format" \n",__FILE__,__LINE__,expect,actual);\
        }\
    } while(0)
    
#define EXPECT_EQ_INT(expect,actual) EXPECT_EQ_BASE((expect==actual),expect,actual,"%d")
#define EXPECT_EQ_DOUBLE(expect,actual) EXPECT_EQ_BASE((expect)==(actual),expect,actual,"%.17g")
#define EXPECT_EQ_STRING(expect,actual,len) \
    EXPECT_EQ_BASE(sizeof(expect)-1==len && memcmp(expect,actual,len)==0,expect,actual,"%s")

#define EXPECT_EQ_SIZE_T(expect,actual) EXPECT_EQ_BASE((expect==actual),expect,actual,"%d")
#define TEST_ERROR(error,json)\
    do{\
        lept_value v;\
        lept_init(&v);\
        v.type=LEPT_FALSE;\
        EXPECT_EQ_INT(error,lept_parse(&v,json));\
        EXPECT_EQ_INT(LEPT_NULL,lept_get_type(&v));\
        lept_free(&v);\
    }while(0)
    

static void test_parse_null()
{
    lept_value v;
    v.type=LEPT_FALSE;
    EXPECT_EQ_INT(LEPT_PARSE_OK,lept_parse(&v,"  null   "));
    EXPECT_EQ_INT(LEPT_NULL,lept_get_type(&v));
}

static void test_parse_true()
{
    lept_value v;
    v.type=LEPT_FALSE;
    EXPECT_EQ_INT(LEPT_PARSE_OK,lept_parse(&v,"  true   "));
    EXPECT_EQ_INT(LEPT_TRUE,lept_get_type(&v));
}

static void test_parse_false()
{
    lept_value v;
    v.type=LEPT_NULL;
    EXPECT_EQ_INT(LEPT_PARSE_OK,lept_parse(&v,"  false "));
    EXPECT_EQ_INT(LEPT_FALSE,lept_get_type(&v));
    EXPECT_EQ_INT(LEPT_PARSE_ROOT_NOT_SINGULAR,lept_parse(&v,"  false 11"));
}


    

#define TEST_NUMBER(expect,json)\
    do{\
        lept_value v;\
        EXPECT_EQ_INT(LEPT_PARSE_OK,lept_parse(&v,json));\
        EXPECT_EQ_INT(LEPT_NUMBER,lept_get_type(&v));\
        EXPECT_EQ_DOUBLE(expect,lept_get_number(&v));\
    }while(0)
    

static void test_parse_number()
{
    TEST_NUMBER(0.0, "0");
    TEST_NUMBER(0.0, "-0");
    TEST_NUMBER(0.0, "-0.0");
    TEST_NUMBER(1.0, "1");
    TEST_NUMBER(-1.0, "-1");
    TEST_NUMBER(1.5, "1.5");
    TEST_NUMBER(-1.5, "-1.5");
    TEST_NUMBER(3.1416, "3.1416");
    TEST_NUMBER(1E10, "1E10");
    TEST_NUMBER(1e10, "1e10");
    TEST_NUMBER(1E+10, "1E+10");
    TEST_NUMBER(1E-10, "1E-10");
    TEST_NUMBER(-1E10, "-1E10");
    TEST_NUMBER(-1e10, "-1e10");
    TEST_NUMBER(-1E+10, "-1E+10");
    TEST_NUMBER(-1E-10, "-1E-10");
    TEST_NUMBER(1.234E+10, "1.234E+10");
    TEST_NUMBER(1.234E-10, "1.234E-10");
    TEST_NUMBER(0.0, "1e-10000"); 
}

#define TEST_STRING(expect,json)\
    do{\
    lept_value v;\
    lept_init(&v);\
    EXPECT_EQ_INT(LEPT_PARSE_OK,lept_parse(&v,json));\
    EXPECT_EQ_INT(LEPT_STRING,lept_get_type(&v));\
    EXPECT_EQ_STRING(expect,lept_get_string(&v),lept_get_string_length(&v));\
    lept_free(&v);\
    } while(0)
    
static void test_access_string()
{
    lept_value v;
    lept_init(&v);
    lept_set_string(&v,"",0);
    EXPECT_EQ_STRING("",lept_get_string(&v),lept_get_string_length(&v));    
    lept_set_string(&v,"hello",5);
    EXPECT_EQ_STRING("hello",lept_get_string(&v),lept_get_string_length(&v));    

}
static void test_parse_string()
{
    TEST_STRING("","\"\"");
    TEST_STRING("hello","\"hello\"");
}
static void test_parse_invalid_string_char()
{
    TEST_ERROR(LEPT_PARSE_INVALID_STRING_CHAR, "\"\x01\"");
    TEST_ERROR(LEPT_PARSE_INVALID_STRING_CHAR, "\"\x1F\"");
}

static void test_access_bool()
{
    // to do
}
static void test_access_number()
{
    // to do
}
static void test_parse_array() {
    size_t i, j;
    lept_value v;

    /* ... */

    lept_init(&v);
    EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, "[ null , false , true , 123 , \"abc\" ]"));
    EXPECT_EQ_INT(LEPT_ARRAY, lept_get_type(&v));
    EXPECT_EQ_SIZE_T(5, lept_get_array_size(&v));
    EXPECT_EQ_INT(LEPT_NULL,   lept_get_type(lept_get_array_element(&v, 0)));
    EXPECT_EQ_INT(LEPT_FALSE,  lept_get_type(lept_get_array_element(&v, 1)));
    EXPECT_EQ_INT(LEPT_TRUE,   lept_get_type(lept_get_array_element(&v, 2)));
    EXPECT_EQ_INT(LEPT_NUMBER, lept_get_type(lept_get_array_element(&v, 3)));
    EXPECT_EQ_INT(LEPT_STRING, lept_get_type(lept_get_array_element(&v, 4)));
    EXPECT_EQ_DOUBLE(123.0, lept_get_number(lept_get_array_element(&v, 3)));
    EXPECT_EQ_STRING("abc", lept_get_string(lept_get_array_element(&v, 4)), lept_get_string_length(lept_get_array_element(&v, 4)));
    lept_free(&v);

    lept_init(&v);
    EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, "[ [ ] , [ 0 ] , [ 0 , 1 ] , [ 0 , 1 , 2 ] ]"));
    EXPECT_EQ_INT(LEPT_ARRAY, lept_get_type(&v));
    EXPECT_EQ_SIZE_T(4, lept_get_array_size(&v));
    for (i = 0; i < 4; i++) {
        lept_value* a = lept_get_array_element(&v, i);
        EXPECT_EQ_INT(LEPT_ARRAY, lept_get_type(a));
        EXPECT_EQ_SIZE_T(i, lept_get_array_size(a));
        for (j = 0; j < i; j++) {
            lept_value* e = lept_get_array_element(a, j);
            EXPECT_EQ_INT(LEPT_NUMBER, lept_get_type(e));
            EXPECT_EQ_DOUBLE((double)j, lept_get_number(e));
        }
    }
    lept_free(&v);
}
static void test_parse()
{
    // test_parse_number();
    // test_access_string();
    test_parse_array();
}

int main(int argc, char const *argv[])
{
    test_parse();
    printf("%d/%d (%3.2f%%) passed\n",test_pass,test_count,test_pass/test_count*100.0);
    return 0;
}

