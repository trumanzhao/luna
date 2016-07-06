# 关于`luna`

## 简介
早些时候基于C\+\+14的特性弄了个lua的C\+\+绑定库.  
这并不是一个完美的实现,后文有介绍其缺陷,但一般应该没啥影响.  
如果不求完美的话,这个绑定库用起来应该还是会非常方便的.  
限于公司编译环境的问题,并没有在公司项目中使用,就扔在仓库中开源了.  
仓库: [luna](https://github.com/trumanzhao/luna)

## 用法


直接将`luna.h`, `luna.cpp`两个文件拷贝添加到你自己的工程中去即可.  
注意,你的编译器必须支持C++14.  
我自己测试过的环境如下(都是64位编译,32位没测试过):

- Windows: Visual studio 2015
- Mac OS X: Apple (GCC) LLVM 7.0.0, 注意在编译参数中加入选项`-std=c++1y`
- GCC 5.3: 当然也要加`-std=c++1y`


## 创建luna运行时环境:

创建lua\_State后,再调用 `lua_setup_env(L)`即可完成初始化.  
该函数会创建一个内部数据结构,将其作为userdata存放到lus\_State中去.  
在lua\_State被close时,会通过userdata的gc方法自动释放.  

``` c++
lua_State* L = luaL_newstate();
luaL_openlibs(L);
lua_setup_env(L);
```

有必要的话,还可以设置以下回调函数(比如你可能需要从自己的打包文件中读取文件):  

`lua_set_error_func`: 错误回调函数,可以用来写日志之内的,默认调用puts.  
`lua_set_file_time_func`: 获取文件的最后修改时间.  
`lua_set_file_size_func`: 获取文件的大小.  
`lua_set_file_data_func`: 一次性读取文件数据.  

## 文件的热更新

luna为每个文件创建了一个独立的环境沙盒,可以为每个文件单独热更新.  
运行中,可以调用`lua_reload_update`检测文件变更(按文件时间)并重新加载.  
当然,脚本中可以自己控制运行时数据在热更新时怎么处理.

## 导出函数

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
class my_export_class_t
{
	// ... other code ...
	int func_a(const char* a, int b);
	int func_b(some_class_t* a, int b);
    char m_name[32];
  	// 插入这句:
	DECLARE_LUA_CLASS(my_export_class_t);
};
```

在cpp中增加导出表的定义:

``` c++
EXPORT_CLASS_BEGIN(my_export_class_t)
EXPORT_LUA_FUNCTION(func_a)
EXPORT_LUA_FUNCTION(func_b)
EXPORT_LUA_STRING(m_name)
EXPORT_CLASS_END()
```

可以用带`_AS`的导出宏指定导出的名字,用带`_R`的宏指定导出为只读变量.  
比如: `EXPORT_LUA_STRING_AS(m_name, Name)`

***注意不要对导出对象memset***


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

## 关于对象生存期问题

目前的实现是以C\+\+为主导来控制对象生存期的.  
C\+\+对象删除时,会自动解除对lua影子对象的引用.  
当然,也可以方便的改为以lua为主导,只要在类元表中加入`__gc`方法即可,但会给C\+\+对象的管理上带来一些新的束缚.  
另外,已经push到lua中的对象,如果需要在lua\_State关闭后才析构,那么可以先调用`lua_clear_ref(object_pointer)`解除引用.


## C\+\+中调用lua函数

比如我们要调用脚本test.lua中的一个函数,如下说示:

``` lua
function some_func(a, b)
  	return a + b, a - b;
end
```

上面的lua函数返回两个值,那么,可以在C++中这样调用:

```cpp
int x, y;
lua_guard_t g(L);
// x,y用于接收返回值
lua_call_file_function(L, "test.lua", "some_func", std::tie(x, y), 11, 2);
```

注意上面这里的lua_guard_t,请注意它的生存期,它实际上做的事情是:

1. 在构造是调用`lua_gettop`保存栈.
2. 析构时调用`lua_settop`恢复栈.


如果lua函数无返回值,也无参数,那么会稍简单点:

```cpp
lua_call_file_function(L, "test.lua", "some_func");
```

下面是lua函数无返回, 有a,b,c三个参数的情况:

```cpp
lua_call_file_function(L, "test.lua", "some_func", std::tie(), a, b, c);
```

上面都是以调用指定文件中的函数为例的,实际中还有另外两种常见的情况:

- 调用导出对象上的函数: `lua_call_object_function`
- 调用指定全局表中的函数: `lua_call_table_function`
- 调用全局环境中的函数: `lua_call_global_function`

## 沙盒环境

每个文件具有独立的环境,文件之间通过`import`相互访问,如:

```lua
local m = import("some_one.lua");
m.some_function();
```

实际上,import返回目标文件的环境表.  
如果目标文件未被加载,则会自动加载.  
已经加载过的文件不会重复加载,而是共享同一实例.  


## 可能的问题

1. DECLAREi\_LUA\_CLASS宏在对象中插入了成员,在某些情况下可能是不能接受的.
2. 如果类之间有继承关系,同时导出基类和子类时,要小心操作指针,比如用基类指针push子类对象,结果可能就是非预期的.

对于1,可以采用将对象与lua table的对应关系另外存一张表的方式解决,当然这会产生一些消耗.这个额外的对应表可以是C++层面维护,也可以在lua中维护,但是还得另外处理C++对象析构时清除引用的问题.  

对于2,问题的根本原因是C++对象的子类指针和其基类指针可能是不一样的,而luna在lua影子对象中存储了对象的指针.之前的版本使用了虚函数来取得导出表之类的元数据,但是并不能完全解决问题,所以在新版中去掉了虚函数.使用者需要了解这一点,以在实践中有针对性的规避这一问题了.
