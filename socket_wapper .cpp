/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016-11-01, trumanzhao@foxmail.com
*/
#include "tools.h"
#include "socket_wapper.h"

lua_socket_listener::lua_socket_listener(socket_mgr* mgr, int token)
{
	mgr->add_ref();
	m_mgr = mgr;
	m_token = token;
}

lua_socket_listener::~lua_socket_listener()
{
	m_mgr->close(m_token);
	SAFE_RELEASE(m_mgr);
}

lua_socket_stream::lua_socket_stream(socket_mgr* mgr, int token)
{
	mgr->add_ref();
	m_mgr = mgr;
	m_token = token;
}

lua_socket_stream::~lua_socket_stream()
{
	m_mgr->close(m_token);
	SAFE_RELEASE(m_mgr);
}

int lua_socket_stream::call(lua_State* L)
{

	return 0;
}

EXPORT_CLASS_BEGIN(lua_socket_mgr)
EXPORT_CLASS_END()

lua_socket_mgr::~lua_socket_mgr()
{
	SAFE_RELEASE(m_mgr);
}

bool lua_socket_mgr::setup(int max_connection)
{
	m_mgr = create_socket_mgr(max_connection);
	return m_mgr != nullptr;
}

lua_socket_mgr* lua_create_socket_mgr(int max_connection)
{
	auto mgr = new lua_socket_mgr();
	if (mgr->setup(max_connection))
	{
		return mgr;
	}
	delete mgr;
	return nullptr;
}
