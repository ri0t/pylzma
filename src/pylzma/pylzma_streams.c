#include "pylzma.h"
#include "pylzma_streams.h"

static SRes
MemoryInStream_Read(void *p, void *buf, size_t *size)
{
    CMemoryInStream *self = (CMemoryInStream *) p;
    size_t toread = *size;
    if (toread > self->avail) {
        toread = self->avail;
    }
    memcpy(buf, self->data, toread);
    self->data += toread;
    self->avail -= toread;
    *size = toread;
    return SZ_OK;
}

void
CreateMemoryInStream(CMemoryInStream *stream, Byte *data, size_t size)
{
    stream->s.Read = MemoryInStream_Read;
    stream->data = data;
    stream->avail = size;
}

static SRes
MemoryInOutStream_Read(void *p, void *buf, size_t *size)
{
    CMemoryInOutStream *self = (CMemoryInOutStream *) p;
    size_t toread = *size;
    if (toread > self->avail) {
        toread = self->avail;
    }
    memcpy(buf, self->data, toread);
    if (self->avail > toread) {
        memmove(self->data, self->data + toread, self->avail - toread);
        self->data = (Byte *) realloc(self->data, self->avail - toread);
        self->avail -= toread;
    } else {
        free(self->data);
        self->data = NULL;
        self->avail = 0;
    }
    *size = toread;
    return SZ_OK;
}

void
CreateMemoryInOutStream(CMemoryInOutStream *stream)
{
    stream->s.Read = MemoryInOutStream_Read;
    stream->data = NULL;
    stream->avail = 0;
}

Bool
MemoryInOutStreamAppend(CMemoryInOutStream *stream, Byte *data, size_t size)
{
    if (!size) {
        return 1;
    }
    stream->data = (Byte *) realloc(stream->data, stream->avail + size);
    if (stream->data == NULL) {
        return 0;
    }
    memcpy(stream->data + stream->avail, data, size);
    stream->avail += size;
    return 1;
}

static SRes
PythonInStream_Read(void *p, void *buf, size_t *size)
{
    CPythonInStream *self = (CPythonInStream *) p;
    size_t toread = *size;
    PyObject *data;
    SRes res;
    
    START_BLOCK_THREADS
    data = PyObject_CallMethod(self->file, "read", "i", toread);
    if (data == NULL) {
        PyErr_Print();
        res = SZ_ERROR_READ;
    } else if (!PyString_Check(data)) {
        res = SZ_ERROR_READ;
    } else {
        *size = PyString_GET_SIZE(data);
        memcpy(buf, PyString_AS_STRING(data), min(*size, toread));
        res = SZ_OK;
    }
    Py_XDECREF(data);    
    END_BLOCK_THREADS
    return res;
}

void
CreatePythonInStream(CPythonInStream *stream, PyObject *file)
{
    stream->s.Read = PythonInStream_Read;
    stream->file = file;
}

static size_t
MemoryOutStream_Write(void *p, const void *buf, size_t size)
{
    CMemoryOutStream *self = (CMemoryOutStream *) p;
    self->data = (Byte *) realloc(self->data, self->size + size);
    if (self->data == NULL) {
        self->size = 0;
        return 0;
    }
    memcpy(self->data + self->size, buf, size);
    self->size += size;
    return size;
}

void
CreateMemoryOutStream(CMemoryOutStream *stream)
{
    stream->s.Write = MemoryOutStream_Write;
    stream->data = NULL;
    stream->size = 0;
}

void
MemoryOutStreamDiscard(CMemoryOutStream *stream, size_t size)
{
    if (size >= stream->size) {
        // Clear stream
        free(stream->data);
        stream->data = NULL;
        stream->size = 0;
    } else {
        memmove(stream->data, stream->data + size, stream->size - size);
        stream->data = (Byte *) realloc(stream->data, stream->size - size);
        stream->size -= size;
    }
}