#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <locale>
#include <cstdint>
#include "lua_adpter.h"

int func_a(int a, int b, int c)
{
    return a + b + c;
}

void func_b()
{
    puts("func_b");
}

struct base 
{
    int m_id = 1;
public:
    virtual void eat()
    {
        puts("base eat");
    }
};

struct my_object : base
{
    int m_id = 2;
public:

    virtual void eat()
    {
        printf("obj eat, id=%d\n", m_id);
    }

	int m_x = 123;
	void Print(int a, int b)
	{
		printf("a+b=%d,m_x=%d\n", a + b, m_x);
	}

	int Add(int a, int b)
	{
		return a + b;
	}

	int MyFunc(lua_State* L)
	{
		printf("%p\n", L);
		return 0;
	}

    int Copy(my_object* object, int x)
    {
        printf("copy: %p-->%p, x=%d\n", object, this, x);
        this->m_x = x;
        return x + 1;
    }

    DECLARE_LUA_CLASS(my_object);
};

EXPORT_CLASS_BEGIN(my_object)
EXPORT_LUA_FUNCTION(Print)
EXPORT_LUA_FUNCTION(Copy)
EXPORT_CLASS_END()

void Fuck(lua_State* L)
{
    lua_load_script(L, "test.lua");


	lua_function_object func(L);
	int x, y;

	if (func.get_file_func("test.lua", "f0"))
	{
		func.call();
	}

	if (func.get_file_func("test.lua", "f1"))
	{
		func.call(&x, 1, 2);
	}

	if (func.get_file_func("test.lua", "f2"))
	{
		func.call(std::tie(x, y), 1, 2);
	}
}

template <typename T>
typename std::enable_if<is_tuple<T>::value, void>::type func(T& t)
{
	printf("is_tuple: %zu\n", sizeof(t));
}

template <typename T>
typename std::enable_if<!is_tuple<T>::value, void>::type func(T& t)
{
	printf("is_int: %zu\n", sizeof(t));
}

int main(int argc, char* argv[])
{
    lua_State* L = lua_create_vm();

	bool b = is_tuple<int>::type();

	int a;
	std::tuple<int, bool> t{1, false};

	func(a);
	func(t);

    lua_register_function(L, "fuck_a", func_a);
    lua_register_function(L, "fuck_b", func_b);

	Fuck(L);

    lua_delete_vm(L);
	return 0;
}

