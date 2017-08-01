# luna工具库

# 概述

luna库提供了几个lua开发的常见辅助功能:
- lua/c++绑定
- lua序列化与反序列化
- 变长整数编码,用于lua序列化,当然也可以方便的用于其他场合

这里把代码编译成了动态库,由于代码非常简单,实际使用时也可以简单的复制文件到自己的工程.  
lua\_archiver引用了lz4库用于数据压缩(lz4.h+lz4.c).

lua/c++绑定库(luna.h, luna.cpp)支持Windows, Linux, macOS三平台,默认的luna.h实现需C\+\+14支持.  
如果编译器不支持C\+\+14,也可以将其替换为C\+\+11版本的luna11.h.  
之所以实现这个lua/c\+\+绑定,是出于以下的想法:
- 希望所有事情在c\+\+代码中就搞定,不希望额外再运行一个什么转换处理工具
- 希望能够方便的导出一般C\+\+函数,而不必写一大堆lua api调用代码
- 希望能简单的处理导出对象的生命期
- 希望能方便的在lua代码中对导出对象进行扩展,重载等
- 希望使用尽可能简单,无需对luna库本身做任何初始化
- 希望执行时无副作用,即没有全局或静态的数据,进程中存在多个lua\_State时不会相互干扰

## C\+\+导出全局函数

当函数的参数以及返回值是***基本类型***或者是***已导出类***时,可以直接用`lua_register_function`导出:

``` cpp
int func_a(const char* a, int b);
int func_b(my_export_class_t* a, int b);
some_export_class* func_c(float x);

lua_register_function(L, func_a);
lua_register_function(L, func_b);
lua_register_function(L, func_c);
```

当然,你也可以导出lua标准的C函数.

## 导出类

首先需要在你得类声明中插入导出声明:

```cpp
class my_class final
{
    // ... other code ...
    int func_a(const char* a, int b);
    int func_b(some_class_t* a, int b);
    char m_name[32];
public:
    // 插入导出声明:
    DECLARE_LUA_CLASS(my_class);
};
```

在cpp中增加导出表的实现:

``` c++
EXPORT_CLASS_BEGIN(my_class)
EXPORT_LUA_FUNCTION(func_a)
EXPORT_LUA_FUNCTION(func_b)
EXPORT_LUA_STRING(m_name)
EXPORT_CLASS_END()
```

可以用带`_AS`的导出宏指定导出的名字,用带`_R`的宏指定导出为只读变量.
比如: `EXPORT_LUA_STRING_AS(m_name, Name)`

## 关于导出类(对象)的注意点

### 目前通过静态断言作了限制: 只能导出声明为final的类

这是为了避免无意间在父类和子类做出错误的指针转换
如果需要父类子类同时导出且保证不会出现这种错误,可以自行去掉这个断言

### 关于C++导出对象的生存期问题

注意,C++对象一旦被push进入lua,其生命期就交给lua的gc管理了,C++层面不能随便删除.
这些lua托管的对象在gc时,会默认调用delete,如果不希望调用delete,可以在对象中实现自定义gc方法: `void __gc()`.  
另外,由于lua的gc回收资源总是具有一定延迟的,所以如果C++对象持有较多的资源的话,最好显示释放资源或者在lua层面显示的调用gc.   
对于已经push到lua的对象,如果想从C++解除引用,可以调用`lua_unref_object(L, object)`;   

``` c++
class my_class final
{
    // ...
public:
    DECLARE_LUA_CLASS(my_class);	
    void __gc()
    {
        // lua gc时,如果存在本函数,那么会调用本函数取代默认的delete
    }
};
```

## lua中访问导出对象

lua代码中直接访问导出对象的成员/方法即可.

``` lua
local obj = get_obj_from_cpp();
obj.func("abc", 123);
obj.name = "new name";
```

另外,C\+\+对象导出到lua中是通过一个table来实现的,可以称之为影子对象.
不但可以在lua中访问C\+\+对象成员,还可以有下面这些常见用法:

- 重载(覆盖)对象上的C\+\+导出方法.
- 在影子对象上增加额外的成员变量和方法.
- 在C\+\+中调用lua中为对象增加的方法,参见`lua_call_object_function`.

## C\+\+中调用lua函数

目前提供了两种支持:

- C++调用全局函数
- C++调用全局table中的函数
- C++调用导出对象上附加的函数.

下面以调用全局table中的函数为例:

``` lua
function s2s.some_func(a, b)
    return a + b, a - b, "tom";
end
```

上面的lua函数返回三个值,那么,可以在C++中这样调用:

```cpp
lua_guard g(L); //用它来做栈保护
int x, y;
const char* name = nullptr; 
// 小心,如果用char*做字符串返回值的话,确保name变量不要在lua_guard g的作用域之外使用
lua_call_table_function(L, nullptr, "s2s", "test.lua", "some_func", std::tie(x, y, name), 11, 2);
```

注意上面的lua\_guard,它实际上做的事情是:

1. 在构造时调用`lua_gettop`保存栈.
2. 析构时调用`lua_settop`恢复栈.

```cpp
// 注意这里由于需要传入abc三个参数,所以需要写一个std::tie()表示没有返回参数
lua_call_table_function(L, nullptr, "s2s", "some_func", std::tie(), a, b, c);
```
如果没有参数,也没有返回值,那就是最简单的写法了:

```cpp
lua_call_table_function(L, nullptr, "s2s", "some_func");
```
