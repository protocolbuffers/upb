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
s/upb_fielddef_layout/upb_FieldDef_Layout/g;
s/_upb_fielddef_extlayout/_upb_FieldDef_ExtensionLayout/g;
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
s/upb_msgdef_layout/upb_MessageDef_Layout/g;
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
s/_upb_filedef_publicdepnums/_upb_FileDef_PublicDependencyNumbers/g;
s/_upb_filedef_weakdepnums/_upb_FileDef_WeakDependencyNumbers/g;
s/upb_filedef/upb_FileDef/g;

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
