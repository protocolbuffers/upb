// Package golang provides direct memory accessors for upb interface. These must be kept in sync
// with the memory layout as defined in C headers.
package golang

import "C"

import (
	"unsafe"
)

// LINT.IfChange(message_raw_fields)

// GetBoolField returns the raw value of a boolean field.
// This function will not return default values properly when the field is not present.
func GetBoolField(msg unsafe.Pointer, ofs uintptr) bool {
	return *(*bool)(unsafe.Add(msg, ofs))
}

// GetUint32Field returns the raw value of a uint32 field.
// This function will not return default values properly when the field is not present.
func GetUint32Field(msg unsafe.Pointer, ofs uintptr) uint32 {
	return *(*uint32)(unsafe.Add(msg, ofs))
}

// GetInt32Field returns the raw value of an int32 field.
// This function will not return default values properly when the field is not present.
func GetInt32Field(msg unsafe.Pointer, ofs uintptr) int32 {
	return *(*int32)(unsafe.Add(msg, ofs))
}

// GetUint64Field returns the raw value of a uint64 field.
// This function will not return default values properly when the field is not present.
func GetUint64Field(msg unsafe.Pointer, ofs uintptr) uint64 {
	return *(*uint64)(unsafe.Add(msg, ofs))
}

// GetInt64Field returns the raw value of an int64 field.
// This function will not return default values properly when the field is not present.
func GetInt64Field(msg unsafe.Pointer, ofs uintptr) int64 {
	return *(*int64)(unsafe.Add(msg, ofs))
}

// GetFloatField returns the raw value of a float field.
// This function will not return default values properly when the field is not present.
func GetFloatField(msg unsafe.Pointer, ofs uintptr) float32 {
	return *(*float32)(unsafe.Add(msg, ofs))
}

// GetDoubleField returns the raw value of a double field.
// This function will not return default values properly when the field is not present.
func GetDoubleField(msg unsafe.Pointer, ofs uintptr) float64 {
	return *(*float64)(unsafe.Add(msg, ofs))
}

// GetStringField returns the raw value of a string field.
// This function will not return default values properly when the field is not present.
func GetStringField(msg unsafe.Pointer, ofs uintptr) string {
	ptr := unsafe.Add(msg, ofs)
	size := *(*uint)(unsafe.Add(msg, ofs+PointerSize))
	return C.GoStringN((*C.char)(ptr), C.int(size))
}

// LINT.ThenChange(//depot/google3/third_party/upb/upb/message/accessors_internal.h:message_raw_fields)
