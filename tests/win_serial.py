"""Small Windows serial transport; avoids installing pyserial on the user's PC."""
import ctypes as C
from ctypes import wintypes as W
class DCB(C.Structure):
    _fields_=[('length',W.DWORD),('baud',W.DWORD),('flags',W.DWORD),('reserved',W.WORD),
              ('xon_limit',W.WORD),('xoff_limit',W.WORD),('byte_size',W.BYTE),('parity',W.BYTE),
              ('stop_bits',W.BYTE),('xon',C.c_char),('xoff',C.c_char),('error',C.c_char),
              ('eof',C.c_char),('event',C.c_char),('reserved1',W.WORD)]
class Timeouts(C.Structure):
    _fields_=[(name,W.DWORD) for name in ('interval','read_multiplier','read_constant','write_multiplier','write_constant')]
class Stat(C.Structure):
    _fields_=[('flags',W.DWORD),('in_queue',W.DWORD),('out_queue',W.DWORD)]
class Serial:
    def __init__(self,port,baudrate,timeout=0.02,write_timeout=1):
        self.k=C.WinDLL('kernel32',use_last_error=True)
        self.k.CreateFileW.argtypes=[W.LPCWSTR,W.DWORD,W.DWORD,W.LPVOID,W.DWORD,W.DWORD,W.HANDLE]
        self.k.CreateFileW.restype=W.HANDLE
        for name in ('GetCommState','SetCommState','SetCommTimeouts'):
            getattr(self.k,name).argtypes=[W.HANDLE,W.LPVOID]
        self.k.ReadFile.argtypes=self.k.WriteFile.argtypes=[W.HANDLE,W.LPVOID,W.DWORD,C.POINTER(W.DWORD),W.LPVOID]
        self.k.ClearCommError.argtypes=[W.HANDLE,C.POINTER(W.DWORD),C.POINTER(Stat)]
        self.k.CloseHandle.argtypes=[W.HANDLE]
        self.h=self.k.CreateFileW('\\\\.\\'+port,0xc0000000,0,None,3,0,None)
        if self.h==W.HANDLE(-1).value: raise C.WinError(C.get_last_error())
        try:
            d=DCB();d.length=C.sizeof(d)
            self.check(self.k.GetCommState(self.h,C.byref(d)))
            d.baud=baudrate;d.flags=1;d.byte_size=8;d.parity=0;d.stop_bits=0
            self.check(self.k.SetCommState(self.h,C.byref(d)))
            t=Timeouts(0xffffffff,0,max(1,int(timeout*1000)),0,int(write_timeout*1000))
            self.check(self.k.SetCommTimeouts(self.h,C.byref(t)))
        except Exception: self.close();raise
    def check(self,result):
        if not result: raise C.WinError(C.get_last_error())
    @property
    def in_waiting(self):
        error=W.DWORD();s=Stat();self.check(self.k.ClearCommError(self.h,C.byref(error),C.byref(s)))
        return s.in_queue
    def read(self,n):
        b=C.create_string_buffer(n);count=W.DWORD()
        self.check(self.k.ReadFile(self.h,b,n,C.byref(count),None));return b.raw[:count.value]
    def write(self,data):
        b=C.create_string_buffer(data);count=W.DWORD()
        self.check(self.k.WriteFile(self.h,b,len(data),C.byref(count),None));return count.value
    def flush(self): pass
    def close(self):
        if self.h is not None: self.k.CloseHandle(self.h);self.h=None
    def __enter__(self): return self
    def __exit__(self,*args): self.close()
