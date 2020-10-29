# luna工具库

# 概述

luna库提供了几个lua开发的常见辅助功能:
- lua/c++绑定
- lua序列化与反序列化
- 变长整数编码,用于lua序列化,当然也可以方便的用于其他场合

这里把代码编译成了动态库,由于代码非常简单,实际使用时也可以简单的复制文件到自己的工程.  
lua\_archiver引用了lz4库用于数据压缩(lz4.h+lz4.c).

之所以实现这个lua/c\+\+绑定,是出于以下的想法:
- 希望所有事情在c\+\+代码中就搞定,不希望额外再运行一个什么转换处理工具
- 希望能够方便的导出一般C\+\+函数,而不必写一大堆lua api调用代码
- 希望能简单的处理导出对象的生命期
- 希望能方便的在lua代码中对导出对象进行扩展,重载等
- 希望使用尽可能简单,无需对luna库本身做任何初始化
- 希望执行时无副作用,即没有全局或静态的数据,进程中存在多个lua\_State时不会相互干扰

## 编译说明

默认基于C\+\+17标准实现,最低要求为C\+\+11.  
需要C\+\+17支持的代码主要是luna.h,如果编译器不支持C\+\+17,可以将其替换为C\+\+11版本的luna11.h.  
在makefile中有可能需要调整的选项一是C\+\+标准(-std=c++17/c++11),另一个是lua的库文件查找路径.   

另一个更简单的使用方法是将源文件直接复制到你的工程中使用.   

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
class my_class final {
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
LUA_EXPORT_CLASS_BEGIN(my_class)
LUA_EXPORT_METHOD(func_a)
LUA_EXPORT_METHOD(func_b)
LUA_EXPORT_PROPERTY(m_name)
LUA_EXPORT_CLASS_END()
```

可以用带`_AS`的导出宏指定导出的名字,用带`_READONLY`的宏指定导出为只读变量.
比如: `LUA_EXPORT_PROPERTY_READONLY_AS(m_name, Name)`  
对于成员函数,导出时指定`READONLY`是指禁止在lua中覆盖这个导出方法.  


## 关于导出类(对象)的注意点

### 目前通过静态断言作了限制: 只能导出声明为final的类

这是为了避免无意间在父类和子类做出错误的指针转换
如果需要父类子类同时导出且保证不会出现这种错误,可以自行去掉这个断言

### 关于C++导出对象的生存期问题

注意,C++对象一旦被push进入lua,其生命期就交给lua的gc管理了,C++层面不能随便删除.
这些lua托管的对象在gc时,会默认调用delete,如果不希望调用delete,可以在对象中实现自定义gc方法: `void __gc()`.  
另外,由于lua的gc回收资源总是具有一定延迟的,所以如果C++对象持有较多的资源的话,最好显示释放资源或者在lua层面显示的调用gc.   
对于已经push到lua的对象,如果想从C++解除引用,可以调用`lua_detach(L, object)`;   

``` c++
struct player final {
    // 通过自定义__gc函数,可以自行管理对象生命期,而不是自动被gc删除
    // void __gc() { puts("__gc"); }
    DECLARE_LUA_CLASS(player);
    std::string m_name = "some-player";
};

LUA_EXPORT_CLASS_BEGIN(player)
LUA_EXPORT_PROPERTY(m_name)
LUA_EXPORT_CLASS_END()

void some_event(lua_State* L) {
    player* p = new player();
    // 在对象p被传参push到lua后,p的生命期默认就托管给lua的gc管理了
    lua_call_global_function(L, nullptr, "new_player", std::tie(), p);
    lua_gc(L, LUA_GCCOLLECT, 0);
    // 下面的写法是错误的,因为对象p可能已经在gc中被回收(delete)了
    // 如果真需要这么写,可以自定义__gc函数,或者在gc前detach
    p->m_name = "abc";
}
```

## lua中访问导出对象

lua代码中直接访问导出对象的成员/方法即可.

``` lua
--当然,你得在C++中实现并导出这个get_obj_from_cpp函数
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
lua_call_table_function(L, nullptr, "s2s", "some_func", std::tie(x, y, name), 11, 2);
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

## 性能上的建议

从lua调用导出对象C\+\+成员函数时,每次`object.some_function`都会触发一次元表查询并产生一个闭包.  
如果代码对此比较敏感,建议将这个返回的闭包保存起来,如`local my_function=object.some_function`.  
当然,也可以这样写`object.some_function=object.some_function`.  
   
lua序列化数据在反序列化(load)时,处于性能考虑,需要用到数据中记录的数组及哈希长度,为了安全起见,建议对此长度做一定限制(set_max_array_reserve/set_max_hash_reserve),他们分别表示一次反序列化(load)过程中可以创建的数组(哈希)长度总和,设为-1时表示不予限制(完全信任数据).











