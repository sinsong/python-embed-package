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
    config.parse_argv = 2; // disable consume argv for python interpreter

    // set program name
    status = PyConfig_SetBytesString(&config, &config.program_name, argv[0]);
    if (PyStatus_Exception(status))
    {
        goto exception;
    }

    // necessary, for pass argc, argv to python environment
    status = PyConfig_SetBytesArgv(&config, argc, argv);
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

    // Python Initialize Success

    // -------------------------------------------------------------------------

    PyObject *pName;
    PyObject *pModule;
    PyObject *pFunc;
    PyObject *pValue;

    // import
    pName = PyUnicode_DecodeFSDefault(MODULE);
    pModule = PyImport_Import(pName);
    Py_DECREF(pName);
    if (pModule == NULL)
    {
        PyErr_Print();
        fprintf(stderr, "Import Module failed\n");
        return 1;
    }

    // get function
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

    // call
    pValue = PyObject_CallObject(pFunc, NULL);
    if (pValue == NULL)
    {
        Py_DECREF(pFunc);
        Py_DECREF(pModule);
        PyErr_Print();
        fprintf(stderr, "call failed\n");
    }

    long ret = PyLong_AsLong(pValue);
    Py_DECREF(pValue);
    if (Py_FinalizeEx() < 0)
    {
        return 120;
    }

    return ret;

exception:
    PyConfig_Clear(&config);
    if (PyStatus_IsExit(status))
    {
        return status.exitcode;
    }
    Py_ExitStatusException(status);
}
