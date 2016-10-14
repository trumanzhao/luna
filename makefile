product = luna
# execute, dynamic_shared, static_shared
target_type = execute
define_macros =
include_dir = . ./lua
# 依赖库列表,空格分开
lib =
# 编译期临时文件目录
build_dir = ./build
# 最终产品目录:
# 注意,只是对可执行文件和动态库而言,静态库忽略此项
target_dir = ./bin
#src_root = ../src
src_root = .
# 依赖库目录,多个目录用空格分开:
lib_dir =
# 本工程(如果)输出.a,.so文件的目录
lib_out =

CC = gcc
CXX = g++
CFLAGS = -m64
CXXFLAGS = $(CFLAGS) -Wno-invalid-offsetof -Wno-deprecated-declarations -std=c++1y

#----------------- 下面部分通常不用改 --------------------------

ifeq ($(target_type), execute)
linker = g++
link_flags = -Wl,-rpath ./
endif

ifeq ($(target_type), dynamic_shared)
link_flags = -shared -ldl -fPIC -lpthread
after_link = cp -f $@ $(target_dir)
endif

ifeq ($(target_type), static_shared)
link_flags =
endif

ifeq ($(target_type), execute)
target = $(target_dir)/$(product)
endif

ifeq ($(target_type), dynamic_shared)
target  = $(lib_out)/lib$(product).so
endif

ifeq ($(target_type), static_shared)
target  = $(lib_out)/lib$(product).a
endif

# exe and .so
ifneq ($(target_type), static_shared)
link = g++ -o $@ $^ $(link_flags) -m64 $(lib_dir:%=-L%) $(lib:%=-l%)
endif

# .a
ifeq ($(target_type), static_shared)
link = ar cr $@ $^ $(link_flags)
endif

the_goal = debug
ifneq ($(MAKECMDGOALS),)
the_goal = $(MAKECMDGOALS)
endif

do_file=no

ifeq ($(the_goal),debug)
do_file=yes
CFLAGS += -g
define_macros += _DEBUG
endif

ifeq ($(the_goal),release)
do_file=yes
CFLAGS += -O3
endif

ifeq ($(do_file),yes)
root_src_c = $(shell find $(src_root) -type f -name '*.c')
root_src_cpp = $(shell find $(src_root) -type f -name '*.cpp')
src_c = $(root_src_c:$(src_root)/%=%)
src_cpp = $(root_src_cpp:$(src_root)/%=%)
obj_list = $(addsuffix .o, $(src_c)) $(addsuffix .o, $(src_cpp))
env_param = $(include_dir:%=-I%) $(define_macros:%=-D%)
endif

ifeq ($(do_file),yes)
my_obj_list = $(obj_list:%=$(build_dir)/%)
$(foreach obj, $(my_obj_list), $(shell mkdir -p $(dir $(obj))))
ifeq ($(lib_out),)
$(shell mkdir -p $(lib_out))
endif
$(shell mkdir -p $(target_dir))
endif

comp_c_echo = @echo gcc $< ...
comp_cxx_echo = @echo g++ $< ...

.PHONY: debug
debug: build_prompt $(target)

.PHONY: release
release: build_prompt $(target)

.PHONY: clean
clean:
	rm -f $(target)
	rm -rf $(build_dir)

.PHONY: build_prompt
build_prompt:
	@echo build $(product) $(the_goal) ...
	@echo cflags=$(CFLAGS) ...
	@echo c++flags=$(CXXFLAGS) ...
	@echo includes=$(include_dir)
	@echo defines=$(define_macros)
	@echo lib_dir=$(lib_dir)
	@echo libs=$(lib)

$(target): $(my_obj_list)
	@echo link "-->" $@
	@$(link)
	$(after_link)

$(build_dir)/%.c.o: $(src_root)/%.c
	$(comp_c_echo)
	@$(CC) $(CFLAGS) $(env_param) -c -o $@ $<

$(build_dir)/%.cpp.o: $(src_root)/%.cpp
	$(comp_cxx_echo)
	@$(CXX) $(CXXFLAGS) $(env_param) -c -o $@ $<
