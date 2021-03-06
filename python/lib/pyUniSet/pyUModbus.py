# This file was automatically generated by SWIG (http://www.swig.org).
# Version 3.0.0
#
# Do not make changes to this file unless you know what you are doing--modify
# the SWIG interface file instead.





from sys import version_info
if version_info >= (2,6,0):
    def swig_import_helper():
        from os.path import dirname
        import imp
        fp = None
        try:
            fp, pathname, description = imp.find_module('_pyUModbus', [dirname(__file__)])
        except ImportError:
            import _pyUModbus
            return _pyUModbus
        if fp is not None:
            try:
                _mod = imp.load_module('_pyUModbus', fp, pathname, description)
            finally:
                fp.close()
            return _mod
    _pyUModbus = swig_import_helper()
    del swig_import_helper
else:
    import _pyUModbus
del version_info
def _swig_setattr_nondynamic(self,class_type,name,value,static=1):
    if (name == "thisown"): return self.this.own(value)
    if (name == "this"):
        if type(value).__name__ == 'SwigPyObject':
            self.__dict__[name] = value
            return
    method = class_type.__swig_setmethods__.get(name,None)
    if method: return method(self,value)
    if (not static):
        self.__dict__[name] = value
    else:
        raise AttributeError("You cannot add attributes to %s" % self)

def _swig_setattr(self,class_type,name,value):
    return _swig_setattr_nondynamic(self,class_type,name,value,0)

def _swig_getattr(self,class_type,name):
    if (name == "thisown"): return self.this.own()
    method = class_type.__swig_getmethods__.get(name,None)
    if method: return method(self)
    raise AttributeError(name)

def _swig_repr(self):
    try: strthis = "proxy of " + self.this.__repr__()
    except: strthis = ""
    return "<%s.%s; %s >" % (self.__class__.__module__, self.__class__.__name__, strthis,)

class UModbus:
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, UModbus, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, UModbus, name)
    __repr__ = _swig_repr
    def __init__(self): 
        this = _pyUModbus.new_UModbus()
        try: self.this.append(this)
        except: self.this = this
    __swig_destroy__ = _pyUModbus.delete_UModbus
    __del__ = lambda self : None;
    def getUIType(self): return _pyUModbus.UModbus_getUIType(self)
    def isWriteFunction(self, *args): return _pyUModbus.UModbus_isWriteFunction(self, *args)
    def prepare(self, *args): return _pyUModbus.UModbus_prepare(self, *args)
    def connect(self, *args): return _pyUModbus.UModbus_connect(self, *args)
    def conn_port(self): return _pyUModbus.UModbus_conn_port(self)
    def conn_ip(self): return _pyUModbus.UModbus_conn_ip(self)
    def isConnection(self): return _pyUModbus.UModbus_isConnection(self)
    def setTimeout(self, *args): return _pyUModbus.UModbus_setTimeout(self, *args)
    def mbread(self, *args): return _pyUModbus.UModbus_mbread(self, *args)
    def getWord(self, *args): return _pyUModbus.UModbus_getWord(self, *args)
    def getByte(self, *args): return _pyUModbus.UModbus_getByte(self, *args)
    def getBit(self, *args): return _pyUModbus.UModbus_getBit(self, *args)
    def mbwrite(self, *args): return _pyUModbus.UModbus_mbwrite(self, *args)
UModbus_swigregister = _pyUModbus.UModbus_swigregister
UModbus_swigregister(UModbus)

class Params:
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, Params, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, Params, name)
    __repr__ = _swig_repr
    max = _pyUModbus.Params_max
    def __init__(self): 
        this = _pyUModbus.new_Params()
        try: self.this.append(this)
        except: self.this = this
    def add(self, *args): return _pyUModbus.Params_add(self, *args)
    __swig_setmethods__["argc"] = _pyUModbus.Params_argc_set
    __swig_getmethods__["argc"] = _pyUModbus.Params_argc_get
    __swig_setmethods__["argv"] = _pyUModbus.Params_argv_set
    __swig_getmethods__["argv"] = _pyUModbus.Params_argv_get
    __swig_getmethods__["inst"] = lambda x: _pyUModbus.Params_inst
    __swig_destroy__ = _pyUModbus.delete_Params
    __del__ = lambda self : None;
Params_swigregister = _pyUModbus.Params_swigregister
Params_swigregister(Params)
cvar = _pyUModbus.cvar
DefaultID = cvar.DefaultID

def Params_inst():
  return _pyUModbus.Params_inst()
Params_inst = _pyUModbus.Params_inst

# This file is compatible with both classic and new-style classes.


