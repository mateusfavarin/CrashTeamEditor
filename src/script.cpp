#include "script.h"

#include "level.h"

#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

#include <exception>
#include <filesystem>
#include <memory>
#include <mutex>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace py = pybind11;

#if defined(CTE_EMBEDDED_BUILD)
extern "C" PyObject* PyInit_crashteameditor();

static bool RegisterCrashTeamEditorModule(std::string& error)
{
	static bool registered = false;
	if (registered) { return true; }

	int result = PyImport_AppendInittab("crashteameditor", PyInit_crashteameditor);
	if (result != 0)
	{
		error = "Failed to register embedded module 'crashteameditor'";
		return false;
	}
	registered = true;
	return true;
}
#else
static bool RegisterCrashTeamEditorModule(std::string&) { return true; }
#endif

namespace
{
	struct PythonStdRedirect
	{
		PythonStdRedirect()
			: sys(py::module_::import("sys")),
			prevStdout(sys.attr("stdout")),
			prevStderr(sys.attr("stderr")),
			buffer(py::module_::import("io").attr("StringIO")())
		{
			sys.attr("stdout") = buffer;
			sys.attr("stderr") = buffer;
		}

		~PythonStdRedirect()
		{
			sys.attr("stdout") = prevStdout;
			sys.attr("stderr") = prevStderr;
		}

		std::string Get() const
		{
			return buffer.attr("getvalue")().cast<std::string>();
		}

		py::module_ sys;
		py::object prevStdout;
		py::object prevStderr;
		py::object buffer;
	};

	static std::unique_ptr<py::scoped_interpreter> g_pythonInterpreter;
	static std::once_flag g_pythonInitFlag;
	static std::string g_pythonInitError;
	static std::once_flag g_moduleInitFlag;
	static std::string g_moduleInitError;

#if defined(_WIN32)
	static std::filesystem::path GetExecutableDir(std::string& error)
	{
		std::wstring buffer(32768, L'\0');
		DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
		if (length == 0 || length >= buffer.size())
		{
			error = "Failed to resolve executable path for bundled Python";
			return {};
		}

		buffer.resize(length);
		return std::filesystem::path(buffer).parent_path();
	}

	static bool AppendPythonPath(PyConfig& config, const std::filesystem::path& path, std::string& error)
	{
		std::error_code ec;
		if (!std::filesystem::exists(path, ec))
		{
			return true;
		}

		std::wstring widePath = path.wstring();
		PyStatus status = PyWideStringList_Append(&config.module_search_paths, widePath.c_str());
		if (PyStatus_Exception(status))
		{
			error = status.err_msg ? status.err_msg : "Failed to configure Python search paths";
			return false;
		}
		return true;
	}

	static bool ConfigureBundledPython(PyConfig& config, std::string& error)
	{
		std::filesystem::path exeDir = GetExecutableDir(error);
		if (exeDir.empty())
		{
			return false;
		}

		std::filesystem::path pythonRoot = exeDir / "python";
		std::error_code ec;
		if (!std::filesystem::exists(pythonRoot, ec))
		{
			error = "Bundled Python directory not found: " + pythonRoot.string();
			return false;
		}

		std::wstring pythonHome = pythonRoot.wstring();
		PyStatus status = PyConfig_SetString(&config, &config.home, pythonHome.c_str());
		if (PyStatus_Exception(status))
		{
			error = status.err_msg ? status.err_msg : "Failed to configure Python home";
			return false;
		}

		config.module_search_paths_set = 1;
		if (!AppendPythonPath(config, pythonRoot, error)) { return false; }

		std::error_code iterEc;
		for (const auto& entry : std::filesystem::directory_iterator(pythonRoot, iterEc))
		{
			if (iterEc) { break; }
			if (!entry.is_regular_file(iterEc)) { continue; }
			const auto& path = entry.path();
			if (path.extension() == ".zip")
			{
				const auto filename = path.filename().wstring();
				if (filename.rfind(L"python", 0) == 0)
				{
					if (!AppendPythonPath(config, path, error)) { return false; }
				}
			}
		}

		if (!AppendPythonPath(config, pythonRoot / "Lib", error)) { return false; }
		if (!AppendPythonPath(config, pythonRoot / "DLLs", error)) { return false; }
		if (!AppendPythonPath(config, pythonRoot / "Lib" / "site-packages", error)) { return false; }

		return true;
	}
#endif

	static bool EnsurePythonInterpreter(std::string& error)
	{
		std::call_once(g_pythonInitFlag, []()
			{
				if (!RegisterCrashTeamEditorModule(g_pythonInitError)) { return; }
				try
				{
#if defined(_WIN32)
					PyConfig config;
					PyConfig_InitIsolatedConfig(&config);
					config.parse_argv = 0;
					config.use_environment = 0;
					config.site_import = 1;
					config.user_site_directory = 0;

					if (!ConfigureBundledPython(config, g_pythonInitError))
					{
						PyConfig_Clear(&config);
						return;
					}

					g_pythonInterpreter = std::make_unique<py::scoped_interpreter>(&config, 0, nullptr, false);
#else
					g_pythonInterpreter = std::make_unique<py::scoped_interpreter>();
#endif
				}
				catch (const std::exception& ex)
				{
					g_pythonInitError = ex.what();
				}
			});

		if (!g_pythonInitError.empty())
		{
			error = g_pythonInitError;
			return false;
		}
		if (!g_pythonInterpreter)
		{
			error = "[Failed to initialize Python interpreter]";
			return false;
		}
		return true;
	}

	static bool EnsureCrashTeamEditorModule(std::string& error)
	{
		std::call_once(g_moduleInitFlag, []()
			{
				try
				{
					py::module_::import("crashteameditor");
				}
				catch (const py::error_already_set& err)
				{
					g_moduleInitError = err.what();
				}
			});

		if (!g_moduleInitError.empty())
		{
			error = g_moduleInitError;
			return false;
		}
		return true;
	}
} // namespace

namespace Script
{
	std::string ExecutePythonScript(Level& level, const std::string& script)
	{
		std::string initError;
		if (!EnsurePythonInterpreter(initError))
		{
			return initError.empty() ? "[Failed to initialize Python interpreter]" : initError;
		}

		if (!Py_IsInitialized())
		{
			return "[Python interpreter not initialized]";
		}

		py::gil_scoped_acquire gil;
		std::string importError;
		if (!EnsureCrashTeamEditorModule(importError))
		{
			return importError.empty() ? "[Failed to import crashteameditor module]" : importError;
		}

		py::module_ main = py::module_::import("__main__");
		py::dict globals = main.attr("__dict__");
		globals["m_lev"] = py::cast(&level);

		PythonStdRedirect redirect;
		std::string output;
		try
		{
			py::exec(script, globals);
			output = redirect.Get();
		}
		catch (const py::error_already_set& error)
		{
			output = error.what();
			std::string captured = redirect.Get();
			if (!captured.empty())
			{
				if (!output.empty()) { output += "\n"; }
				output += captured;
			}
		}
		return output;
	}
}
