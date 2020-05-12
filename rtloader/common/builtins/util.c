// Unless explicitly stated otherwise all files in this repository are licensed
// under the Apache License Version 2.0.
// This product includes software developed at Datadog (https://www.datadoghq.com/).
// Copyright 2019-2020 Datadog, Inc.
#include "util.h"
#include "datadog_agent.h"
#include "util.h"

#include <stringutils.h>

static cb_obfuscate_sql_t cb_obfuscate_sql = NULL;

static PyObject *headers(PyObject *self, PyObject *args, PyObject *kwargs);
static PyObject *get_hostname(PyObject *self, PyObject *args);
static PyObject *get_clustername(PyObject *self, PyObject *args);
static PyObject *log_message(PyObject *self, PyObject *args);
static PyObject *set_external_tags(PyObject *self, PyObject *args);
static PyObject *obfuscate_sql(PyObject *self, PyObject *args);

static PyMethodDef methods[] = {
    { "headers", (PyCFunction)headers, METH_VARARGS | METH_KEYWORDS, "Get standard set of HTTP headers." },
    { "obfuscate_sql", (PyCFunction)obfuscate_sql, METH_VARARGS, "Obfuscate Sql." },
    { NULL, NULL } // guards
};

#ifdef DATADOG_AGENT_THREE
static struct PyModuleDef module_def = { PyModuleDef_HEAD_INIT, UTIL_MODULE_NAME, NULL, -1, methods };

PyMODINIT_FUNC PyInit_util(void)
{
    return PyModule_Create(&module_def);
}
#elif defined(DATADOG_AGENT_TWO)
// in Python2 keep the object alive for the program lifetime
static PyObject *module;

void Py2_init_util()
{
    module = Py_InitModule(UTIL_MODULE_NAME, methods);
}
#endif

void _set_obfuscate_sql_cb(cb_obfuscate_sql_t cb)
{
    cb_obfuscate_sql = cb;
}

/*! \fn PyObject *headers(PyObject *self, PyObject *args, PyObject *kwargs)
    \brief This function provides a standard set of HTTP headers the caller might want to
    use for HTTP requests.
    \param self A PyObject* pointer to the util module.
    \param args A PyObject* pointer to the `agentConfig`, but not expected to be used.
    \param kwargs A PyObject* pointer to a dictonary. If the `http_host` key is present
    it will be added to the headers.
    \return a PyObject * pointer to a python dictionary with the expected headers.

    This function is callable as the `util.headers` python method, the entry point:
    `_public_headers()` is provided in the `datadog_agent` module, the method is duplicated.
*/
PyObject *headers(PyObject *self, PyObject *args, PyObject *kwargs)
{
    return _public_headers(self, args, kwargs);
}

/*! \fn PyObject *obfuscate_sql(PyObject *self, PyObject *args)
    \brief This function implements the `datadog_agent.obfuscate_sql` method, retrieving
    the value for the key previously stored.
    \param self A PyObject* pointer to the `datadog_agent` module.
    \param args A PyObject* pointer to a tuple containing the key to retrieve.
    \return A PyObject* pointer to the value.

    This function is callable as the `datadog_agent.obfuscate_sql` Python method and
    uses the `cb_obfuscate_sql()` callback to retrieve the value from the agent
    with CGO. If the callback has not been set `None` will be returned.
*/
static PyObject *obfuscate_sql(PyObject *self, PyObject *args)
{
    // callback must be set
    if (cb_obfuscate_sql == NULL) {
        Py_RETURN_NONE;
    }

    char *key;

    // datadog_agent.obfuscate_sql(key)
    if (!PyArg_ParseTuple(args, "s", &key)) {
        return NULL;
    }

    char *v = NULL;
    Py_BEGIN_ALLOW_THREADS
    v = cb_obfuscate_sql(key);
    Py_END_ALLOW_THREADS

    if (v == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "failed to read data");
        return NULL;
    }

    PyObject *retval = PyStringFromCString(v);
    cgo_free(v);
    return retval;
}
