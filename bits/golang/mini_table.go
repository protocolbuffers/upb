package golang

/*
#include "third_party/upb/upb/mini_table/message_internal.h"
*/
import "C"

import (
	"unsafe"
)

// FieldArray returns upb_MiniTable.fields for a upb_MiniTable* pointer.
func FieldArray(miniTable *C.struct_upb_MiniTable) *C.struct_upb_MiniTableField {
	return (*C.struct_upb_MiniTableField)(unsafe.Pointer(miniTable.fields))
}

// FieldCount returns upb_MiniTable.field_count for a upb_MiniTable* pointer.
func FieldCount(miniTable *C.struct_upb_MiniTable) uint16 {
	return uint16(miniTable.field_count)
}
