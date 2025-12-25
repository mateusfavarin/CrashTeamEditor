#include "script.h"

#include "level.h"

#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

#include <exception>
#include <memory>
#include <mutex>

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

	static bool EnsurePythonInterpreter(std::string& error)
	{
		std::call_once(g_pythonInitFlag, []()
			{
				if (!RegisterCrashTeamEditorModule(g_pythonInitError)) { return; }
				try
				{
					g_pythonInterpreter = std::make_unique<py::scoped_interpreter>();
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
