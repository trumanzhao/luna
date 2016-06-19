#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <locale>
#include <cstdint>
#include "luna.h"

int func_a(int a, int b, int c)
{
	return a + b + c;
}

void func_b()
{
	puts("func_b");
}

struct base_t
{
	int m_id = 1;
public:
	virtual void eat()
	{
		puts("base_t eat");
	}
};

struct my_object_t : base_t
{
	int m_id = 2;
public:

	virtual void eat()
	{
		printf("obj eat, id=%d\n", m_id);
	}

	int m_x = 123;
	void print(int a, int b)
	{
		printf("a+b=%d,m_x=%d\n", a + b, m_x);
	}

	int add(int a, int b)
	{
		return a + b;
	}

	DECLARE_LUA_CLASS(my_object_t);
};

EXPORT_CLASS_BEGIN(my_object_t)
EXPORT_LUA_FUNCTION(print)
EXPORT_LUA_FUNCTION(add)
EXPORT_CLASS_END()

void call_some_func(lua_State* L)
{
	lua_guard_t g(L);

	const char* msg = nullptr;
	int len = 0;

	if (lua_call_file_function(L, "test.lua", "some_func", std::tie(msg, len), "hello", "world"))
		printf("msg=%s, len=%d\n", msg, len);
}

int main(int argc, char* argv[])
{
	lua_State* L = luaL_newstate();

	luaL_openlibs(L);

	lua_setup_env(L);

	lua_register_function(L, "fuck_a", func_a);
	lua_register_function(L, "fuck_b", func_b);

	call_some_func(L);

	lua_close(L);
	return 0;
}
