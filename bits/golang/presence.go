package golang

import "C"

import (
	"unsafe"
)

// LINT.IfChange(presence_logic)

// Message Layout

func oneofCaseOffset(presence int16) int16 {
	if presence < 0 {
		return ^presence
	}
	return -1
}

func hasbitIndex(presence int16) int16 {
	if presence > 0 {
		return presence
	}
	return -1
}

func hasOneofField(msg unsafe.Pointer, caseOffset int16, fieldNum uint32) bool {
	return *(*uint32)(unsafe.Add(msg, caseOffset)) == fieldNum
}

func hasHasbit(msg unsafe.Pointer, idx int16) bool {
	ofs := (idx / 8)
	mask := uint8(1 << (idx % 8))
	return (*(*uint8)(unsafe.Add(msg, ofs)) & mask) != 0
}

// HasField returns true unless this is an optional or oneof field that is not present.
// Notably, proto3 fields without presence will always return true.
func HasField(msg unsafe.Pointer, field *C.struct_upb_MiniTableField) bool {
	presence := FieldPresence(field)
	idx := hasbitIndex(presence)
	if idx != -1 {
		return hasHasbit(msg, idx)
	}
	caseOfs := oneofCaseOffset(presence)
	if caseOfs != -1 {
		return hasOneofField(msg, caseOfs, FieldNumber(field))
	}
	return true
}

// LINT.ThenChange(//depot/google3/third_party/upb/upb/mini_table/field_internal.h:presence_logic)
