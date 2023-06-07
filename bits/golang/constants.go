package golang

/*
#include "third_party/upb/upb/mini_table/field_internal.h"
*/
import "C"

import (
	"unsafe"
)

const (
	// PointerSize represents the size in bytes of a pointer in the current machine.
	PointerSize uintptr = unsafe.Sizeof((*byte)(nil))
	// MiniTableFieldSize represents the size in bytes of a upb_MiniTableField struct object.
	MiniTableFieldSize uintptr = unsafe.Sizeof(C.struct_upb_MiniTableField{})
)
