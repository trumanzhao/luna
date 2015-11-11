# lua_adpter简介

## 怎么加到工程中去

直接将lua_adpter.h, lua_adpter.cpp两个文件加到工程中去即可.

值得注意的是,你得编译器必须支持C++11.

我自己测试过的环境如下(都是64位编译,32位没测试过):

Windows: Visual studio 2013

Mac OS X: Apple LLVM 7.0.0

Linux: gcc 4.8.2

注意,代码中使用了C++标准中未定义行为的写法,大概这样子:

``` c++
void SomeFunc(int a, int b, in c)
{
	printf("a=%d, b=%d, c=%d\n", a, b, c);
}
int i = 0;
SomeFunc(i++, i++, i++);
```

上述代码中,a,b,c三个参数的求值顺序是未定义的.

所以,在lua_adpter的实现中,对不同的编译器写了不同的代码,但不同版本编译器任然可能有差异.

在Mac下编译可能会报警告: multiple unsequenced modifications.

之所以非得这样写,是因为要利用模板参数包的展开来计算参数,如果你又更好的写法,记得告诉我.

## 创建lua_adpter运行时环境:

创建lua_State以及加载文件,删除lua_State:

``` c++
lua_State* L = lua_create_vm();

lua_load_script(L, "test.lua");

lua_delete_vm(L);
```

注意,有必要的话,可以设置以下即可回调函数:

lua_set_error_func: 错误回调函数,可以用来写日志之内的,默认调用puts.

lua_set_file_time_func: 获取文件的最后修改时间.

lua_set_file_size_func: 获取文件的大小.

lua_set_file_data_func: 一次性读取文件数据.

运行中可以调用la_reload_update重新加载已变更的文件.

## 导出函数

当函数的参数以及返回值是基本类型或者是导出类时,可以直接用lua_register_function导出:

``` c++
int func_a(const char* a, int b);
int func_b(some_export_class* a, int b);
some_export_class* func_c(float x);

lua_register_function(L, func_a);
lua_register_function(L, func_b);
lua_register_function(L, func_c);
```

当然,你也可以导出lua标准的c函数.

## 导出类

首先需要在你得类声明中插入导出声明:

``` c++
class my_class
{
	// ... other code ...
	int func_a(const char* a, int b);
	int func_b(some_export_class* a, int b);
  	// 插入这句:
	DECLARE_LUA_CLASS(my_class);

  	char m_name[32];
};
```

在cpp中加入导出代码:

``` c++
EXPORT_CLASS_BEGIN(my_class)
EXPORT_LUA_FUNCTION(func_a)
EXPORT_LUA_FUNCTION(func_b)
EXPORT_LUA_STRING(m_name)
EXPORT_CLASS_END()
```

注意导出的成员变量如果有"m_"前缀的话会自动去除,也可以用带"__AS"的导出宏指定导出名,用带"__R"宏指定导出为只读变量.

-   不要对导出对象memset.
-   已导出的对象,应该在关闭虚拟机之前析构,或者提前调用lua_clear_ref解除引用.

## lua中访问导出对象
lua代码中直接访问导出对象的成员/方法即可.

```lua
local obj = get_obj_from_cpp();
obj.func("abc", 123);
obj.name = "new name";
```
另外,也可以在lua中重载(覆盖)导出方法.


