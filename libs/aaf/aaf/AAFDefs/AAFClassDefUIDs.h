#ifndef __AAFClassDefUIDs_h__
#define __AAFClassDefUIDs_h__

#include "aaf/AAFTypes.h"

// AAF class definition UIDs.
//

// The AAF reference implementation uses shorter names than
// SMPTE. The names are shortened by the following aliases.
//
#define AAFClassID_ClassDef             AAFClassID_ClassDefinition
#define AAFClassID_CodecDef             AAFClassID_CodecDefinition
#define AAFClassID_DataDef              AAFClassID_DataDefinition
#define AAFClassID_DefObject            AAFClassID_DefinitionObject
#define AAFClassID_Edgecode             AAFClassID_EdgeCode
#define AAFClassID_OperationDef         AAFClassID_OperationDefinition
#define AAFClassID_Object               AAFClassID_InterchangeObject
#define AAFClassID_ParameterDef         AAFClassID_ParameterDefinition
#define AAFClassID_InterpolationDef     AAFClassID_InterpolationDefinition
#define AAFClassID_PropertyDef          AAFClassID_PropertyDefinition
#define AAFClassID_TypeDef              AAFClassID_TypeDefinition
#define AAFClassID_TypeDefCharacter     AAFClassID_TypeDefinitionCharacter
#define AAFClassID_TypeDefEnum          AAFClassID_TypeDefinitionEnumeration
#define AAFClassID_TypeDefExtEnum       AAFClassID_TypeDefinitionExtendibleEnumeration
#define AAFClassID_TypeDefFixedArray    AAFClassID_TypeDefinitionFixedArray
#define AAFClassID_TypeDefInt           AAFClassID_TypeDefinitionInteger
#define AAFClassID_TypeDefRecord        AAFClassID_TypeDefinitionRecord
#define AAFClassID_TypeDefRename        AAFClassID_TypeDefinitionRename
#define AAFClassID_TypeDefSet           AAFClassID_TypeDefinitionSet
#define AAFClassID_TypeDefStream        AAFClassID_TypeDefinitionStream
#define AAFClassID_TypeDefString        AAFClassID_TypeDefinitionString
#define AAFClassID_TypeDefIndirect      AAFClassID_TypeDefinitionIndirect
#define AAFClassID_TypeDefOpaque        AAFClassID_TypeDefinitionOpaque
#define AAFClassID_TypeDefStrongObjRef  AAFClassID_TypeDefinitionStrongObjectReference
#define AAFClassID_TypeDefVariableArray AAFClassID_TypeDefinitionVariableArray
#define AAFClassID_TypeDefWeakObjRef    AAFClassID_TypeDefinitionWeakObjectReference
#define AAFClassID_ContainerDef         AAFClassID_ContainerDefinition
#define AAFClassID_PluginDef            AAFClassID_PluginDefinition




// https://github.com/nevali/aaf/blob/a03404ad8dc371757f3847b8dae0a9d70fff6a4e/ref-impl/include/OM/OMDictionary.h
static const aafUID_t AAFClassID_Root =
{0xb3b398a5, 0x1c90, 0x11d4, {0x80, 0x53, 0x08, 0x00, 0x36, 0x21, 0x08, 0x04}};





//{0d010101-0101-0100-060e-2b3402060101}
static const aafUID_t AAFClassID_InterchangeObject =
{0x0d010101, 0x0101, 0x0100, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-0200-060e-2b3402060101}
static const aafUID_t AAFClassID_Component =
{0x0d010101, 0x0101, 0x0200, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-0300-060e-2b3402060101}
static const aafUID_t AAFClassID_Segment =
{0x0d010101, 0x0101, 0x0300, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-0400-060e-2b3402060101}
static const aafUID_t AAFClassID_EdgeCode =
{0x0d010101, 0x0101, 0x0400, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-0500-060e-2b3402060101}
static const aafUID_t AAFClassID_EssenceGroup =
{0x0d010101, 0x0101, 0x0500, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-0600-060e-2b3402060101}
static const aafUID_t AAFClassID_Event =
{0x0d010101, 0x0101, 0x0600, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-0700-060e-2b3402060101}
static const aafUID_t AAFClassID_GPITrigger =
{0x0d010101, 0x0101, 0x0700, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-0800-060e-2b3402060101}
static const aafUID_t AAFClassID_CommentMarker =
{0x0d010101, 0x0101, 0x0800, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-0900-060e-2b3402060101}
static const aafUID_t AAFClassID_Filler =
{0x0d010101, 0x0101, 0x0900, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-0a00-060e-2b3402060101}
static const aafUID_t AAFClassID_OperationGroup =
{0x0d010101, 0x0101, 0x0a00, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-0b00-060e-2b3402060101}
static const aafUID_t AAFClassID_NestedScope =
{0x0d010101, 0x0101, 0x0b00, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-0c00-060e-2b3402060101}
static const aafUID_t AAFClassID_Pulldown =
{0x0d010101, 0x0101, 0x0c00, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-0d00-060e-2b3402060101}
static const aafUID_t AAFClassID_ScopeReference =
{0x0d010101, 0x0101, 0x0d00, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-0e00-060e-2b3402060101}
static const aafUID_t AAFClassID_Selector =
{0x0d010101, 0x0101, 0x0e00, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-0f00-060e-2b3402060101}
static const aafUID_t AAFClassID_Sequence =
{0x0d010101, 0x0101, 0x0f00, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-1000-060e-2b3402060101}
static const aafUID_t AAFClassID_SourceReference =
{0x0d010101, 0x0101, 0x1000, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-1100-060e-2b3402060101}
static const aafUID_t AAFClassID_SourceClip =
{0x0d010101, 0x0101, 0x1100, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-1200-060e-2b3402060101}
static const aafUID_t AAFClassID_TextClip =
{0x0d010101, 0x0101, 0x1200, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-1300-060e-2b3402060101}
static const aafUID_t AAFClassID_HTMLClip =
{0x0d010101, 0x0101, 0x1300, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-1400-060e-2b3402060101}
static const aafUID_t AAFClassID_Timecode =
{0x0d010101, 0x0101, 0x1400, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-1500-060e-2b3402060101}
static const aafUID_t AAFClassID_TimecodeStream =
{0x0d010101, 0x0101, 0x1500, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-1600-060e-2b3402060101}
static const aafUID_t AAFClassID_TimecodeStream12M =
{0x0d010101, 0x0101, 0x1600, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-1700-060e-2b3402060101}
static const aafUID_t AAFClassID_Transition =
{0x0d010101, 0x0101, 0x1700, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-1800-060e-2b3402060101}
static const aafUID_t AAFClassID_ContentStorage =
{0x0d010101, 0x0101, 0x1800, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-1900-060e-2b3402060101}
static const aafUID_t AAFClassID_ControlPoint =
{0x0d010101, 0x0101, 0x1900, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-1a00-060e-2b3402060101}
static const aafUID_t AAFClassID_DefinitionObject =
{0x0d010101, 0x0101, 0x1a00, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-1b00-060e-2b3402060101}
static const aafUID_t AAFClassID_DataDefinition =
{0x0d010101, 0x0101, 0x1b00, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-1c00-060e-2b3402060101}
static const aafUID_t AAFClassID_OperationDefinition =
{0x0d010101, 0x0101, 0x1c00, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-1d00-060e-2b3402060101}
static const aafUID_t AAFClassID_ParameterDefinition =
{0x0d010101, 0x0101, 0x1d00, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-1e00-060e-2b3402060101}
static const aafUID_t AAFClassID_PluginDefinition =
{0x0d010101, 0x0101, 0x1e00, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-1f00-060e-2b3402060101}
static const aafUID_t AAFClassID_CodecDefinition =
{0x0d010101, 0x0101, 0x1f00, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-2000-060e-2b3402060101}
static const aafUID_t AAFClassID_ContainerDefinition =
{0x0d010101, 0x0101, 0x2000, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-2100-060e-2b3402060101}
static const aafUID_t AAFClassID_InterpolationDefinition =
{0x0d010101, 0x0101, 0x2100, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-2200-060e-2b3402060101}
static const aafUID_t AAFClassID_Dictionary =
{0x0d010101, 0x0101, 0x2200, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-2300-060e-2b3402060101}
static const aafUID_t AAFClassID_EssenceData =
{0x0d010101, 0x0101, 0x2300, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-2400-060e-2b3402060101}
static const aafUID_t AAFClassID_EssenceDescriptor =
{0x0d010101, 0x0101, 0x2400, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-2500-060e-2b3402060101}
static const aafUID_t AAFClassID_FileDescriptor =
{0x0d010101, 0x0101, 0x2500, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-2600-060e-2b3402060101}
static const aafUID_t AAFClassID_AIFCDescriptor =
{0x0d010101, 0x0101, 0x2600, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-2700-060e-2b3402060101}
static const aafUID_t AAFClassID_DigitalImageDescriptor =
{0x0d010101, 0x0101, 0x2700, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-2800-060e-2b3402060101}
static const aafUID_t AAFClassID_CDCIDescriptor =
{0x0d010101, 0x0101, 0x2800, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-2900-060e-2b3402060101}
static const aafUID_t AAFClassID_RGBADescriptor =
{0x0d010101, 0x0101, 0x2900, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-2a00-060e-2b3402060101}
static const aafUID_t AAFClassID_HTMLDescriptor =
{0x0d010101, 0x0101, 0x2a00, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-2b00-060e-2b3402060101}
static const aafUID_t AAFClassID_TIFFDescriptor =
{0x0d010101, 0x0101, 0x2b00, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-2c00-060e-2b3402060101}
static const aafUID_t AAFClassID_WAVEDescriptor =
{0x0d010101, 0x0101, 0x2c00, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-2d00-060e-2b3402060101}
static const aafUID_t AAFClassID_FilmDescriptor =
{0x0d010101, 0x0101, 0x2d00, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-2e00-060e-2b3402060101}
static const aafUID_t AAFClassID_TapeDescriptor =
{0x0d010101, 0x0101, 0x2e00, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-2f00-060e-2b3402060101}
static const aafUID_t AAFClassID_Header =
{0x0d010101, 0x0101, 0x2f00, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-3000-060e-2b3402060101}
static const aafUID_t AAFClassID_Identification =
{0x0d010101, 0x0101, 0x3000, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-3100-060e-2b3402060101}
static const aafUID_t AAFClassID_Locator =
{0x0d010101, 0x0101, 0x3100, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-3200-060e-2b3402060101}
static const aafUID_t AAFClassID_NetworkLocator =
{0x0d010101, 0x0101, 0x3200, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-3300-060e-2b3402060101}
static const aafUID_t AAFClassID_TextLocator =
{0x0d010101, 0x0101, 0x3300, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-3400-060e-2b3402060101}
static const aafUID_t AAFClassID_Mob =
{0x0d010101, 0x0101, 0x3400, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-3500-060e-2b3402060101}
static const aafUID_t AAFClassID_CompositionMob =
{0x0d010101, 0x0101, 0x3500, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-3600-060e-2b3402060101}
static const aafUID_t AAFClassID_MasterMob =
{0x0d010101, 0x0101, 0x3600, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-3700-060e-2b3402060101}
static const aafUID_t AAFClassID_SourceMob =
{0x0d010101, 0x0101, 0x3700, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-3800-060e-2b3402060101}
static const aafUID_t AAFClassID_MobSlot =
{0x0d010101, 0x0101, 0x3800, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-3900-060e-2b3402060101}
static const aafUID_t AAFClassID_EventMobSlot =
{0x0d010101, 0x0101, 0x3900, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-3a00-060e-2b3402060101}
static const aafUID_t AAFClassID_StaticMobSlot =
{0x0d010101, 0x0101, 0x3a00, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-3b00-060e-2b3402060101}
static const aafUID_t AAFClassID_TimelineMobSlot =
{0x0d010101, 0x0101, 0x3b00, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-3c00-060e-2b3402060101}
static const aafUID_t AAFClassID_Parameter =
{0x0d010101, 0x0101, 0x3c00, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-3d00-060e-2b3402060101}
static const aafUID_t AAFClassID_ConstantValue =
{0x0d010101, 0x0101, 0x3d00, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-3e00-060e-2b3402060101}
static const aafUID_t AAFClassID_VaryingValue =
{0x0d010101, 0x0101, 0x3e00, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-3f00-060e-2b3402060101}
static const aafUID_t AAFClassID_TaggedValue =
{0x0d010101, 0x0101, 0x3f00, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-4000-060e-2b3402060101}
static const aafUID_t AAFClassID_KLVData =
{0x0d010101, 0x0101, 0x4000, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-4100-060e-2b3402060101}
static const aafUID_t AAFClassID_DescriptiveMarker =
{0x0d010101, 0x0101, 0x4100, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-4200-060e-2b3402060101}
static const aafUID_t AAFClassID_SoundDescriptor =
{0x0d010101, 0x0101, 0x4200, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-4300-060e-2b3402060101}
static const aafUID_t AAFClassID_DataEssenceDescriptor =
{0x0d010101, 0x0101, 0x4300, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-4400-060e-2b3402060101}
static const aafUID_t AAFClassID_MultipleDescriptor =
{0x0d010101, 0x0101, 0x4400, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-4500-060e-2b3402060101}
static const aafUID_t AAFClassID_DescriptiveClip =
{0x0d010101, 0x0101, 0x4500, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-4700-060e-2b3402060101}
static const aafUID_t AAFClassID_AES3PCMDescriptor =
{0x0d010101, 0x0101, 0x4700, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-4800-060e-2b3402060101}
static const aafUID_t AAFClassID_PCMDescriptor =
{0x0d010101, 0x0101, 0x4800, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-4900-060e-2b3402060101}
static const aafUID_t AAFClassID_PhysicalDescriptor =
{0x0d010101, 0x0101, 0x4900, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-4a00-060e-2b3402060101}
static const aafUID_t AAFClassID_ImportDescriptor =
{0x0d010101, 0x0101, 0x4a00, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-4b00-060e-2b3402060101}
static const aafUID_t AAFClassID_RecordingDescriptor =
{0x0d010101, 0x0101, 0x4b00, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-4c00-060e-2b3402060101}
static const aafUID_t AAFClassID_TaggedValueDefinition =
{0x0d010101, 0x0101, 0x4c00, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-4d00-060e-2b3402060101}
static const aafUID_t AAFClassID_KLVDataDefinition =
{0x0d010101, 0x0101, 0x4d00, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-4e00-060e-2b3402060101}
static const aafUID_t AAFClassID_AuxiliaryDescriptor =
{0x0d010101, 0x0101, 0x4e00, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-4f00-060e-2b3402060101}
static const aafUID_t AAFClassID_RIFFChunk =
{0x0d010101, 0x0101, 0x4f00, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-5000-060e-2b3402060101}
static const aafUID_t AAFClassID_BWFImportDescriptor =
{0x0d010101, 0x0101, 0x5000, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0101-5100-060e-2b3402060101}
static const aafUID_t AAFClassID_MPEGVideoDescriptor =
{0x0d010101, 0x0101, 0x5100, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0201-0000-060e-2b3402060101}
static const aafUID_t AAFClassID_ClassDefinition =
{0x0d010101, 0x0201, 0x0000, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0202-0000-060e-2b3402060101}
static const aafUID_t AAFClassID_PropertyDefinition =
{0x0d010101, 0x0202, 0x0000, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0203-0000-060e-2b3402060101}
static const aafUID_t AAFClassID_TypeDefinition =
{0x0d010101, 0x0203, 0x0000, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0204-0000-060e-2b3402060101}
static const aafUID_t AAFClassID_TypeDefinitionInteger =
{0x0d010101, 0x0204, 0x0000, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0205-0000-060e-2b3402060101}
static const aafUID_t AAFClassID_TypeDefinitionStrongObjectReference =
{0x0d010101, 0x0205, 0x0000, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0206-0000-060e-2b3402060101}
static const aafUID_t AAFClassID_TypeDefinitionWeakObjectReference =
{0x0d010101, 0x0206, 0x0000, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0207-0000-060e-2b3402060101}
static const aafUID_t AAFClassID_TypeDefinitionEnumeration =
{0x0d010101, 0x0207, 0x0000, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0208-0000-060e-2b3402060101}
static const aafUID_t AAFClassID_TypeDefinitionFixedArray =
{0x0d010101, 0x0208, 0x0000, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0209-0000-060e-2b3402060101}
static const aafUID_t AAFClassID_TypeDefinitionVariableArray =
{0x0d010101, 0x0209, 0x0000, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-020a-0000-060e-2b3402060101}
static const aafUID_t AAFClassID_TypeDefinitionSet =
{0x0d010101, 0x020a, 0x0000, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-020b-0000-060e-2b3402060101}
static const aafUID_t AAFClassID_TypeDefinitionString =
{0x0d010101, 0x020b, 0x0000, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-020c-0000-060e-2b3402060101}
static const aafUID_t AAFClassID_TypeDefinitionStream =
{0x0d010101, 0x020c, 0x0000, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-020d-0000-060e-2b3402060101}
static const aafUID_t AAFClassID_TypeDefinitionRecord =
{0x0d010101, 0x020d, 0x0000, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-020e-0000-060e-2b3402060101}
static const aafUID_t AAFClassID_TypeDefinitionRename =
{0x0d010101, 0x020e, 0x0000, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0220-0000-060e-2b3402060101}
static const aafUID_t AAFClassID_TypeDefinitionExtendibleEnumeration =
{0x0d010101, 0x0220, 0x0000, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0221-0000-060e-2b3402060101}
static const aafUID_t AAFClassID_TypeDefinitionIndirect =
{0x0d010101, 0x0221, 0x0000, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0222-0000-060e-2b3402060101}
static const aafUID_t AAFClassID_TypeDefinitionOpaque =
{0x0d010101, 0x0222, 0x0000, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0223-0000-060e-2b3402060101}
static const aafUID_t AAFClassID_TypeDefinitionCharacter =
{0x0d010101, 0x0223, 0x0000, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0224-0000-060e-2b3402060101}
static const aafUID_t AAFClassID_MetaDefinition =
{0x0d010101, 0x0224, 0x0000, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010101-0225-0000-060e-2b3402060101}
static const aafUID_t AAFClassID_MetaDictionary =
{0x0d010101, 0x0225, 0x0000, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010400-0000-0000-060e-2b3402060101}
static const aafUID_t AAFClassID_DescriptiveObject =
{0x0d010400, 0x0000, 0x0000, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};

//{0d010401-0000-0000-060e-2b3402060101}
static const aafUID_t AAFClassID_DescriptiveFramework =
{0x0d010401, 0x0000, 0x0000, {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x06, 0x01, 0x01}};


#endif // ! __AAFClassDefUIDs_h__
