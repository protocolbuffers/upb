s/upb_fielddef_options/upb_FieldDef_Options/g;
s/upb_fielddef_hasoptions/upb_FieldDef_HasOptions/g;
s/upb_fielddef_fullname/upb_FieldDef_FullName/g;
s/upb_fielddef_type/upb_FieldDef_CType/g;
s/upb_fielddef_descriptortype/upb_FieldDef_Type/g;
s/upb_fielddef_label/upb_FieldDef_Label/g;
s/upb_fielddef_number/upb_FieldDef_Number/g;
s/upb_fielddef_name/upb_FieldDef_Name/g;
s/upb_fielddef_jsonname/upb_FieldDef_JsonName/g;
s/upb_fielddef_hasjsonname/upb_FieldDef_HasJsonName/g;
s/upb_fielddef_isextension/upb_FieldDef_IsExtension/g;
s/upb_fielddef_packed/upb_FieldDef_IsPacked/g;
s/upb_fielddef_file/upb_FieldDef_File/g;
s/upb_fielddef_containingtype/upb_FieldDef_ContainingType/g;
s/upb_fielddef_extensionscope/upb_FieldDef_ExtensionScope/g;
s/upb_fielddef_containingoneof/upb_FieldDef_ContainingOneof/g;
s/upb_fielddef_realcontainingoneof/upb_FieldDef_RealContainingOneof/g;
s/upb_fielddef_index/upb_FieldDef_Index/g;
s/upb_fielddef_issubmsg/upb_FieldDef_IsSubMessage/g;
s/upb_fielddef_isstring/upb_FieldDef_IsString/g;
s/upb_fielddef_isseq/upb_FieldDef_IsRepeated/g;
s/upb_fielddef_isprimitive/upb_FieldDef_IsPrimitive/g;
s/upb_fielddef_ismap/upb_FieldDef_IsMap/g;
s/upb_fielddef_hasdefault/upb_FieldDef_HasDefault/g;
s/upb_fielddef_defaultint64/REPLACE_WITH_UPB_FIELDDEF_DEFAULT/g;
s/upb_fielddef_defaultint32/REPLACE_WITH_UPB_FIELDDEF_DEFAULT/g;
s/upb_fielddef_defaultuint64/REPLACE_WITH_UPB_FIELDDEF_DEFAULT/g;
s/upb_fielddef_defaultuint32/REPLACE_WITH_UPB_FIELDDEF_DEFAULT/g;
s/upb_fielddef_defaultbool/REPLACE_WITH_UPB_FIELDDEF_DEFAULT/g;
s/upb_fielddef_defaultfloat/REPLACE_WITH_UPB_FIELDDEF_DEFAULT/g;
s/upb_fielddef_defaultdouble/REPLACE_WITH_UPB_FIELDDEF_DEFAULT/g;
s/upb_fielddef_defaultstr/REPLACE_WITH_UPB_FIELDDEF_DEFAULT/g;
s/upb_fielddef_hassubdef/upb_FieldDef_HasSubDef/g;
s/upb_fielddef_haspresence/upb_FieldDef_HasPresence/g;
s/upb_fielddef_msgsubdef/upb_FieldDef_MessageSubDef/g;
s/upb_fielddef_enumsubdef/upb_FieldDef_EnumSubDef/g;
s/upb_fielddef_layout/upb_FieldDef_MiniTable/g;
s/upb_FieldDef_Layout/upb_FieldDef_MiniTable/g;
s/_upb_fielddef_extlayout/_upb_FieldDef_ExtensionMiniTable/g;
s/_upb_FieldDef_ExtensionLayout/_upb_FieldDef_ExtensionMiniTable/g;
s/_upb_fielddef_proto3optional/_upb_FieldDef_IsProto3Optional/g;
s/upb_fielddef_default/upb_FieldDef_Default/g;
s/upb_fielddef/upb_FieldDef/g;

s/upb_oneofdef_options/upb_OneofDef_Options/g;
s/upb_oneofdef_hasoptions/upb_OneofDef_HasOptions/g;
s/upb_oneofdef_name/upb_OneofDef_Name/g;
s/upb_oneofdef_containingtype/upb_OneofDef_ContainingType/g;
s/upb_oneofdef_index/upb_OneofDef_Index/g;
s/upb_oneofdef_issynthetic/upb_OneofDef_IsSynthetic/g;
s/upb_oneofdef_fieldcount/upb_OneofDef_FieldCount/g;
s/upb_oneofdef_field/upb_OneofDef_Field/g;
s/upb_oneofdef_ntofz/upb_OneofDef_LookupName/g;
s/upb_oneofdef_ntof/upb_OneofDef_LookupNameWithSize/g;
s/upb_oneofdef_itof/upb_OneofDef_LookupNumber/g;
s/upb_oneofdef/upb_OneofDef/g;

s/upb_msgdef_options/upb_MessageDef_Options/g;
s/upb_msgdef_hasoptions/upb_MessageDef_HasOptions/g;
s/upb_msgdef_fullname/upb_MessageDef_FullName/g;
s/upb_msgdef_file/upb_MessageDef_File/g;
s/upb_msgdef_containingtype/upb_MessageDef_ContainingType/g;
s/upb_msgdef_name/upb_MessageDef_Name/g;
s/upb_msgdef_syntax/upb_MessageDef_Syntax/g;
s/upb_msgdef_wellknowntype/upb_MessageDef_WellKnownType/g;
s/upb_msgdef_extrangecount/upb_MessageDef_ExtensionRangeCount/g;
s/upb_msgdef_fieldcount/upb_MessageDef_FieldCount/g;
s/upb_msgdef_oneofcount/upb_MessageDef_OneofCount/g;
s/upb_msgdef_extrange/upb_MessageDef_ExtensionRange/g;
s/upb_msgdef_field/upb_MessageDef_Field/g;
s/upb_msgdef_oneof/upb_MessageDef_Oneof/g;
s/upb_msgdef_ntooz/upb_MessageDef_FindOneofByName/g;
s/upb_msgdef_ntofz/upb_MessageDef_FindFieldByName/g;
s/upb_msgdef_itof/upb_MessageDef_FindFieldByNumberWithSize/g;
s/upb_msgdef_ntof/upb_MessageDef_FindFieldByNameWithSize/g;
s/upb_msgdef_ntoo/upb_MessageDef_FindOneofByNameWithSize/g;
s/upb_msgdef_layout/upb_MessageDef_MiniTable/g;
s/upb_MessageDef_Layout/upb_MessageDef_MiniTable/g;
s/upb_msgdef_mapentry/upb_MessageDef_IsMapEntry/g;
s/upb_msgdef_nestedmsgcount/upb_MessageDef_NestedMessageCount/g;
s/upb_msgdef_nestedenumcount/upb_MessageDef_NestedEnumCount/g;
s/upb_msgdef_nestedextcount/upb_MessageDef_NestedExtensionCount/g;
s/upb_msgdef_nestedmsg/upb_MessageDef_NestedMessage/g;
s/upb_msgdef_nestedenum/upb_MessageDef_NestedEnum/g;
s/upb_msgdef_nestedext/upb_MessageDef_NestedExtension/g;
s/upb_msgdef_lookupnamez/upb_MessageDef_FindByName/g;
s/upb_msgdef_lookupname/upb_MessageDef_FindByNameWithSize/g;
s/upb_msgdef_lookupjsonnamez/upb_MessageDef_FindByNameName/g;
s/upb_msgdef_lookupjsonname/upb_MessageDef_FindByJsonNameWithSize/g;
s/upb_msgdef/upb_MessageDef/g;
s/WithSizez//g;

s/upb_extrange_options/upb_ExtensionRange_Options/g;
s/upb_extrange_hasoptions/upb_ExtensionRange_HasOptions/g;
s/upb_extrange_start/upb_ExtensionRange_Start/g;
s/upb_extrange_end/upb_ExtensionRange_End/g;
s/upb_extrange/upb_ExtensionRange/g;

s/upb_enumdef_options/upb_EnumDef_Options/g;
s/upb_enumdef_hasoptions/upb_EnumDef_HasOptions/g;
s/upb_enumdef_fullname/upb_EnumDef_FullName/g;
s/upb_enumdef_name/upb_EnumDef_Name/g;
s/upb_enumdef_file/upb_EnumDef_File/g;
s/upb_enumdef_containingtype/upb_EnumDef_ContainingType/g;
s/upb_enumdef_default/upb_EnumDef_Default/g;
s/upb_enumdef_valuecount/upb_EnumDef_ValueCount/g;
s/upb_enumdef_value/upb_EnumDef_Value/g;
s/upb_enumdef_lookupnamez/upb_EnumDef_FindValueByName/g;
s/upb_enumdef_lookupname/upb_EnumDef_FindValueByNameWithSize/g;
s/upb_enumdef_lookupnum/upb_EnumDef_FindValueByNumber/g;
s/upb_enumdef_checknum/upb_EnumDef_CheckNumber/g;
s/upb_enumdef/upb_EnumDef/g;

s/upb_enumvaldef_options/upb_EnumValueDef_Options/g;
s/upb_enumvaldef_hasoptions/upb_EnumValueDef_HasOptions/g;
s/upb_enumvaldef_fullname/upb_EnumValueDef_FullName/g;
s/upb_enumvaldef_name/upb_EnumValueDef_Name/g;
s/upb_enumvaldef_number/upb_EnumValueDef_Number/g;
s/upb_enumvaldef_index/upb_EnumValueDef_Index/g;
s/upb_enumvaldef_enum/upb_EnumValueDef_Enum/g;
s/upb_enumvaldef\b/upb_EnumValueDef/g;

s/upb_filedef_options/upb_FileDef_Options/g;
s/upb_filedef_hasoptions/upb_FileDef_HasOptions/g;
s/upb_filedef_name/upb_FileDef_Name/g;
s/upb_filedef_package/upb_FileDef_Package/g;
s/upb_filedef_syntax/upb_FileDef_Syntax/g;
s/upb_filedef_depcount/upb_FileDef_DependencyCount/g;
s/_upb_filedef_publicdepnums/_upb_FileDef_PublicDependencyNumbers/g;
s/_upb_filedef_weakdepnums/_upb_FileDef_WeakDependencyNumbers/g;
s/upb_filedef_publicdepcount/upb_FileDef_PublicDependencyCount/g;
s/upb_filedef_weakdepcount/upb_FileDef_WeakDependencyCount/g;
s/upb_filedef_toplvlmsgcount/upb_FileDef_TopLevelMessageCount/g;
s/upb_filedef_toplvlenumcount/upb_FileDef_TopLevelEnumCount/g;
s/upb_filedef_toplvlextcount/upb_FileDef_TopLevelExtensionCount/g;
s/upb_filedef_servicecount/upb_FileDef_ServiceCount/g;
s/upb_filedef_dep/upb_FileDef_Dependency/g;
s/upb_filedef_publicdep/upb_FileDef_PublicDependency/g;
s/upb_filedef_weakdep/upb_FileDef_WeakDependency/g;
s/upb_filedef_toplvlmsg/upb_FileDef_TopLevelMessage/g;
s/upb_filedef_toplvlenum/upb_FileDef_TopLevelEnum/g;
s/upb_filedef_toplvlext/upb_FileDef_TopLevelExtension/g;
s/upb_filedef_service/upb_FileDef_Service/g;
s/upb_filedef_symtab/upb_FileDef_Pool/g;
s/upb_filedef/upb_FileDef/g;
s/_upb_FileDef_PublicDependencynums/_upb_FileDef_PublicDependencyIndexes/g;
s/_upb_FileDef_WeakDependencynums/_upb_FileDef_WeakDependencyIndexes/g;

s/upb_methoddef_options/upb_MethodDef_Options/g;
s/upb_methoddef_hasoptions/upb_MethodDef_HasOptions/g;
s/upb_methoddef_fullname/upb_MethodDef_FullName/g;
s/upb_methoddef_name/upb_MethodDef_Name/g;
s/upb_methoddef_service/upb_MethodDef_Service/g;
s/upb_methoddef_inputtype/upb_MethodDef_InputType/g;
s/upb_methoddef_outputtype/upb_MethodDef_OutputType/g;
s/upb_methoddef_clientstreaming/upb_MethodDef_ClientStreaming/g;
s/upb_methoddef_serverstreaming/upb_MethodDef_ServerStreaming/g;
s/upb_methoddef/upb_MethodDef/g;

s/upb_servicedef_options/upb_ServiceDef_Options/g;
s/upb_servicedef_hasoptions/upb_ServiceDef_HasOptions/g;
s/upb_servicedef_fullname/upb_ServiceDef_FullName/g;
s/upb_servicedef_name/upb_ServiceDef_Name/g;
s/upb_servicedef_index/upb_ServiceDef_Index/g;
s/upb_servicedef_file/upb_ServiceDef_File/g;
s/upb_servicedef_methodcount/upb_ServiceDef_MethodCount/g;
s/upb_servicedef_method/upb_ServiceDef_Method/g;
s/upb_servicedef_lookupmethod/upb_ServiceDef_FindMethodByName/g;
s/upb_servicedef/upb_ServiceDef/g;

s/upb_symtab_new/upb_DefPool_New/g;
s/upb_symtab_free/upb_DefPool_Free/g;
s/upb_symtab_lookupmsg2/upb_DefPool_FindMessageByNameWithSize/g;
s/upb_symtab_lookupmsg/upb_DefPool_FindMessageByName/g;
s/upb_symtab_lookupenum/upb_DefPool_FindEnumByName/g;
s/upb_symtab_lookupenumval/upb_DefPool_FindEnumValueByName/g;
s/upb_symtab_lookupext2/upb_DefPool_FindExtensionByNameWithSize/g;
s/upb_symtab_lookupext\b/upb_DefPool_FindExtensionByName/g;
s/upb_symtab_lookupfile2/upb_DefPool_FindFileByNameWithSize/g;
s/upb_symtab_lookupfile/upb_DefPool_FindFileByName/g;
s/upb_symtab_lookupservice/upb_DefPool_FindServiceByName/g;
s/upb_symtab_lookupfileforsym/upb_DefPool_FindFileForSymbol/g;
s/upb_symtab_addfile/upb_DefPool_AddFile/g;
s/_upb_symtab_bytesloaded/_upb_DefPool_BytesLoaded/g;
s/_upb_symtab_arena/_upb_DefPool_Arena/g;
s/_upb_symtab_lookupextfield/_upb_DefPool_FindExtensionField/g;
s/upb_symtab_lookupextbynum/upb_DefPool_FindExtensionByNumber/g;
s/upb_DefPool_FindExtensionByNamebynum/upb_DefPool_FindExtensionByNumber/g;
s/_upb_DefPool_FindExtensionByNamefield/_upb_DefPool_FindExtensionByMiniTable/g;
s/upb_symtab_extreg/upb_DefPool_ExtensionRegistry/g;
s/upb_symtab_getallexts/upb_DefPool_GetAllExtensions/g;
s/upb_symtab/upb_DefPool/g;

s/upb_def_init/_upb_DefPool_Init/g;

s/_upb_DefPool_loaddefinit/_upb_DefPool_LoadDefInit/g;

s/UPB_SYNTAX_PROTO2/kUpb_Syntax_Proto2/g;
s/UPB_SYNTAX_PROTO3/kUpb_Syntax_Proto3/g;
s/upb_syntax_t/upb_Syntax/g;

s/UPB_WELLKNOWN_UNSPECIFIED/kUpb_WellKnown_Unspecified/g;
s/UPB_WELLKNOWN_ANY/kUpb_WellKnown_Any/g;
s/UPB_WELLKNOWN_FIELDMASK/kUpb_WellKnown_FieldMask/g;
s/UPB_WELLKNOWN_DURATION/kUpb_WellKnown_Duration/g;
s/UPB_WELLKNOWN_TIMESTAMP/kUpb_WellKnown_Timestamp/g;
s/UPB_WELLKNOWN_DOUBLEVALUE/kUpb_WellKnown_DoubleValue/g;
s/UPB_WELLKNOWN_FLOATVALUE/kUpb_WellKnown_FloatValue/g;
s/UPB_WELLKNOWN_INT64VALUE/kUpb_WellKnown_Int64Value/g;
s/UPB_WELLKNOWN_UINT64VALUE/kUpb_WellKnown_UInt64Value/g;
s/UPB_WELLKNOWN_INT32VALUE/kUpb_WellKnown_Int32Value/g;
s/UPB_WELLKNOWN_UINT32VALUE/kUpb_WellKnown_UInt32Value/g;
s/UPB_WELLKNOWN_STRINGVALUE/kUpb_WellKnown_StringValue/g;
s/UPB_WELLKNOWN_BYTESVALUE/kUpb_WellKnown_BytesValue/g;
s/UPB_WELLKNOWN_BOOLVALUE/kUpb_WellKnown_BoolValue/g;
s/UPB_WELLKNOWN_VALUE/kUpb_WellKnown_Value/g;
s/UPB_WELLKNOWN_LISTVALUE/kUpb_WellKnown_ListValue/g;
s/UPB_WELLKNOWN_STRUCT/kUpb_WellKnown_Struct/g;
s/upb_wellknowntype_t/upb_WellKnown/g;
s/kUpb_WellKnown/upb_WellKnown/g;
s/upb_WellKnown_/kUpb_WellKnown_/g;

s/UPB_MAX_FIELDNUMBER/kUpb_MaxFieldNumber/g;

s/UPB_MAPENTRY_KEY/kUpb_MapEntry_KeyFieldNumber/g;
s/UPB_MAPENTRY_VALUE/kUpb_MapEntry_ValueFieldNumber/g;
s/UPB_ANY_TYPE/kUpb_Any_TypeFieldNumber/g;
s/UPB_ANY_VALUE/kUpb_Any_ValueFieldNumber/g;
s/UPB_DURATION_SECONDS/kUpb_Duration_SecondsFieldNumber/g;
s/UPB_DURATION_NANOS/kUpb_Duration_NanosFieldNumber/g;
s/UPB_TIMESTAMP_SECONDS/kUpb_Timestamp_SecondsFieldNumber/g;
s/UPB_TIMESTAMP_NANOS/kUpb_Timestamp_NanosFieldNumber/g;

s/upb_status_errmsg/upb_Status_ErrorMessage/g;
s/upb_ok/upb_Status_IsOk/g;
s/upb_status_clear/upb_Status_Clear/g;
s/upb_status_seterrmsg/upb_Status_SetErrorMessage/g;
s/upb_status_seterrf/upb_Status_SetErrorFormat/g;
s/upb_status_vseterrf/upb_Status_VSetErrorFormat/g;
s/upb_status_vappenderrf/upb_Status_VAppendErrorFormat/g;
s/upb_status/upb_Status/g;
s/UPB_STATUS_MAX_MESSAGE/_kUpb_Status_MaxMessage/g;

s/upb_strview_make/upb_StringView_FromStringAndSize/g;
s/upb_strview_makez/upb_StringView_FromCString/g;
s/upb_strview_eql/upb_StringView_IsEqual/g;
s/upb_strview/upb_StringView/g;

s/UPB_STRVIEW_INIT/UPB_STRINGVIEW_INIT/g;
s/UPB_STRVIEW_FORMAT/UPB_STRINGVIEW_FORMAT/g;
s/UPB_STRVIEW_ARGS/UPB_STRINGVIEW_ARGS/g;

s/upb_cleanup_func/upb_CleanupFunc/g;
s/_upb_arena_head/_upb_ArenaHead/g;
s/upb_arena_init/upb_Arena_Init/g;
s/upb_arena_free/upb_Arena_Free/g;
s/upb_arena_addcleanup/upb_Arena_AddCleanup/g;
s/upb_arena_fuse/upb_Arena_Fuse/g;
s/_upb_arena_slowmalloc/_upb_Arena_SlowMalloc/g;
s/upb_arena_alloc/upb_Arena_Alloc/g;
s/_upb_arenahas/_upb_ArenaHas/g;
s/upb_arena_malloc/upb_Arena_Malloc/g;
s/upb_arena_realloc/upb_Arena_Realloc/g;
s/upb_arena_new/upb_Arena_New/g;
s/upb_arena/upb_Arena/g;

s/UPB_WIRE_TYPE_VARINT/kUpb_WireType_Varint/g;
s/UPB_WIRE_TYPE_64BIT/kUpb_WireType_64Bit/g;
s/UPB_WIRE_TYPE_DELIMITED/kUpb_WireType_Delimited/g;
s/UPB_WIRE_TYPE_START_GROUP/kUpb_WireType_StartGroup/g;
s/UPB_WIRE_TYPE_END_GROUP/kUpb_WireType_EndGroup/g;
s/UPB_WIRE_TYPE_32BIT/kUpb_WireType_32Bit/g;
s/upb_wiretype_t/upb_WireType/g;

s/UPB_TYPE_BOOL/kUpb_CType_Bool/g;
s/UPB_TYPE_FLOAT/kUpb_CType_Float/g;
s/UPB_TYPE_INT32/kUpb_CType_Int32/g;
s/UPB_TYPE_UINT32/kUpb_CType_UInt32/g;
s/UPB_TYPE_ENUM/kUpb_CType_Enum/g;
s/UPB_TYPE_MESSAGE/kUpb_CType_Message/g;
s/UPB_TYPE_DOUBLE/kUpb_CType_Double/g;
s/UPB_TYPE_INT64/kUpb_CType_Int64/g;
s/UPB_TYPE_UINT64/kUpb_CType_UInt64/g;
s/UPB_TYPE_STRING/kUpb_CType_String/g;
s/UPB_TYPE_BYTES/kUpb_CType_Bytes/g;
s/upb_fieldtype_t/upb_CType/g;
s/UPB_LABEL_OPTIONAL/kUpb_Label_Optional/g;
s/UPB_LABEL_REQUIRED/kUpb_Label_Required/g;
s/UPB_LABEL_REPEATED/kUpb_Label_Repeated/g;
s/upb_label_t/upb_Label/g;
s/UPB_DESCRIPTOR_TYPE_DOUBLE/upb_FieldType_Double/g;
s/UPB_DESCRIPTOR_TYPE_FLOAT/upb_FieldType_Float/g;
s/UPB_DESCRIPTOR_TYPE_INT64/upb_FieldType_Int64/g;
s/UPB_DESCRIPTOR_TYPE_UINT64/upb_FieldType_UInt64/g;
s/UPB_DESCRIPTOR_TYPE_INT32/upb_FieldType_Int32/g;
s/UPB_DESCRIPTOR_TYPE_FIXED64/upb_FieldType_Fixed64/g;
s/UPB_DESCRIPTOR_TYPE_FIXED32/upb_FieldType_Fixed32/g;
s/UPB_DESCRIPTOR_TYPE_BOOL/upb_FieldType_Bool/g;
s/UPB_DESCRIPTOR_TYPE_STRING/upb_FieldType_String/g;
s/UPB_DESCRIPTOR_TYPE_GROUP/upb_FieldType_Group/g;
s/UPB_DESCRIPTOR_TYPE_MESSAGE/upb_FieldType_Message/g;
s/UPB_DESCRIPTOR_TYPE_BYTES/upb_FieldType_Bytes/g;
s/UPB_DESCRIPTOR_TYPE_UINT32/upb_FieldType_UInt32/g;
s/UPB_DESCRIPTOR_TYPE_ENUM/upb_FieldType_Enum/g;
s/UPB_DESCRIPTOR_TYPE_SFIXED32/upb_FieldType_SFixed32/g;
s/UPB_DESCRIPTOR_TYPE_SFIXED64/upb_FieldType_SFixed64/g;
s/UPB_DESCRIPTOR_TYPE_SINT32/upb_FieldType_SInt32/g;
s/UPB_DESCRIPTOR_TYPE_SINT64/upb_FieldType_SInt64/g;
s/UPB_DTYPE_DOUBLE/upb_FieldType_Double/g;
s/UPB_DTYPE_FLOAT/upb_FieldType_Float/g;
s/UPB_DTYPE_INT64/upb_FieldType_Int64/g;
s/UPB_DTYPE_UINT64/upb_FieldType_UInt64/g;
s/UPB_DTYPE_INT32/upb_FieldType_Int32/g;
s/UPB_DTYPE_FIXED64/upb_FieldType_Fixed64/g;
s/UPB_DTYPE_FIXED32/upb_FieldType_Fixed32/g;
s/UPB_DTYPE_BOOL/upb_FieldType_Bool/g;
s/UPB_DTYPE_STRING/upb_FieldType_String/g;
s/UPB_DTYPE_GROUP/upb_FieldType_Group/g;
s/UPB_DTYPE_MESSAGE/upb_FieldType_Message/g;
s/UPB_DTYPE_BYTES/upb_FieldType_Bytes/g;
s/UPB_DTYPE_UINT32/upb_FieldType_UInt32/g;
s/UPB_DTYPE_ENUM/upb_FieldType_Enum/g;
s/UPB_DTYPE_SFIXED32/upb_FieldType_SFixed32/g;
s/UPB_DTYPE_SFIXED64/upb_FieldType_SFixed64/g;
s/UPB_DTYPE_SINT32/upb_FieldType_SInt32/g;
s/UPB_DTYPE_SINT64/upb_FieldType_SInt64/g;
s/upb_descriptortype_t/upb_FieldType/g;
s/UPB_MAP_BEGIN/kUpb_Map_Begin/g;
s/_upb_isle/_upb_IsLittleEndian/g;
s/_upb_be_swap32/_upb_BigEndian_Swap32/g;
s/_upb_be_swap64/_upb_BigEndian_Swap64/g;
s/_upb_lg2ceil/_upb_Log2Ceiling/g;
s/_upb_lg2ceilsize/_upb_Log2CeilingSize/g;

s/upb_msgval/upb_MessageValue/g;
s/upb_mutmsgval/upb_MutableMessageValue/g;
s/upb_msg_new/upb_Message_New/g;
s/upb_msg_get/upb_Message_Get/g;
s/upb_msg_mutable/upb_Message_Mutable/g;
s/upb_msg_has/upb_Message_Has/g;
s/upb_msg_whichoneof/upb_Message_WhichOneof/g;
s/upb_msg_set/upb_Message_Set/g;
s/upb_msg_clearfield/upb_Message_ClearField/g;
s/upb_msg_clear/upb_Message_Clear/g;
s/UPB_MSG_BEGIN/kUpb_Message_Begin/g;
s/upb_msg_next/upb_Message_Next/g;
s/upb_msg_discardunknown/upb_Message_DiscardUnknown/g;
s/upb_array_new/upb_Array_New/g;
s/upb_array_size/upb_Array_Size/g;
s/upb_array_get/upb_Array_Get/g;
s/upb_array_set/upb_Array_Set/g;
s/upb_array_append/upb_Array_Append/g;
s/upb_array_move/upb_Array_Move/g;
s/upb_array_insert/upb_Array_Insert/g;
s/upb_array_delete/upb_Array_Delete/g;
s/upb_array_resize/upb_Array_Resize/g;
s/upb_map_new/upb_Map_New/g;
s/upb_map_size/upb_Map_Size/g;
s/upb_map_get/upb_Map_Get/g;
s/upb_map_clear/upb_Map_Clear/g;
s/upb_map_set/upb_Map_Set/g;
s/upb_map_delete/upb_Map_Delete/g;
s/upb_mapiter_next/upb_MapIterator_Next/g;
s/upb_mapiter_done/upb_MapIterator_Done/g;
s/upb_mapiter_key/upb_MapIterator_Key/g;
s/upb_mapiter_value/upb_MapIterator_Value/g;
s/upb_mapiter_setvalue/upb_MapIterator_SetValue/g;

s/UPB_ENCODE_DETERMINISTIC/kUpb_Encode_Deterministic/g;
s/UPB_ENCODE_SKIPUNKNOWN/kUpb_Encode_SkipUnknown/g;
s/UPB_ENCODE_CHECKREQUIRED/kUpb_Encode_CheckRequired/g;
s/upb_encode_ex/upb_EncodeEx/g;
s/upb_encode/upb_Encode/g;

s/UPB_JSONDEC_IGNOREUNKNOWN/upb_JsonDecode_IgnoreUnknown/g;
s/upb_json_decode/upb_JsonDecode/g;

s/UPB_JSONENC_EMITDEFAULTS/upb_JsonEncode_EmitDefaults/g;
s/UPB_JSONENC_PROTONAMES/upb_JsonEncode_UseProtoNames/g;
s/upb_json_encode/upb_JsonEncode/g;

s/\bupb_msglayout_field\b/upb_MiniTable_Field/g;
s/_UPB_MODE_MAP/kUpb_FieldMode_Map/g;
s/_UPB_MODE_ARRAY/kUpb_FieldMode_Array/g;
s/_UPB_MODE_SCALAR/kUpb_FieldMode_Scalar/g;
s/_UPB_MODE_MASK/kUpb_FieldMode_Mask/g;
s/\bupb_fieldmode\b/upb_FieldMode/g;
s/upb_labelflags/upb_LabelFlags/g;
s/_UPB_MODE_IS_PACKED/upb_LabelFlags_IsPacked/g;
s/_UPB_MODE_IS_EXTENSION/upb_LabelFlags_IsExtension/g;
s/\bupb_rep\b/upb_FieldRep/g;
s/_UPB_REP_1BYTE/upb_FieldRep_1Byte/g;
s/_UPB_REP_4BYTE/upb_FieldRep_4Byte/g;
s/_UPB_REP_8BYTE/upb_FieldRep_8Byte/g;
s/_UPB_REP_STRVIEW/upb_FieldRep_StringView/g;
s/_UPB_REP_PTR/upb_FieldRep_Pointer/g;
s/_UPB_REP_SHIFT/upb_FieldRep_Shift/g;
s/upb_fieldmode/upb_FieldMode/g;
s/_upb_getmode/upb_FieldMode_Get/g;
s/_upb_repeated_or_map/upb_IsRepeatedOrMap/g;
s/_upb_issubmsg/upb_IsSubMessage/g;
s/upb_decstate/upb_Decoder/g;
s/upb_msglayout/upb_MiniTable/g;
s/_upb_field_parser/_upb_FieldParser/g;
s/_upb_fasttable_entry/_upb_FastTable_Entry/g;
s/\bupb_enumlayout\b/upb_MiniTable_Enum/g;
s/_upb_enumlayout_checkval/upb_MiniTable_Enum_CheckValue/g;
s/\bupb_msglayout_sub\b/upb_MiniTable_Sub/g;
s/_UPB_MSGEXT_NONE/upb_ExtMode_NonExtendable/g;
s/_UPB_MSGEXT_EXTENDABLE/upb_ExtMode_Extendable/g;
s/_UPB_MSGEXT_MSGSET/upb_ExtMode_IsMessageSet/g;
s/_UPB_MSGEXT_MSGSET_ITEM/upb_ExtMode_IsMessageSetItem/g;
s/\bupb_msgext_mode\b/upb_ExtMode/g;
#_UPB_MSGSET_ITEM = 1,
#_UPB_MSGSET_TYPEID = 2,
#_UPB_MSGSET_MESSAGE = 3,
#s/\bupb_msgext_fieldnum\b/upb_MSetFieldNum
s/upb_msglayout_ext/upb_MiniTable_Extension/g;
s/upb_msglayout_file/upb_MiniTable_File/g;
s/upb_msglayout_requiredmask/upb_MiniTable_RequiredMask/g;
#_upb_extreg_add(upb_extreg *r, const upb_msglayout_ext **e, size_t count);
#upb_msglayout_ext *_upb_extreg_get(const upb_extreg *r,
s/\bupb_msg_internaldata\b/upb_Message_InternalData/g;
s/\bupb_msg_internal\b/upb_Message_Internal/g;
#_upb_CTypeo_size[12];
#upb_msg_sizeof(const upb_msglayout *l) {
#_upb_Message_New_inl(const upb_msglayout *l, upb_Arena *a) {
#_upb_Message_New(const upb_msglayout *l, upb_Arena *a);
#upb_Message_Getinternal(upb_msg *msg) {
#_upb_Message_Clear(upb_msg *msg, const upb_msglayout *l);
#_upb_Message_DiscardUnknown_shallow(upb_msg *msg);
#_upb_msg_addunknown(upb_msg *msg, const char *data, size_t len,
s/\bupb_msg_ext\b/upb_Message_Extension/g;
#_upb_Message_Getorcreateext(upb_msg *msg, const upb_msglayout_ext *ext,
#_upb_Message_Getexts(const upb_msg *msg, size_t *count);
#_upb_Message_Getext(const upb_msg *msg,
#_upb_Message_Clearext(upb_msg *msg, const upb_msglayout_ext *ext);
#_upb_Message_Clearext(upb_msg *msg, const upb_msglayout_ext *ext);
#_upb_hasbit(const upb_msg *msg, size_t idx) {
#_upb_sethas(const upb_msg *msg, size_t idx) {
#_upb_clearhas(const upb_msg *msg, size_t idx) {
#_upb_Message_Hasidx(const upb_msglayout_field *f) {
#_upb_hasbit_field(const upb_msg *msg,
#_upb_sethas_field(const upb_msg *msg,
#_upb_clearhas_field(const upb_msg *msg,
#_upb_oneofcase(upb_msg *msg, size_t case_ofs) {
#_upb_getoneofcase(const void *msg, size_t case_ofs) {
#_upb_oneofcase_ofs(const upb_msglayout_field *f) {
#_upb_oneofcase_field(upb_msg *msg,
#_upb_getoneofcase_field(const upb_msg *msg,
#_upb_has_submsg_nohasbit(const upb_msg *msg, size_t ofs) {
s/\bupb_array\b/upb_Array/g;
#_upb_array_constptr(const upb_array *arr) {
#_upb_array_tagptr(void* ptr, int elem_size_lg2) {
#_upb_array_ptr(upb_array *arr) {
#_upb_tag_arrptr(void* ptr, int elem_size_lg2) {
#_upb_Array_New(upb_Arena *a, size_t init_size,
#_upb_array_realloc(upb_array *arr, size_t min_size, upb_Arena *arena);
#_upb_Array_Resize_fallback(upb_array **arr_ptr, size_t size,
#_upb_Array_Append_fallback(upb_array **arr_ptr, const void *value,
#_upb_array_reserve(upb_array *arr, size_t size,
#_upb_Array_Resize(upb_array *arr, size_t size,
#_upb_array_accessor(const void *msg, size_t ofs,
#_upb_array_mutable_accessor(void *msg, size_t ofs,
#_upb_Array_Resize_accessor2(void *msg, size_t ofs, size_t size,
#_upb_Array_Append_accessor2(void *msg, size_t ofs,
#_upb_sizelg2(upb_CType type) {
#_upb_Array_Resize_accessor(void *msg, size_t ofs, size_t size,
#_upb_Array_Append_accessor(void *msg, size_t ofs,
s/\bupb_map\b/upb_Map/g;
s/\bupb_map_entry\b/upb_MapEntry/g;
#_upb_Map_New(upb_Arena *a, size_t key_size, size_t value_size);
#_upb_map_tokey(const void *key, size_t size) {
#_upb_map_fromkey(upb_StringView key, void* out, size_t size) {
#_upb_map_tovalue(const void *val, size_t size, upb_value *msgval,
#_upb_map_fromvalue(upb_value val, void* out, size_t size) {
#_upb_Map_Size(const upb_map *map) {
#_upb_Map_Get(const upb_map *map, const void *key,
#_upb_map_next(const upb_map *map, size_t *iter) {
#_upb_Map_Set(upb_map *map, const void *key, size_t key_size,
#_upb_Map_Delete(upb_map *map, const void *key, size_t key_size) {
#_upb_Map_Clear(upb_map *map) {
#_upb_msg_map_size(const upb_msg *msg, size_t ofs) {
#_upb_msg_map_get(const upb_msg *msg, size_t ofs,
#_upb_msg_map_next(const upb_msg *msg, size_t ofs,
#_upb_msg_map_set(upb_msg *msg, size_t ofs, const void *key,
#_upb_msg_map_delete(upb_msg *msg, size_t ofs, const void *key,
#_upb_msg_map_clear(upb_msg *msg, size_t ofs) {
#_upb_msg_map_key(const void* msg, void* key, size_t size) {
#_upb_msg_map_value(const void* msg, void* val, size_t size) {
#_upb_msg_map_set_value(void* msg, const void* val, size_t size) {
#_upb_mapsorter;
#_upb_sortedmap;
#_upb_mapsorter_init(_upb_mapsorter *s) {
#_upb_mapsorter_destroy(_upb_mapsorter *s) {
#_upb_mapsorter_pushmap(_upb_mapsorter *s, upb_FieldType key_type,
#_upb_mapsorter_popmap(_upb_mapsorter *s, _upb_sortedmap *sorted) {
#_upb_sortedmap_next(_upb_mapsorter *s, const upb_map *map,
s/\bupb_msglayout\b/upb_MiniTable/g;

s/upb_MiniTable_ext/upb_MiniTable_Extension/g;
s/upb_MiniTable_file/upb_MiniTable_File/g;
s/upb_MiniTable_sub/upb_MiniTable_Sub/g;

s/\bLookupMessage\b/FindMessageByName/g;
s/\bLookupEnum\b/FindEnumByName/g;
s/\bLookupFile\b/FindFileByName/g;
