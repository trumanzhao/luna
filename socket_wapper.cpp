/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016-11-01, trumanzhao@foxmail.com
*/
#include "tools.h"
#include "var_int.h"
#include "socket_wapper.h"
#include "lua/lstate.h"
#include "lz4/lz4.h"

EXPORT_CLASS_BEGIN(lua_socket_mgr)
EXPORT_LUA_FUNCTION(wait)
EXPORT_LUA_FUNCTION(listen)
EXPORT_LUA_FUNCTION(connect)
EXPORT_LUA_FUNCTION(set_package_size)
EXPORT_LUA_FUNCTION(set_compress_size)
EXPORT_LUA_FUNCTION(route)
EXPORT_LUA_FUNCTION(master)
EXPORT_CLASS_END()

lua_socket_mgr::~lua_socket_mgr()
{
}

bool lua_socket_mgr::setup(lua_State* L, int max_fd)
{
    auto mgr = create_socket_mgr(max_fd);
    if (mgr != nullptr)
    {
        m_lvm = L;
        m_mgr = std::shared_ptr<socket_mgr>(mgr, [](auto o) { o->release(); });
        m_archiver = std::make_shared<lua_archiver>();
        m_ar_buffer = std::make_shared<io_buffer>();
        m_lz_buffer = std::make_shared<io_buffer>();
        m_router = std::make_shared<socket_router>(m_mgr);
        return true;
    }
    return false;
}

int lua_socket_mgr::listen(lua_State* L)
{
    const char* ip = lua_tostring(L, 1);
    int port = (int)lua_tointeger(L, 2);
    if (ip == nullptr || port <= 0)
    {
        lua_pushnil(L);
        lua_pushstring(L, "invalid param");
        return 2;
    }

    std::string err;
    int token = m_mgr->listen(err, ip, port);
    if (token == 0)
    {
        lua_pushnil(L);
        lua_pushstring(L, err.c_str());
        return 2;
    }

    auto listener = new lua_socket_node(token, m_lvm, m_mgr, m_archiver, m_ar_buffer, m_lz_buffer, m_router);
    lua_push_object(L, listener);
    lua_pushstring(L, "OK");
    return 2;
}

int lua_socket_mgr::connect(lua_State* L)
{
    const char* ip = lua_tostring(L, 1);
    const char* port = lua_tostring(L, 2);
    if (ip == nullptr || port == nullptr)
    {
        lua_pushnil(L);
        lua_pushstring(L, "invalid param");
        return 2;
    }

    std::string err;
    int token = m_mgr->connect(err, ip, port);
    if (token == 0)
    {
        lua_pushnil(L);
        lua_pushstring(L, err.c_str());
        return 2;
    }

    auto stream = new lua_socket_node(token, m_lvm, m_mgr, m_archiver, m_ar_buffer, m_lz_buffer, m_router);
    lua_push_object(L, stream);
    lua_pushstring(L, "OK");
    return 2;
}

void lua_socket_mgr::set_package_size(size_t size)
{
    if (size > 0)
    {
        m_ar_buffer->resize(size);
        m_lz_buffer->resize(size);
    }
}

int lua_socket_mgr::route(lua_State* L)
{
    uint32_t service_id = (uint32_t)lua_tointeger(L, 1);
    if (lua_isnil(L, 2))
    {
        m_router->erase(service_id);
    }
    else
    {
        uint32_t token = (uint32_t)lua_tointeger(L, 2);
        m_router->update(service_id, token);
    }
    return 0;
}

void lua_socket_mgr::master(uint8_t group_idx, uint32_t token)
{
    m_router->set_master(group_idx, token);
}

EXPORT_CLASS_BEGIN(lua_socket_node)
EXPORT_LUA_FUNCTION(call)
EXPORT_LUA_FUNCTION(forward_target)
EXPORT_LUA_FUNCTION_AS(forward_by_class<msg_id::forward_master>, "forward_master")
EXPORT_LUA_FUNCTION_AS(forward_by_class<msg_id::forward_random>, "forward_random")
EXPORT_LUA_FUNCTION_AS(forward_by_class<msg_id::forward_broadcast>, "forward_broadcast")
EXPORT_LUA_FUNCTION(forward_hash)
EXPORT_LUA_FUNCTION(close)
EXPORT_LUA_FUNCTION(set_send_cache)
EXPORT_LUA_FUNCTION(set_recv_cache)
EXPORT_LUA_FUNCTION(set_timeout)
EXPORT_LUA_STD_STR_AS_R(m_ip, "ip")
EXPORT_LUA_INT_AS_R(m_token, "token")
EXPORT_CLASS_END()

lua_socket_node::lua_socket_node(uint32_t token, lua_State* L, std::shared_ptr<socket_mgr>& mgr, std::shared_ptr<lua_archiver>& ar,
    std::shared_ptr<io_buffer>& ar_buffer, std::shared_ptr<io_buffer>& lz_buffer, std::shared_ptr<socket_router> router)
{
    m_token = token;
    m_lvm = L;
    m_mgr = mgr;
    m_archiver = ar;
    m_ar_buffer = ar_buffer;
    m_lz_buffer = lz_buffer;
    m_router = router;

    m_mgr->get_remote_ip(m_ip, m_token); // just valid for accepted stream

    m_mgr->set_accept_callback(token, [this](uint32_t steam_token)
    {
        auto stream = new lua_socket_node(steam_token, m_lvm, m_mgr, m_archiver, m_ar_buffer, m_lz_buffer, m_router);
        if (!lua_call_object_function(m_lvm, this, "on_accept", std::tie(), stream))
            delete stream;
    });

    m_mgr->set_connect_callback(token, [this]()
    {
        m_mgr->get_remote_ip(m_ip, m_token);
        lua_call_object_function(m_lvm, this, "on_connected");
    });

    m_mgr->set_error_callback(token, [this](const char* err)
    {
        lua_call_object_function(m_lvm, this, "on_error", std::tie(), err);
    });

    m_mgr->set_package_callback(token, [this](char* data, size_t data_len)
    {
        on_recv(data, data_len);
    });
}

lua_socket_node::~lua_socket_node()
{
    m_mgr->close(m_token);
}

int lua_socket_node::call(lua_State* L)
{
    int top = lua_gettop(L);
    if (top < 1)
        return 0;

	BYTE msg_id_data[MAX_ENCODE_LEN];
	size_t msg_id_len = encode_u64(msg_id_data, sizeof(msg_id_data), (char)msg_id::remote_call);

    m_ar_buffer->clear();
    size_t buffer_size = 0;
    auto* buffer = m_ar_buffer->peek_space(&buffer_size);
    size_t ar_len = 0;
    if (!m_archiver->save(&ar_len, buffer, buffer_size, L, 1, top))
        return 0;

	sendv_item items[] = {{msg_id_data, msg_id_len}, {buffer, ar_len}};
	m_mgr->sendv(m_token, items, _countof(items));
    lua_pushinteger(L, ar_len);
    return 1;
}

int lua_socket_node::forward_target(lua_State* L)
{
    int top = lua_gettop(L);
    if (top < 2)
        return 0;

	BYTE msg_id_data[MAX_ENCODE_LEN];
	size_t msg_id_len = encode_u64(msg_id_data, sizeof(msg_id_data), (char)msg_id::forward_target);

	uint32_t service_id = (uint32_t)lua_tointeger(L, 1);
	BYTE svr_id_data[MAX_ENCODE_LEN];
	size_t svr_id_len = encode_u64(msg_id_data, sizeof(msg_id_data), service_id);

	m_ar_buffer->clear();
	size_t buffer_size = 0;
	auto* buffer = m_ar_buffer->peek_space(&buffer_size);
    size_t ar_len = 0;
    if (!m_archiver->save(&ar_len, buffer, buffer_size, L, 2, top))
        return 0;

	sendv_item items[] = {{msg_id_data, msg_id_len}, {svr_id_data, svr_id_len}, {buffer, ar_len}};
    m_mgr->sendv(m_token, items, _countof(items));
    lua_pushinteger(L, ar_len);
    return 1;
}

template <msg_id forward_method>
int lua_socket_node::forward_by_class(lua_State* L)
{
    int top = lua_gettop(L);
    if (top < 2)
        return 0;

    static_assert(forward_method == msg_id::forward_master || forward_method == msg_id::forward_random ||
        forward_method == msg_id::forward_broadcast, "Unexpected forward method !");

	BYTE msg_id_data[MAX_ENCODE_LEN];
	size_t msg_id_len = encode_u64(msg_id_data, sizeof(msg_id_data), (char)forward_method);

	uint8_t group_id = (uint8_t)lua_tointeger(L, 1);
	BYTE group_id_data[MAX_ENCODE_LEN];
	size_t group_id_len = encode_u64(group_id_data, sizeof(group_id_data), group_id);

    m_ar_buffer->clear();
    size_t buffer_size = 0;
    auto* buffer = m_ar_buffer->peek_space(&buffer_size);
    size_t ar_len = 0;
    if (!m_archiver->save(&ar_len, buffer, buffer_size, L, 2, top))
        return 0;

	sendv_item items[] = {{msg_id_data, msg_id_len}, {group_id_data, group_id_len},{buffer, ar_len}};
	m_mgr->sendv(m_token, items, _countof(items));
	lua_pushinteger(L, ar_len);
    return 1;
}

// BKDR Hash
static uint32_t string_hash(const char* str)
{
    uint32_t seed = 131; // 31 131 1313 13131 131313 etc..
    uint32_t hash = 0;
    while (*str)
    {
        hash = hash * seed + (*str++);
    }
    return (hash & 0x7FFFFFFF);
}

int lua_socket_node::forward_hash(lua_State* L)
{
    int top = lua_gettop(L);
    if (top < 3)
        return 0;

	BYTE msg_id_data[MAX_ENCODE_LEN];
	size_t msg_id_len = encode_u64(msg_id_data, sizeof(msg_id_data), (char)msg_id::forward_hash);

    uint8_t group_id = (uint8_t)lua_tointeger(L, 1);
	BYTE group_id_data[MAX_ENCODE_LEN];
	size_t group_id_len = encode_u64(group_id_data, sizeof(group_id_data), group_id);

    int type = lua_type(L, 2);
    uint32_t hash_key = 0;
    if (type == LUA_TNUMBER)
    {
        hash_key = (uint32_t)lua_tointeger(L, 2);
    }
    else if (type == LUA_TSTRING)
    {
        const char* str = lua_tostring(L, 2);
        if (str == nullptr)
            return 0;
        hash_key = string_hash(str);
    }
    else
    {
        // unexpected hash key
        return 0;
    }

	BYTE hash_data[MAX_ENCODE_LEN];
	size_t hash_len = encode_u64(hash_data, sizeof(hash_data), hash_key);

	m_ar_buffer->clear();
	size_t buffer_size = 0;
	auto* buffer = m_ar_buffer->peek_space(&buffer_size);
    size_t ar_len = 0;
    if (!m_archiver->save(&ar_len, buffer, buffer_size, L, 3, top))
        return 0;

	sendv_item items[] = {{msg_id_data, msg_id_len}, {group_id_data, group_id_len}, {hash_data, hash_len}, {buffer, ar_len}};
	m_mgr->sendv(m_token, items, _countof(items));
    lua_pushinteger(L, ar_len);
    return 1;
}

void lua_socket_node::close()
{
    if (m_token != 0)
    {
        m_mgr->close(m_token);
        m_token = 0;
    }
}

void lua_socket_node::on_recv(char* data, size_t data_len)
{
    uint64_t msg = 0;
	size_t len = decode_u64(&msg, (BYTE*)data, data_len);
	if (len == 0)
		return;
	data += len;
	data_len -= len;

    switch ((msg_id)msg)
    {
    case msg_id::remote_call:
        on_call(data, data_len);
        break;

    case msg_id::forward_target:
        m_router->forward_target(data, data_len);
        break;

    case msg_id::forward_random:
        m_router->forward_random(data, data_len);
        break;

    case msg_id::forward_master:
        m_router->forward_master(data, data_len);
        break;

    case msg_id::forward_hash:
        m_router->forward_hash(data, data_len);
        break;

    case msg_id::forward_broadcast:
        m_router->forward_broadcast(data, data_len);
        break;

    default:
        break;
    }
}

void lua_socket_node::on_call(char* data, size_t data_len)
{
    lua_guard g(m_lvm);

    if (!lua_get_object_function(m_lvm, this, "on_call"))
        return;

    int param_count = 0;
    if (!m_archiver->load(&param_count, m_lvm, (BYTE*)data, data_len))
        return;

    lua_call_function(m_lvm, param_count, 0);
}

int create_socket_mgr(lua_State* L)
{
    int max_fd = (int)lua_tointeger(L, 1);
    if (max_fd <= 0)
    {
        lua_pushnil(L);
        return 1;
    }

    auto mgr = new lua_socket_mgr();
    if (mgr->setup(G(L)->mainthread, max_fd))
    {
        lua_push_object(L, mgr);
        return 1;
    }

    delete mgr;
    lua_pushnil(L);
    return 1;
}
