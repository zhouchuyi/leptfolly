#include"leptjson.h"
#include<assert.h>
#include<stdlib.h>
#include<string.h>
#define EXPECT(c,ch) do{ assert(*(c->json)==(ch));c->json++; } while(0)
#define ISDIGIT(ch) ((ch)>='0' && ch<='9')
#define ISDIGIT09(ch) ((ch)>='1' && ch<='9')
#define ISALPA(ch) (((ch)>='a'&&(ch)<='z')||((ch)>='A'&&(ch)<='Z'))
static void lept_parse_whitespace(lept_context* c);
static int lept_parse_null(lept_context* c,lept_value* v);
static int lept_parse_true(lept_context* c,lept_value* v);
static int lept_parse_false(lept_context* c,lept_value* v);
static int lept_parse_value(lept_context* c,lept_value* v);
static int lept_parse_number(lept_context* c,lept_value* v);
static int lept_parse_string(lept_context* c,lept_value* v);
static int lept_parse_array(lept_context* c,lept_value* v);
static int lept_parse_literal(lept_context* c,lept_value* v,const char* literal,lept_type type);
lept_type lept_get_type(const lept_value* v){return v->type;}

static void lept_parse_whitespace(lept_context* c)
{
    const char* p=c->json;
    while (*p=='\r'||*p=='\n'||*p==' '||*p=='\t')
    {
        ++p;
    }
    c->json=p; 
}
static int lept_parse_null(lept_context* c,lept_value* v)
{
    EXPECT(c,'n');
    if(c->json[0]!='u'||c->json[1]!='l'||c->json[2]!='l')
        return LEPT_PARSE_INVALID_VALUE;
    c->json+=3;
    lept_init(v);
    return LEPT_PARSE_OK;
}
static int lept_parse_true(lept_context* c,lept_value* v)
{
    EXPECT(c,'t');
    if(c->json[0]!='r'||c->json[1]!='u'||c->json[2]!='e')
        return  LEPT_PARSE_INVALID_VALUE;
    c->json+=3;
    v->type=LEPT_TRUE;
    return LEPT_PARSE_OK;
}
static int lept_parse_false(lept_context* c,lept_value* v)
{
    EXPECT(c,'f');
    if(c->json[0]!='a'||c->json[1]!='l'||c->json[2]!='s'||c->json[3]!='e')
        return LEPT_PARSE_INVALID_VALUE;
    c->json+=4;
    v->type=LEPT_FALSE;
    return LEPT_PARSE_OK;
}

static int lept_parse_value(lept_context* c,lept_value* v)
{
    switch (*c->json)
    {
    case 'n': return lept_parse_literal(c,v,"null",LEPT_NULL);
    case 't': return lept_parse_literal(c,v,"true",LEPT_TRUE);
    case 'f': return lept_parse_literal(c,v,"false",LEPT_FALSE);
    case '\"': return lept_parse_string(c,v);
    case '[': return lept_parse_array(c,v);
    case '\0': return LEPT_PARSE_EXPECT_VALUE;
    default: return lept_parse_number(c,v);
    }
}
int lept_parse(lept_value* v,const char* json)
{
    lept_context c;
    int ret;
    assert(v!=NULL);
    c.json=json;
    c.stack=NULL;
    c.top=c.size=0;
    lept_init(v);
    lept_parse_whitespace(&c);
    if((ret=lept_parse_value(&c,v))==LEPT_PARSE_OK)
    {
        lept_parse_whitespace(&c);
        if(c.json[0]!='\0')
            ret=LEPT_PARSE_ROOT_NOT_SINGULAR;
    }
    return ret;
}

double lept_get_number(const lept_value* v)
{
    assert(v!=NULL&&v->type==LEPT_NUMBER);
    return v->n;
}

static int lept_parse_literal(lept_context* c,lept_value* v,const char* literal,lept_type type)
{
    EXPECT(c,literal[0]);
    size_t i;
    for (i = 0; literal[i+1]; i++)
    {
        if(c->json[i]!=literal[i+1])
            return LEPT_PARSE_INVALID_VALUE;
    }
    c->json+=i;
    v->type=type;
    return LEPT_PARSE_OK;
}


static int lept_parse_number(lept_context* c,lept_value* v)
{
    const char*p=c->json;
    if(*p=='-') p++;
    if(*p=='0') p++;
    else
    {
        if(!ISDIGIT09(*p)) return LEPT_PARSE_INVALID_VALUE;
        for (p++;ISDIGIT(*p); p++);
    }
    if(*p=='.')
    {
        p++;
        if(!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
        for (p++;ISDIGIT(*p); p++);        
    }
    if(*p=='E'||*p=='e')
    {
        p++;
        if(*p=='+'||*p=='-') p++;
        if(!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
        for (p++;ISDIGIT(*p); p++);        
    }
    
    v->n=strtod(c->json,NULL);
    v->type=LEPT_NUMBER;
    c->json=p;
    return LEPT_PARSE_OK;
}

void lept_free(lept_value* v)
{
    // assert(v!=NULL);
    // if(v->type==LEPT_STRING)
    //     free(v->string.s);
    // lept_init(v);
    assert(v!=NULL);
    switch (v->type)
    {
    case LEPT_STRING:
        free(v->string.s);
        break;
    case LEPT_ARRAY: 
        for (size_t i = 0; i < v->array.size; i++)
        {
            lept_free(&v->array.e[i]);
        }
        free(v->array.e);
        break;
    case LEPT_OBJECT: 
        for (size_t i = 0; i < v->object.size; i++)
        {
            free(v->object.m[i].k);
            lept_free(&v->object.m[i].v);
        }
        free(v->object.m);
        break;
    default:
        break;
    }
}
void lept_set_string(lept_value* v,const char*s,size_t len)
{
    assert((v!=NULL)&&(s!=NULL || len==0));
    lept_free(v);
    v->string.s=(char*)malloc(len+1);
    memcpy(v->string.s,s,len);
    v->string.s[len]='\0';
    v->string.len=len;
    v->type=LEPT_STRING;
}

void lept_set_number(lept_value* v,double n)
{
    assert(v!=NULL);
    lept_free(v);
    v->n=n;
    v->type=LEPT_NUMBER;
}

const char* lept_get_string(const lept_value* v)
{
    assert(v!=NULL && v->type==LEPT_STRING);
    return v->string.s;
}

size_t lept_get_string_length(const lept_value* v)
{
    assert(v!=NULL && v->type==LEPT_STRING);
    return v->string.len;
}

int lept_get_boolean(const lept_value* v)
{
    assert(v!=NULL && (v->type==LEPT_FALSE)||v->type==LEPT_TRUE);
    if(v->type==LEPT_FALSE)
        return 0;
    else
        return 1;
}

void lept_set_boolean(lept_value* v,int b)
{
    assert(v!=NULL);
    lept_free(v);
    if(b)
    {
        v->type=LEPT_TRUE;
    }
    else
    {
        v->type=LEPT_FALSE;
    }
    
}

#ifndef LEPT_PARSE_STACK_SIZE
#define LEPT_PARSE_STACK_SIZE 256
#endif



static void* lept_contex_push(lept_context* c,size_t size)
{
    void* ret;
    if(size+c->top>=c->size)
    {
        if(c->size==0)
            c->size=LEPT_PARSE_STACK_SIZE;
        while(c->top+size>=c->size)
        {
            c->size+=c->size>>1;
        }
        c->stack=(char*)realloc(c->stack,c->size);
        
    }
    ret=c->stack+c->top;
    c->top+=size;
    return ret;
}

static void* lept_contex_pop(lept_context* c,size_t size)
{
    assert(c->top>=size);
    c->top-=size;
    return c->stack+c->top;
}

#define PUTC(c,ch) do {*(char*)lept_contex_push(c,sizeof(char))=(ch);} while(0)

static void lept_encode_utf8(lept_context* c,unsigned u)
{
    if(u<0x007F)
        PUTC(c,u& 0xFF);
    else if (u<0x07FF)
    {
        // PUTC(c, 0x u>>6)
    }
    
}
static const char* lept_parse_hex4(const char* p,unsigned* u)
{
    *u=0;
    for (size_t i = 0; i < 4; i++)
    {
        *u<<=4;
        char ch=*p++;
        if(ch>='0'&&ch<='9') { *u|=ch-'0';}
        else if(ch>='A'&&ch<='F') {*u|=ch-'A'+10;}
        else if(ch>='a'&&ch<='f') {*u|=ch='a'+10; }
        else return NULL;
    }
    return p;
}

static int parse_string_raw(lept_context*c,char** str,size_t* len)
{
    size_t head=c->top;
    EXPECT(c,'\"');
    const char* p=c->json;
    for (;;)
    {
        char ch=*p++;
        switch (ch)
        {
        case '\"':
            len=c->top-head;
            *str=(const char*)lept_contex_pop(c,len);
            c->json=p;
            return LEPT_PARSE_OK;
        case '\0':
            c->top=head;
            return LEPT_PARSE_MISS_QUOTATION_MARK;
        case '\\':
            switch (*p++)
            {
            case '\\': PUTC(c,'\\'); break;
            case '\"':  PUTC(c,'\"'); break;
            case '/': PUTC(c,'/'); break;
            case 'b': PUTC(c,'b'); break;
            case 'f': PUTC(c,'f'); break;
            case 'n': PUTC(c,'n'); break;
            case 'r': PUTC(c,'r'); break;
            case 't': PUTC(c,'t'); break;
            default:
                c->top=head;
                return LEPT_PARSE_INVALID_STRING_ESCAPE;
            } 
        default:
                if((unsigned char)ch<=0x20)
                {
                    c->top=head;
                    return LEPT_PARSE_INVALID_STRING_ESCAPE;
                }
                PUTC(c,ch);
            
        }
    }    
}

static int lept_parse_string(lept_context* c,lept_value* v)
{
    int ret;
    char* s;
    size_t len;
    if((ret=parse_string_raw(c,&s,&len))==LEPT_PARSE_OK)
        lept_set_string(v,s,len);
    return ret;
}

size_t lept_get_array_size(const lept_value* v)
{
    assert(v!=NULL && v->type==LEPT_ARRAY);
    return v->array.size;
}

lept_value* lept_get_array_element(const lept_value* v,size_t index)
{
    assert(v!=NULL && v->type==LEPT_ARRAY);
    assert(index<v->array.size && index >=0);
    return &(v->array.e[index]);
}

static int lept_parse_array(lept_context* c,lept_value* v)
{
    size_t size=0;
    EXPECT(c,'[');
    lept_parse_whitespace(c);
    int ret;
    if(*c->json==']')
    {
        c->json++;
        v->type=LEPT_ARRAY;
        v->array.e=NULL;
        v->array.size=0;
        return LEPT_PARSE_OK;
    }
    for (;;)
    {
        lept_value e;
        lept_init(&e);
        if(ret=lept_parse_value(c,&e)!=LEPT_PARSE_OK)
            return ret;
        memcpy(lept_contex_push(c,sizeof(lept_value)),&e,sizeof(lept_value));
        size++;
        lept_parse_whitespace(c);
        if(*c->json==',')
        {
            c->json++;
            lept_parse_whitespace(c);            
        }
        else if (*c->json==']')
        {
            c->json++;
            v->type=LEPT_ARRAY;
            v->array.size=size;
            size*=sizeof(lept_value);
            v->array.e=(lept_value*)malloc(size);
            memcpy(v->array.e,lept_contex_pop(c,size),size);
            return LEPT_PARSE_OK;
        }
        else
            return LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET;
        
    }
    for (size_t i = 0; i < size; i++)
        lept_free((lept_value*)lept_context_pop(c, sizeof(lept_value)));
    return ret;
}

static int lept_parse_object(lept_context* c,lept_value* v)
{
    size_t size;
    lept_member m;
    int ret;
    m.k=NULL;
    m.klen=0;
    EXPECT(c,'{');
    lept_parse_whitespace(c);
    if(*c->json=='}')
    {
        c->json++;
        v->type=LEPT_OBJECT;
        v->object.m=NULL;
        v->object.size=0;
        return LEPT_PARSE_OK;
    }
    for(;;)
    {
        lept_init(&m.v);
        char* str;
        size_t len;
        if(*c->json!='"')
        {
            ret=LEPT_PARSE_MISS_KEY;
            break;
        }
        if((ret=parse_string_raw(c,&str,&len))!=LEPT_PARSE_OK)
            break;
        m.k=(char *)malloc(len+1);
        m.klen=len;
        memcpy(m.k,str,len);
        m.k[len]='\0';
        lept_parse_whitespace(c);
        if(*c->json!=':')
        {
            ret=LEPT_PARSE_MISS_COLON;
            break;
        }
        c->json++;
        lept_parse_whitespace(c);
        if((ret=lept_parse_value(c,&m.v))!=LEPT_PARSE_OK)
            break;
        memcpy(lept_contex_push(c,sizeof(lept_member)),&m,sizeof(lept_member));
        size++;
        m.k=NULL;
        m.klen=0;
        lept_parse_whitespace(c);
        if(*c->json==',')
        {
            c->json++;
            lept_parse_whitespace(c);
        }
        else if (*c->json=='}')
        {
            size_t s=sizeof(lept_member)*size;
            v->object.m=malloc(s);
            memcpy(v->object.m,lept_contex_pop(c,s),s);
            v->object.size=size;
            return LEPT_PARSE_OK;
        }
        else
        {
            
        }
        
        free(m.k);
        for (size_t i = 0; i < size; i++)
        {
            lept_member* m=lept_contex_pop(c,sizeof(lept_member));
            free(m->k);
            lept_free(&m->v);
        }
        
    }
    v->type=LEPT_NULL;
    return ret;
}