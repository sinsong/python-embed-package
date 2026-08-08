#define PY_SSIZE_T_CLEAN
#include <Python.h>

#define MODULE ""
#define ENTRY ""

// § 1.3. Pure Embedding
// https://docs.python.org/3.14/extending/embedding.html#pure-embedding

int
main(int argc, char *argv[])
{
    PyStatus status;
    PyConfig config;
    PyConfig_InitPythonConfig(&config);

    // if ._pth file present...
    // isolated -> 1; use_environment -> 0; site_import -> 0; safe_path -> 1
    config.isolated = 1; // for embedded
    // config.user_site_directory = 0; // set by isolated mode

    // set program name
    status = PyConfig_SetBytesString(&config, &config.program_name, argv[0]);
    if (PyStatus_Exception(status))
    {
        goto exception;
    }

    // read config
    status = PyConfig_Read(&config);
    if (PyStatus_Exception(status))
    {
        goto exception;
    }

    // initialize interpreter
    status = Py_InitializeFromConfig(&config);
    if (PyStatus_Exception(status))
    {
        fprintf(stderr, "Py_InitializeFromConfig failed\n");
        goto exception;
    }
    PyConfig_Clear(&config);

    // adjust module search path through sys.path
    PyObject *sys_path = PySys_GetObject("path"); // sys.path
    PyObject *additional_module_path = PyUnicode_DecodeFSDefault(".");
    PyList_Append(sys_path, additional_module_path);

    // -------------------------------------------------------------------------

    PyObject *pName;
    PyObject *pModule;
    PyObject *pFunc;
    PyObject *pArgs;
    PyObject *pValue;

    pName = PyUnicode_DecodeFSDefault(MODULE);
    pModule = PyImport_Import(pName);
    Py_DECREF(pName);

    if (pModule == NULL)
    {
        PyErr_Print();
        fprintf(stderr, "Import Module failed\n");
        return 1;
    }

    pFunc = PyObject_GetAttrString(pModule, ENTRY);

    if (!pFunc || !PyCallable_Check(pFunc))
    {
        if (PyErr_Occurred())
            PyErr_Print();
        fprintf(stderr, "find function failed\n");
        Py_XDECREF(pFunc);
        Py_DECREF(pModule);
        return 1;
    }

    pArgs = PyTuple_New(argc - 1); // crossbone to next line
    for (int i = 0; i < argc - 1; ++i)
    {
        pValue = PyUnicode_DecodeFSDefault(argv[i + 1]);
           if (!pValue)
        {
            Py_DECREF(pArgs);
            Py_DECREF(pModule);
            return 1;
        }
        PyTuple_SetItem(pArgs, i, pValue);
    }

    pValue = PyObject_CallObject(pFunc, pArgs);
    if (pValue == NULL)
    {
        Py_DECREF(pFunc);
        Py_DECREF(pModule);
        PyErr_Print();
        fprintf(stderr, "call failed\n");
    }

    Py_DECREF(pArgs);

    long ret = PyLong_AsLong(pValue);
    Py_DECREF(pValue);
    if (Py_FinalizeEx() < 0)
    {
        return 120;
    }

    return ret;

exception:
    PyConfig_Clear(&config);
    Py_ExitStatusException(status);
}
