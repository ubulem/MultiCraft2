/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 3.0 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "lua_api/l_internal.h"
#include "common/c_converter.h"
#include "common/c_content.h"
#include "lua_api/l_http.h"
#include "httpfetch.h"
#include "settings.h"
#include "debug.h"
#include "log.h"

#include <algorithm>
#include <iomanip>
#include <cctype>

#define HTTP_API(name) \
	lua_pushstring(L, #name); \
	lua_pushcfunction(L, l_http_##name); \
	lua_settable(L, -3);

#if USE_CURL
void ModApiHttp::read_http_fetch_request(lua_State *L, HTTPFetchRequest &req)
{
	luaL_checktype(L, 1, LUA_TTABLE);

	req.caller = httpfetch_caller_alloc_secure();
	getstringfield(L, 1, "url", req.url);
	lua_getfield(L, 1, "user_agent");
	if (lua_isstring(L, -1))
		req.useragent = getstringfield_default(L, 1, "user_agent", "");
	lua_pop(L, 1);
	req.multipart = getboolfield_default(L, 1, "multipart", false);
	req.timeout = getintfield_default(L, 1, "timeout", 3) * 1000;

	lua_getfield(L, 1, "method");
	if (lua_isstring(L, -1)) {
		std::string mth = getstringfield_default(L, 1, "method", "");
		if (mth == "GET")
			req.method = HTTP_GET;
		else if (mth == "POST")
			req.method = HTTP_POST;
		else if (mth == "PUT")
			req.method = HTTP_PUT;
		else if (mth == "DELETE")
			req.method = HTTP_DELETE;
	}
	lua_pop(L, 1);

	// post_data: if table, post form data, otherwise raw data DEPRECATED use data and method instead
	lua_getfield(L, 1, "post_data");
	if (lua_isnil(L, 2)) {
		lua_pop(L, 1);
		lua_getfield(L, 1, "data");
	}
	else {
		req.method = HTTP_POST;
	}

	if (lua_istable(L, 2)) {
		lua_pushnil(L);
		while (lua_next(L, 2) != 0) {
			req.fields[readParam<std::string>(L, -2)] = readParam<std::string>(L, -1);
			lua_pop(L, 1);
		}
	} else if (lua_isstring(L, 2)) {
		req.raw_data = readParam<std::string>(L, 2);
	}

	lua_pop(L, 1);

	lua_getfield(L, 1, "extra_headers");
	if (lua_istable(L, 2)) {
		lua_pushnil(L);
		while (lua_next(L, 2) != 0) {
			req.extra_headers.emplace_back(readParam<std::string>(L, -1));
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);
}

void ModApiHttp::push_http_fetch_result(lua_State *L, HTTPFetchResult &res, bool completed)
{
	lua_newtable(L);
	setboolfield(L, -1, "succeeded", res.succeeded);
	setboolfield(L, -1, "timeout", res.timeout);
	setboolfield(L, -1, "completed", completed);
	setintfield(L, -1, "code", res.response_code);
	setstringfield(L, -1, "data", res.data);
}

// http_api.fetch_sync(HTTPRequest definition)
int ModApiHttp::l_http_fetch_sync(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	HTTPFetchRequest req;
	read_http_fetch_request(L, req);

	infostream << "Mod performs HTTP request with URL " << req.url << std::endl;

	HTTPFetchResult res;
	bool completed = httpfetch_sync_interruptible(req, res);

	push_http_fetch_result(L, res, completed);

	return 1;
}

// http_api.fetch_async(HTTPRequest definition)
int ModApiHttp::l_http_fetch_async(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	HTTPFetchRequest req;
	read_http_fetch_request(L, req);

	infostream << "Mod performs HTTP request with URL " << req.url << std::endl;
	httpfetch_async(req);

	// Convert handle to hex string since lua can't handle 64-bit integers
	std::stringstream handle_conversion_stream;
	handle_conversion_stream << std::hex << req.caller;
	std::string caller_handle(handle_conversion_stream.str());

	lua_pushstring(L, caller_handle.c_str());
	return 1;
}

// http_api.fetch_async_get(handle)
int ModApiHttp::l_http_fetch_async_get(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	std::string handle_str = luaL_checkstring(L, 1);

	// Convert hex string back to 64-bit handle
	u64 handle;
	std::stringstream handle_conversion_stream;
	handle_conversion_stream << std::hex << handle_str;
	handle_conversion_stream >> handle;

	HTTPFetchResult res;
	bool completed = httpfetch_async_get(handle, res);

	push_http_fetch_result(L, res, completed);

	return 1;
}

int ModApiHttp::l_request_http_api(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	// We have to make sure that this function is being called directly by
	// a mod, otherwise a malicious mod could override this function and
	// steal its return value.
	lua_Debug info;

	// Make sure there's only one item below this function on the stack...
	if (lua_getstack(L, 2, &info)) {
		return 0;
	}
	FATAL_ERROR_IF(!lua_getstack(L, 1, &info), "lua_getstack() failed");
	FATAL_ERROR_IF(!lua_getinfo(L, "S", &info), "lua_getinfo() failed");

	// ...and that that item is the main file scope.
	if (strcmp(info.what, "main") != 0) {
		return 0;
	}

	// Mod must be listed in secure.http_mods or secure.trusted_mods
	lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_CURRENT_MOD_NAME);
	if (!lua_isstring(L, -1)) {
		return 0;
	}

	std::string mod_name = readParam<std::string>(L, -1);
	std::string http_mods = g_settings->get("secure.http_mods");
	http_mods.erase(std::remove(http_mods.begin(), http_mods.end(), ' '), http_mods.end());
	std::vector<std::string> mod_list_http = str_split(http_mods, ',');

	std::string trusted_mods = g_settings->get("secure.trusted_mods");
	trusted_mods.erase(std::remove(trusted_mods.begin(), trusted_mods.end(), ' '), trusted_mods.end());
	std::vector<std::string> mod_list_trusted = str_split(trusted_mods, ',');

	mod_list_http.insert(mod_list_http.end(), mod_list_trusted.begin(), mod_list_trusted.end());
	if (std::find(mod_list_http.begin(), mod_list_http.end(), mod_name) == mod_list_http.end()) {
		lua_pushnil(L);
		return 1;
	}

	lua_getglobal(L, "core");
	lua_getfield(L, -1, "http_add_fetch");

	lua_newtable(L);
	HTTP_API(fetch_async);
	HTTP_API(fetch_async_get);

	// Stack now looks like this:
	// <core.http_add_fetch> <table with fetch_async, fetch_async_get>
	// Now call core.http_add_fetch to append .fetch(request, callback) to table
	lua_call(L, 1, 1);

	return 1;
}

int ModApiHttp::l_get_http_api(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	lua_newtable(L);
	HTTP_API(fetch_async);
	HTTP_API(fetch_async_get);
	HTTP_API(fetch_sync);

	return 1;
}

#endif

void ModApiHttp::Initialize(lua_State *L, int top)
{
#if USE_CURL

	bool isMainmenu = false;
#ifndef SERVER
	isMainmenu = ModApiBase::getGuiEngine(L) != nullptr;
#endif

	if (isMainmenu) {
		API_FCT(get_http_api);
	} else {
		API_FCT(request_http_api);
	}

#endif
}

void ModApiHttp::InitializeAsync(lua_State *L, int top)
{
#if USE_CURL
	API_FCT(get_http_api);
#endif
}
