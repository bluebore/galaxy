#!/usr/bin/python

from ctypes import CDLL, byref, CFUNCTYPE, POINTER, Structure, cast, addressof
from ctypes import c_void_p, c_char_p, c_long, c_int, c_bool, py_object
import threading

SDKError = ('OK', 'ClusterDown', 'NoSuchKey', 'Timeout', 'LockFail',
            'CleanBinlogFail', 'UserExists', 'PermissionDenied', 'PasswordError',
            'UnknownUser')
NodeStatus = ('Leader', 'Candidate', 'Follower', 'Offline')
ClusterInfo = ('server_id', 'status', 'term', 'last_log_index', 'last_log_term',
               'commit_index', 'last_applied')

class WatchParam:
    def __init__(self, key, value, deleted, context):
        self.key = key
        self.value = value
        self.deleted = deleted
        self.context = context

class InsSDK:
    class _ClusterNodeInfo(Structure):
        _fields_ = [('server_id', c_char_p),
                    ('status', c_int),
                    ('term', c_long),
                    ('last_log_index', c_long),
                    ('last_log_term', c_long),
                    ('commit_index', c_long),
                    ('last_applied', c_long)]
    class _NodeStatInfo(Structure):
        _fields_ = [('server_id', c_char_p),
                    ('status', c_int),
                    ('stats', c_long * 2 * 8)]
    class _WatchParam(Structure):
        _fields_ = [('key', c_char_p),
                    ('value', c_char_p),
                    ('deleted', c_bool),
                    ('context', c_void_p)]

    WatchCallback = CFUNCTYPE(None, POINTER(_WatchParam), c_long, c_int)
    SessionTimeoutCallback = CFUNCTYPE(None, c_long, c_void_p)

    def __init__(self, members):
        if isinstance(members, basestring):
            self._sdk = _ins.GetSDK(members)
        else:
            self._sdk = _ins.GetSDKFromArray(members)
        self._local = threading.local()
        self._local.errno = c_int()

    def __del__(self):
        if _ins != None:
            _ins.DeleteSDK(self._sdk)

    def error(self):
        return SDKError[self._local.errno]

    def show(self):
        count = c_int()
        cluster_ptr = _ins.SDKShowCluster(self._sdk, byref(count))
        count = count.value
        ClusterArray = self._ClusterNodeInfo * count
        clusters = ClusterArray.from_address(cluster_ptr)
        cluster_list = []
        for i in xrange(count):
            cluster_list.append({
                'server_id' : str(clusters[i].server_id),
                'status' : NodeStatus[clusters[i].status],
                'term' : clusters[i].term,
                'last_log_index' : clusters[i].last_log_index,
                'last_log_term' : clusters[i].last_log_term,
                'commit_index' : clusters[i].commit_index,
                'last_applied' : clusters[i].last_applied
            })
        _ins.DeleteClusterArray(cluster_ptr)
        return cluster_list

    def stat(self):
        count = c_int()
        stat_ptr = _ins.SDKShowStatistics(self._sdk, byref(count))
        count = count.value
        StatArray = self._NodeStatInfo * count
        stats = StatArray.from_address(stat_ptr)
        stat_list = []
        for i in xrange(count):
            stat_list.append({
                'server_id' : str(stats[i].server_id),
                'status' : NodeStatus[stats[i].status],
                'put' : {
                    'current' : int(stats[i].stats[0][0]),
                    'average' : int(stats[i].stats[0][1]),
                },
                'get' : {
                    'current' : int(stats[i].stats[1][0]),
                    'average' : int(stats[i].stats[1][1]),
                },
                'delete' : {
                    'current' : int(stats[i].stats[2][0]),
                    'average' : int(stats[i].stats[2][1]),
                },
                'scan' : {
                    'current' : int(stats[i].stats[3][0]),
                    'average' : int(stats[i].stats[3][1]),
                },
                'keepalive' : {
                    'current' : int(stats[i].stats[4][0]),
                    'average' : int(stats[i].stats[4][1]),
                },
                'lock' : {
                    'current' : int(stats[i].stats[5][0]),
                    'average' : int(stats[i].stats[5][1]),
                },
                'unlock' : {
                    'current' : int(stats[i].stats[6][0]),
                    'average' : int(stats[i].stats[6][1]),
                },
                'watch' : {
                    'current' : int(stats[i].stats[7][0]),
                    'average' : int(stats[i].stats[7][1]),
                }
            })
        _ins.DeleteStatArray(stat_ptr)
        return stat_list

    def get(self, key):
        return _ins.SDKGet(self._sdk, key, byref(self._local.errno))

    def put(self, key, value):
        return _ins.SDKPut(self._sdk, key, value, byref(self._local.errno))

    def delete(self, key):
        return _ins.SDKDelete(self._sdk, key, byref(self._local.errno))

    def scan(self, start_key, end_key):
        self._local.errno = c_int(0)
        return ScanResult(_ins.SDKScan(self._sdk, start_key, end_key))

    def watch(self, key, callback, context):
        ctx = py_object(context)
        self._contexts[addressof(ctx)] = ctx
        self._callback[id(callback)] = callback
        def _watch_wrapper(param, cb, error):
            param = param.contents
            context = cast(c_void_p(param.context), POINTER(py_object)).contents
            pm = WatchParam(param.key, param.value, param.deleted, context.value)
            InsSDK._callback[cb](pm, SDKError[error])
            del InsSDK._callback[cb]
            del InsSDK._contexts[addressof(context)]
        return _ins.SDKWatch(self._sdk, key, self.WatchCallback(_watch_wrapper), \
                             id(callback), byref(ctx), byref(self._local.errno))

    def lock(self, key):
        return _ins.SDKLock(self._sdk, key, byref(self._local.errno))

    def trylock(self, key):
        return _ins.SDKTryLock(self._sdk, key, byref(self._local.errno))

    def unlock(self, key):
        return _ins.SDKUnLock(self._sdk, key, byref(self._local.errno))

    def login(self, username, password):
        return _ins.SDKLogin(self._sdk, username, password, byref(self._local.errno))

    def logout(self):
        return _ins.SDKLogout(self._sdk, byref(self._local.errno))

    def register(self, username, password):
        return _ins.SDKRegister(self._sdk, username, password, byref(self._local.errno))

    def get_session_id(self):
        return _ins.SDKGetSessionID(self._sdk)

    def get_current_user_id(self):
        return _ins.SDKGetCurrentUserID(self._sdk)

    def is_logged_in(self):
        return _ins.SDKIsLoggedIn(self._sdk)

    def register_session_timeout(self, callback, context):
        ctx = py_object(context)
        self._contexts[addressof(context)] = context
        self._callback[id(callback)] = callback
        def _timeout_wrapper(cb, ctx):
            context = cast(ctx, POINTER(py_object)).contents
            InsSDK._callback[cb](context.value)
            del InsSDK._callback[cb]
            del InsSDK._contexts[addressof(context)]
        _ins.SDKRegisterSessionTimeout(self._sdk, self.SessionTimeoutCallback(wrapper), \
                                       id(callback), byref(ctx))

    _contexts = {}
    _callback = {}

# <-- InsSDK class definition ends here

class ScanResult:
    def __init__(self, res_ptr):
        self.scanner = res_ptr

    def __del__(self):
        if _ins != None:
            _ins.DeleteScanResult(self.scanner)

    def __iter__(self):
        return self

    def done(self):
        return _ins.ScanResultDone(self.scanner)

    def error(self):
        return SDKError[_ins.ScanResultError(self.scanner)]

    def key(self):
        return _ins.ScanResultKey(self.scanner)

    def value(self):
        return _ins.ScanResultValue(self.scanner)

    def pair(self):
        return self.key(), self.value()

    def next(self):
        if self.done():
            raise StopIteration()
        else:
            key, value = self.pair()
            _ins.ScanResultNext(self.scanner)
            return key, value

# <-- ScanResult class definition ends here

_ins = CDLL('./libins_py.so')
def _set_function_sign():
    _ins.SDKShowCluster.argtypes = [c_void_p, POINTER(c_int)]
    _ins.SDKShowCluster.restype = c_void_p
    _ins.SDKShowStatistics.argtypes = [c_void_p, POINTER(c_int)]
    _ins.SDKShowStatistics.restype = c_void_p
    _ins.SDKGet.argtypes = [c_void_p, c_char_p, POINTER(c_int)]
    _ins.SDKGet.restype = c_char_p
    _ins.SDKPut.argtypes = [c_void_p, c_char_p, c_char_p, POINTER(c_int)]
    _ins.SDKPut.restype = c_bool
    _ins.SDKDelete.argtypes = [c_void_p, c_char_p, POINTER(c_int)]
    _ins.SDKDelete.restype = c_bool
    _ins.SDKScan.argtypes = [c_void_p, c_char_p, c_char_p]
    _ins.SDKScan.restype = c_void_p
    _ins.SDKWatch.argtypes = [c_void_p, c_char_p, InsSDK.WatchCallback, \
                              c_long, c_void_p, c_void_p]
    _ins.SDKWatch.restype = c_bool
    _ins.SDKLock.argtypes = [c_void_p, c_char_p, POINTER(c_int)]
    _ins.SDKLock.restype = c_bool
    _ins.SDKTryLock.argtypes = [c_void_p, c_char_p, POINTER(c_int)]
    _ins.SDKTryLock.restype = c_bool
    _ins.SDKUnLock.argtypes = [c_void_p, c_char_p, POINTER(c_int)]
    _ins.SDKUnLock.restype = c_bool
    _ins.SDKLogin.argtypes = [c_void_p, c_char_p, c_char_p, POINTER(c_int)]
    _ins.SDKLogin.restype = c_bool
    _ins.SDKLogout.argtypes = [c_void_p, POINTER(c_int)]
    _ins.SDKLogout.restype = c_bool
    _ins.SDKRegister.argtypes = [c_void_p, c_char_p, c_char_p, POINTER(c_int)]
    _ins.SDKRegister.restype = c_bool
    _ins.SDKGetSessionID.argtypes = [c_void_p]
    _ins.SDKGetSessionID.restype = c_char_p
    _ins.SDKGetCurrentUserID.argtypes = [c_void_p]
    _ins.SDKGetCurrentUserID.restype = c_char_p
    _ins.SDKIsLoggedIn.argtypes = [c_void_p]
    _ins.SDKIsLoggedIn.restype = c_bool
    _ins.SDKRegisterSessionTimeout.argtypes = [c_void_p, InsSDK.SessionTimeoutCallback, \
                                               c_long, c_void_p]
    _ins.ScanResultDone.argtypes = [c_void_p]
    _ins.ScanResultDone.restype = c_bool
    _ins.ScanResultError.argtypes = [c_void_p]
    _ins.ScanResultError.restype = c_int
    _ins.ScanResultKey.argtypes = [c_void_p]
    _ins.ScanResultKey.restype = c_char_p
    _ins.ScanResultValue.argtypes = [c_void_p]
    _ins.ScanResultValue.restype = c_char_p
_set_function_sign()

