#include <iostream>

#include <string_view>
#include <string>

#include "hostfxr.h"
#include "nethost.h"
#include "coreclr_delegates.h"

#include <assert.h>

#include <Windows.h>

class HostFxrLibrary
{
public:
	HostFxrLibrary(std::wstring_view runtimeConfigPath)
		: m_module(nullptr)
		, m_init_for_cmd_line_fptr(nullptr)
		, m_init_for_config_fptr(nullptr)
		, m_get_delegate_fptr(nullptr)
		, m_run_app_fptr(nullptr)
		, m_close_fptr(nullptr)
		, m_config_handle(nullptr)
		, m_load_assembly_and_get_function_pointer_fn(nullptr)
	{
		init_hostfxr();
		init_hostfxr_funcs();
		init_clr_funcs(runtimeConfigPath);
	}
	HostFxrLibrary(const HostFxrLibrary&) = delete;
	HostFxrLibrary(HostFxrLibrary&&) = delete;

	~HostFxrLibrary()
	{
		m_close_fptr(m_config_handle);
	}

	HostFxrLibrary& operator=(const HostFxrLibrary&) = delete;
	HostFxrLibrary& operator=(HostFxrLibrary&&) = delete;

	void* load_function(std::wstring_view assemblyPath, std::wstring_view classPath, std::wstring_view methodName) const
	{
		std::wstring type_name;
		type_name += classPath;
		type_name += L", ";
		type_name += assemblyPath.substr(assemblyPath.find_last_of(L"\\") + 1);
		type_name = type_name.substr(0, type_name.find_last_of(L"."));

		// Format of the typename is
		// <namespace>.<classname>, <library filename without extension>
		void* func_ptr = nullptr;
		int rc = m_load_assembly_and_get_function_pointer_fn(
			assemblyPath.data(),
			type_name.data(),
			methodName.data(),
			UNMANAGEDCALLERSONLY_METHOD,
			nullptr,
			(void**)&func_ptr);

		assert(rc == 0 && func_ptr != nullptr && "Failure: load_assembly_and_get_function_pointer()");

		return func_ptr;
	}

private:
	void* load_procedure(void* h, const char* name)
	{
		void* f = ::GetProcAddress((HMODULE)h, name);
		assert(f != nullptr);
		return f;
	}

	// Init hostfxr itself, finding its path and loading the module
	void init_hostfxr()
	{
		get_hostfxr_parameters params{ sizeof(get_hostfxr_parameters), nullptr, nullptr };
		char_t hostfxr_path[MAX_PATH];
		size_t hostfxr_path_length = sizeof(hostfxr_path) / sizeof(char_t);

		assert(get_hostfxr_path(hostfxr_path, &hostfxr_path_length, &params) == 0 && "Failed to find hostfxr path");
		m_module = LoadLibraryW(hostfxr_path);
		assert(m_module != nullptr);
	}
	// Calls into the hostfxr module to load the api functions
	void init_hostfxr_funcs()
	{
		m_init_for_cmd_line_fptr = (hostfxr_initialize_for_dotnet_command_line_fn)load_procedure(m_module, "hostfxr_initialize_for_dotnet_command_line");
		m_init_for_config_fptr = (hostfxr_initialize_for_runtime_config_fn)load_procedure(m_module, "hostfxr_initialize_for_runtime_config");
		m_get_delegate_fptr = (hostfxr_get_runtime_delegate_fn)load_procedure(m_module, "hostfxr_get_runtime_delegate");
		m_run_app_fptr = (hostfxr_run_app_fn)load_procedure(m_module, "hostfxr_run_app");
		m_close_fptr = (hostfxr_close_fn)load_procedure(m_module, "hostfxr_close");
	}
	// Loads clr api functions
	void init_clr_funcs(std::wstring_view runtimeConfigPath)
	{
		// Load .NET Core
		int rc = m_init_for_config_fptr(runtimeConfigPath.data(), nullptr, &m_config_handle);
		if (rc != 0 || m_config_handle == nullptr)
		{
			std::cerr << "Init failed: " << std::hex << std::showbase << rc << std::endl;
			m_close_fptr(m_config_handle);
			return;
		}

		// Get the load assembly function pointer
		void* load_assembly_and_get_function_pointer = nullptr;
		rc = m_get_delegate_fptr(
			m_config_handle,
			hdt_load_assembly_and_get_function_pointer,
			&load_assembly_and_get_function_pointer);
		if (rc != 0 || load_assembly_and_get_function_pointer == nullptr)
			std::cerr << "Get delegate failed: " << std::hex << std::showbase << rc << std::endl;

		m_load_assembly_and_get_function_pointer_fn = (load_assembly_and_get_function_pointer_fn)load_assembly_and_get_function_pointer;
	}

private:
	// The module to hostfxr itself
	HMODULE m_module;

	// These are functions, part of the hostfxr.dll itself, which we're loading at runtime
	hostfxr_initialize_for_dotnet_command_line_fn m_init_for_cmd_line_fptr;
	hostfxr_initialize_for_runtime_config_fn m_init_for_config_fptr;
	hostfxr_get_runtime_delegate_fn m_get_delegate_fptr;
	hostfxr_run_app_fn m_run_app_fptr;
	hostfxr_close_fn m_close_fptr;

	// This is part of the Common Language Runtime (CLR)
	hostfxr_handle m_config_handle;
	load_assembly_and_get_function_pointer_fn m_load_assembly_and_get_function_pointer_fn;
};

class DotNetAssembly
{
public:
	DotNetAssembly(std::wstring_view assemblyPath, const HostFxrLibrary* hostFxrLib)
		: m_assembly_path(assemblyPath)
		, m_host_fxr_path(hostFxrLib)
	{

	}

	template <typename T>
	T get_function(std::wstring_view classPath, std::wstring_view methodName)
	{
		return (T)m_host_fxr_path->load_function(m_assembly_path, classPath, methodName);
	}

private:
	std::wstring m_assembly_path;
	const HostFxrLibrary* m_host_fxr_path;
};

int main()
{
	std::wstring runtime_config = L"D:\\Testing\\CppAndCs\\Config\\Test.runtimeconfig.json";
	std::wstring assembly_path = L"D:\\Testing\\CppAndCs\\DotNetLib\\obj\\Debug\\DotNetLib.dll";
	HostFxrLibrary host_fxr_library(runtime_config);
	DotNetAssembly assembly(assembly_path, &host_fxr_library);

	std::wstring type_name = L"DotNetLib2.TestClass";
	std::wstring method_name = L"Hello2";
	using hello_fn = void(*)(const char*);
	hello_fn func = assembly.get_function<hello_fn>(type_name, method_name);

	func("hello");
	func("hello again");
	
	return 0;
}