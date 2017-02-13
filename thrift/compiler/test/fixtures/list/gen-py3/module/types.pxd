#
# Autogenerated by Thrift
#
# DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
#  @generated
#

from libcpp.string cimport string
from libcpp cimport bool as cbool
from cpython cimport bool as pbool
from libc.stdint cimport int8_t, int16_t, int32_t, int64_t
from libcpp.memory cimport shared_ptr, unique_ptr
from libcpp.vector cimport vector
from libcpp.set cimport set as cset
from libcpp.map cimport map as cmap, pair as cpair
from thrift.py3.exceptions cimport cTException
cimport thrift.py3.types








cdef class List__string:
    cdef object __hash
    cdef object __weakref__
    cdef shared_ptr[vector[string]] _vector
    @staticmethod
    cdef create(shared_ptr[vector[string]])

cdef class Map__i64_List__string:
    cdef object __hash
    cdef object __weakref__
    cdef shared_ptr[cmap[int64_t,vector[string]]] _map
    @staticmethod
    cdef create(shared_ptr[cmap[int64_t,vector[string]]])

cdef extern from "<utility>" namespace "std" nogil:
    cdef shared_ptr[vector[string]] move(unique_ptr[vector[string]])
    cdef shared_ptr[cmap[int64_t,vector[string]]] move(unique_ptr[cmap[int64_t,vector[string]]])

cdef extern from "src/gen-cpp2/module_constants.h" namespace "cpp2":
    cdef cmap[int64_t,vector[string]] cTEST_MAP "cpp2::module_constants::TEST_MAP"()
