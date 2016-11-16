# 关于`luna`

## 这是个什么东西?

luna的目的,早期是一个lua的C++绑定工具,现在的目的是实现一个机遇lua的集群服务器框架.
主要特性:

- 方便进行任意拓扑结构集群的搭建.
- 方便的在集群进程间进行RPC调用.
- 方便的实现主要的集中消息转发方式如: 点对点,哈希,广播,主从.
- 方便的进行lua与C++交互.
- 方便的使用脚本热加载,以实现高效开发&运维.

## 环境

luna同时支持Windows, Linux, macOS三平台,但是,你的编译器必须支持C++14.
我的开发环境如下(目前只支持64位,32位未经测试):

- Windows: Visual studio 2015
- macOS: Apple (GCC) LLVM 7.0.0
- Linux: GCC 5.3

## 程序的启动&运行

- 程序启动后,会固定加载(require)base/base.lua,改文件是个基础模块,一般无需改动.
- 加载base.lua后,程序会注册一些基础函数,然后调用base.lua中的全局函数: luna_entry
- luna_entry中会首先加载(import)用户指定(命令行)的入口文件, 然后循环调用其中的on_loop函数.

## 文件沙盒及文件热加载

在luna中,用户主要通过全局的import函数加载lua文件.
- import为每个文件创建了一个沙盒环境,这使得各个文件中的变量名不会冲突.
- 在需要的地方,用户还可以继续使用require.
- 多次import同一个文件,实际上只会加重一次,import返回的是文件的环境表.
- 通过import加载的文件会定期的检查文件时间戳,如发生修改,则会自动热加载.

## C++导出函数

当函数的参数以及返回值是***基本类型***或者是***已导出类***时,可以直接用`lua_register_function`导出:

``` c++
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

``` c++
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

注意,对象一旦被push进入lua,那么C++层面就不能随便删除它了.
绑定到lua中的C++对象,将会在lua影子对象gc时,自动delete.
如果这个对象在gc时不能调用delete,请在对象中实现gc方法: void gc();
这时gc将调用该函数,而不会直接delete对象.


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
  	return a + b, a - b;
end
```

上面的lua函数返回两个值,那么,可以在C++中这样调用:

```cpp
int x, y;
lua_guard_t g(L);
// x,y用于接收返回值
lua_call_table_function(L, "s2s", "test.lua", "some_func", std::tie(x, y), 11, 2);
```

注意上面这里的lua_guard_t,所有从C++调用Lua的地方,都需要通过它来做栈保护.
请注意它的生存期,它实际上做的事情是:

1. 在构造是调用`lua_gettop`保存栈.
2. 析构时调用`lua_settop`恢复栈.


如果lua函数无返回值,也无参数,那么会比较简单:

```cpp
lua_guard_t g(L);
lua_call_table_function(L, "s2s", "some_func");
```

下面是lua函数无返回, 有a,b,c三个参数的情况:

```cpp
lua_guard_t g(L);
int a;
char* b;
std::string c;
lua_call_table_function(L, "s2s", "some_func", std::tie(), a, b, c);
```


## 网络
TODO:

## 路由转发
TODO:




