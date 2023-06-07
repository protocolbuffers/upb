package golang

/*
#include "third_party/upb/upb/mini_table/field_internal.h"
*/
import "C"

import (
	"unsafe"
)

// FieldAtIndex indexes into an array of upb_MiniTableField*. It returns the equivalent of
// (fields + idx).
func FieldAtIndex(fields unsafe.Pointer, idx uintptr) unsafe.Pointer {
	return unsafe.Add(fields, idx*MiniTableFieldSize)
}

// FieldNumber returns upb_MiniTableField.number for a upb_MiniTableField* pointer.
func FieldNumber(field *C.struct_upb_MiniTableField) uint32 {
	return uint32(field.number)
}

// FieldOffset returns upb_MiniTableField.offset for a upb_MiniTableField* pointer.
func FieldOffset(field *C.struct_upb_MiniTableField) uint16 {
	return uint16(field.offset)
}

// FieldPresence returns upb_MiniTableField.presence for a upb_MiniTableField* pointer.
func FieldPresence(field *C.struct_upb_MiniTableField) int16 {
	return int16(field.presence)
}

// FieldMode returns upb_MiniTableField.mode for a upb_MiniTableField* pointer.
func FieldMode(field *C.struct_upb_MiniTableField) uint8 {
	return uint8(field.mode)
}
