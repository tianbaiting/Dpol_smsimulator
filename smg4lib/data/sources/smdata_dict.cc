// Do NOT change. Changes will be lost next time file is generated

#define R__DICTIONARY_FILENAME smdata_dict
#define R__NO_DEPRECATION

/*******************************************************************/
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#define G__DICTIONARY
#include "RConfig.h"
#include "TClass.h"
#include "TDictAttributeMap.h"
#include "TInterpreter.h"
#include "TROOT.h"
#include "TBuffer.h"
#include "TMemberInspector.h"
#include "TInterpreter.h"
#include "TVirtualMutex.h"
#include "TError.h"

#ifndef G__ROOT
#define G__ROOT
#endif

#include "RtypesImp.h"
#include "TIsAProxy.h"
#include "TFileMergeInfo.h"
#include <algorithm>
#include "TCollectionProxyInfo.h"
/*******************************************************************/

#include "TDataMember.h"

// Header files passed as explicit arguments
#include "include/TBeamSimData.hh"
#include "include/TRunSimParameter.hh"
#include "include/TSimData.hh"
#include "include/TDetectorSimParameter.hh"
#include "include/TFragSimParameter.hh"
#include "include/TNEBULASimParameter.hh"
#include "include/TSimParameter.hh"

// Header files passed via #pragma extra_include

// The generated code does not explicitly qualify STL entities
namespace std {} using namespace std;

namespace ROOT {
   static void *new_TBeamSimData(void *p = nullptr);
   static void *newArray_TBeamSimData(Long_t size, void *p);
   static void delete_TBeamSimData(void *p);
   static void deleteArray_TBeamSimData(void *p);
   static void destruct_TBeamSimData(void *p);

   // Function generating the singleton type initializer
   static TGenericClassInfo *GenerateInitInstanceLocal(const ::TBeamSimData*)
   {
      ::TBeamSimData *ptr = nullptr;
      static ::TVirtualIsAProxy* isa_proxy = new ::TInstrumentedIsAProxy< ::TBeamSimData >(nullptr);
      static ::ROOT::TGenericClassInfo 
         instance("TBeamSimData", ::TBeamSimData::Class_Version(), "TBeamSimData.hh", 14,
                  typeid(::TBeamSimData), ::ROOT::Internal::DefineBehavior(ptr, ptr),
                  &::TBeamSimData::Dictionary, isa_proxy, 4,
                  sizeof(::TBeamSimData) );
      instance.SetNew(&new_TBeamSimData);
      instance.SetNewArray(&newArray_TBeamSimData);
      instance.SetDelete(&delete_TBeamSimData);
      instance.SetDeleteArray(&deleteArray_TBeamSimData);
      instance.SetDestructor(&destruct_TBeamSimData);
      return &instance;
   }
   TGenericClassInfo *GenerateInitInstance(const ::TBeamSimData*)
   {
      return GenerateInitInstanceLocal((::TBeamSimData*)nullptr);
   }
   // Static variable to force the class initialization
   static ::ROOT::TGenericClassInfo *_R__UNIQUE_DICT_(Init) = GenerateInitInstanceLocal((const ::TBeamSimData*)nullptr); R__UseDummy(_R__UNIQUE_DICT_(Init));
} // end of namespace ROOT

namespace ROOT {
   static void delete_TSimParameter(void *p);
   static void deleteArray_TSimParameter(void *p);
   static void destruct_TSimParameter(void *p);

   // Function generating the singleton type initializer
   static TGenericClassInfo *GenerateInitInstanceLocal(const ::TSimParameter*)
   {
      ::TSimParameter *ptr = nullptr;
      static ::TVirtualIsAProxy* isa_proxy = new ::TInstrumentedIsAProxy< ::TSimParameter >(nullptr);
      static ::ROOT::TGenericClassInfo 
         instance("TSimParameter", ::TSimParameter::Class_Version(), "TSimParameter.hh", 7,
                  typeid(::TSimParameter), ::ROOT::Internal::DefineBehavior(ptr, ptr),
                  &::TSimParameter::Dictionary, isa_proxy, 4,
                  sizeof(::TSimParameter) );
      instance.SetDelete(&delete_TSimParameter);
      instance.SetDeleteArray(&deleteArray_TSimParameter);
      instance.SetDestructor(&destruct_TSimParameter);
      return &instance;
   }
   TGenericClassInfo *GenerateInitInstance(const ::TSimParameter*)
   {
      return GenerateInitInstanceLocal((::TSimParameter*)nullptr);
   }
   // Static variable to force the class initialization
   static ::ROOT::TGenericClassInfo *_R__UNIQUE_DICT_(Init) = GenerateInitInstanceLocal((const ::TSimParameter*)nullptr); R__UseDummy(_R__UNIQUE_DICT_(Init));
} // end of namespace ROOT

namespace ROOT {
   static void *new_TRunSimParameter(void *p = nullptr);
   static void *newArray_TRunSimParameter(Long_t size, void *p);
   static void delete_TRunSimParameter(void *p);
   static void deleteArray_TRunSimParameter(void *p);
   static void destruct_TRunSimParameter(void *p);

   // Function generating the singleton type initializer
   static TGenericClassInfo *GenerateInitInstanceLocal(const ::TRunSimParameter*)
   {
      ::TRunSimParameter *ptr = nullptr;
      static ::TVirtualIsAProxy* isa_proxy = new ::TInstrumentedIsAProxy< ::TRunSimParameter >(nullptr);
      static ::ROOT::TGenericClassInfo 
         instance("TRunSimParameter", ::TRunSimParameter::Class_Version(), "TRunSimParameter.hh", 10,
                  typeid(::TRunSimParameter), ::ROOT::Internal::DefineBehavior(ptr, ptr),
                  &::TRunSimParameter::Dictionary, isa_proxy, 4,
                  sizeof(::TRunSimParameter) );
      instance.SetNew(&new_TRunSimParameter);
      instance.SetNewArray(&newArray_TRunSimParameter);
      instance.SetDelete(&delete_TRunSimParameter);
      instance.SetDeleteArray(&deleteArray_TRunSimParameter);
      instance.SetDestructor(&destruct_TRunSimParameter);
      return &instance;
   }
   TGenericClassInfo *GenerateInitInstance(const ::TRunSimParameter*)
   {
      return GenerateInitInstanceLocal((::TRunSimParameter*)nullptr);
   }
   // Static variable to force the class initialization
   static ::ROOT::TGenericClassInfo *_R__UNIQUE_DICT_(Init) = GenerateInitInstanceLocal((const ::TRunSimParameter*)nullptr); R__UseDummy(_R__UNIQUE_DICT_(Init));
} // end of namespace ROOT

namespace ROOT {
   static void *new_TSimData(void *p = nullptr);
   static void *newArray_TSimData(Long_t size, void *p);
   static void delete_TSimData(void *p);
   static void deleteArray_TSimData(void *p);
   static void destruct_TSimData(void *p);

   // Function generating the singleton type initializer
   static TGenericClassInfo *GenerateInitInstanceLocal(const ::TSimData*)
   {
      ::TSimData *ptr = nullptr;
      static ::TVirtualIsAProxy* isa_proxy = new ::TInstrumentedIsAProxy< ::TSimData >(nullptr);
      static ::ROOT::TGenericClassInfo 
         instance("TSimData", ::TSimData::Class_Version(), "TSimData.hh", 10,
                  typeid(::TSimData), ::ROOT::Internal::DefineBehavior(ptr, ptr),
                  &::TSimData::Dictionary, isa_proxy, 4,
                  sizeof(::TSimData) );
      instance.SetNew(&new_TSimData);
      instance.SetNewArray(&newArray_TSimData);
      instance.SetDelete(&delete_TSimData);
      instance.SetDeleteArray(&deleteArray_TSimData);
      instance.SetDestructor(&destruct_TSimData);
      return &instance;
   }
   TGenericClassInfo *GenerateInitInstance(const ::TSimData*)
   {
      return GenerateInitInstanceLocal((::TSimData*)nullptr);
   }
   // Static variable to force the class initialization
   static ::ROOT::TGenericClassInfo *_R__UNIQUE_DICT_(Init) = GenerateInitInstanceLocal((const ::TSimData*)nullptr); R__UseDummy(_R__UNIQUE_DICT_(Init));
} // end of namespace ROOT

namespace ROOT {
   static void *new_TDetectorSimParameter(void *p = nullptr);
   static void *newArray_TDetectorSimParameter(Long_t size, void *p);
   static void delete_TDetectorSimParameter(void *p);
   static void deleteArray_TDetectorSimParameter(void *p);
   static void destruct_TDetectorSimParameter(void *p);

   // Function generating the singleton type initializer
   static TGenericClassInfo *GenerateInitInstanceLocal(const ::TDetectorSimParameter*)
   {
      ::TDetectorSimParameter *ptr = nullptr;
      static ::TVirtualIsAProxy* isa_proxy = new ::TInstrumentedIsAProxy< ::TDetectorSimParameter >(nullptr);
      static ::ROOT::TGenericClassInfo 
         instance("TDetectorSimParameter", ::TDetectorSimParameter::Class_Version(), "TDetectorSimParameter.hh", 9,
                  typeid(::TDetectorSimParameter), ::ROOT::Internal::DefineBehavior(ptr, ptr),
                  &::TDetectorSimParameter::Dictionary, isa_proxy, 4,
                  sizeof(::TDetectorSimParameter) );
      instance.SetNew(&new_TDetectorSimParameter);
      instance.SetNewArray(&newArray_TDetectorSimParameter);
      instance.SetDelete(&delete_TDetectorSimParameter);
      instance.SetDeleteArray(&deleteArray_TDetectorSimParameter);
      instance.SetDestructor(&destruct_TDetectorSimParameter);
      return &instance;
   }
   TGenericClassInfo *GenerateInitInstance(const ::TDetectorSimParameter*)
   {
      return GenerateInitInstanceLocal((::TDetectorSimParameter*)nullptr);
   }
   // Static variable to force the class initialization
   static ::ROOT::TGenericClassInfo *_R__UNIQUE_DICT_(Init) = GenerateInitInstanceLocal((const ::TDetectorSimParameter*)nullptr); R__UseDummy(_R__UNIQUE_DICT_(Init));
} // end of namespace ROOT

namespace ROOT {
   static void *new_TFragSimParameter(void *p = nullptr);
   static void *newArray_TFragSimParameter(Long_t size, void *p);
   static void delete_TFragSimParameter(void *p);
   static void deleteArray_TFragSimParameter(void *p);
   static void destruct_TFragSimParameter(void *p);

   // Function generating the singleton type initializer
   static TGenericClassInfo *GenerateInitInstanceLocal(const ::TFragSimParameter*)
   {
      ::TFragSimParameter *ptr = nullptr;
      static ::TVirtualIsAProxy* isa_proxy = new ::TInstrumentedIsAProxy< ::TFragSimParameter >(nullptr);
      static ::ROOT::TGenericClassInfo 
         instance("TFragSimParameter", ::TFragSimParameter::Class_Version(), "TFragSimParameter.hh", 8,
                  typeid(::TFragSimParameter), ::ROOT::Internal::DefineBehavior(ptr, ptr),
                  &::TFragSimParameter::Dictionary, isa_proxy, 4,
                  sizeof(::TFragSimParameter) );
      instance.SetNew(&new_TFragSimParameter);
      instance.SetNewArray(&newArray_TFragSimParameter);
      instance.SetDelete(&delete_TFragSimParameter);
      instance.SetDeleteArray(&deleteArray_TFragSimParameter);
      instance.SetDestructor(&destruct_TFragSimParameter);
      return &instance;
   }
   TGenericClassInfo *GenerateInitInstance(const ::TFragSimParameter*)
   {
      return GenerateInitInstanceLocal((::TFragSimParameter*)nullptr);
   }
   // Static variable to force the class initialization
   static ::ROOT::TGenericClassInfo *_R__UNIQUE_DICT_(Init) = GenerateInitInstanceLocal((const ::TFragSimParameter*)nullptr); R__UseDummy(_R__UNIQUE_DICT_(Init));
} // end of namespace ROOT

namespace ROOT {
   static void *new_TNEBULASimParameter(void *p = nullptr);
   static void *newArray_TNEBULASimParameter(Long_t size, void *p);
   static void delete_TNEBULASimParameter(void *p);
   static void deleteArray_TNEBULASimParameter(void *p);
   static void destruct_TNEBULASimParameter(void *p);

   // Function generating the singleton type initializer
   static TGenericClassInfo *GenerateInitInstanceLocal(const ::TNEBULASimParameter*)
   {
      ::TNEBULASimParameter *ptr = nullptr;
      static ::TVirtualIsAProxy* isa_proxy = new ::TInstrumentedIsAProxy< ::TNEBULASimParameter >(nullptr);
      static ::ROOT::TGenericClassInfo 
         instance("TNEBULASimParameter", ::TNEBULASimParameter::Class_Version(), "TNEBULASimParameter.hh", 10,
                  typeid(::TNEBULASimParameter), ::ROOT::Internal::DefineBehavior(ptr, ptr),
                  &::TNEBULASimParameter::Dictionary, isa_proxy, 4,
                  sizeof(::TNEBULASimParameter) );
      instance.SetNew(&new_TNEBULASimParameter);
      instance.SetNewArray(&newArray_TNEBULASimParameter);
      instance.SetDelete(&delete_TNEBULASimParameter);
      instance.SetDeleteArray(&deleteArray_TNEBULASimParameter);
      instance.SetDestructor(&destruct_TNEBULASimParameter);
      return &instance;
   }
   TGenericClassInfo *GenerateInitInstance(const ::TNEBULASimParameter*)
   {
      return GenerateInitInstanceLocal((::TNEBULASimParameter*)nullptr);
   }
   // Static variable to force the class initialization
   static ::ROOT::TGenericClassInfo *_R__UNIQUE_DICT_(Init) = GenerateInitInstanceLocal((const ::TNEBULASimParameter*)nullptr); R__UseDummy(_R__UNIQUE_DICT_(Init));
} // end of namespace ROOT

//______________________________________________________________________________
atomic_TClass_ptr TBeamSimData::fgIsA(nullptr);  // static to hold class pointer

//______________________________________________________________________________
const char *TBeamSimData::Class_Name()
{
   return "TBeamSimData";
}

//______________________________________________________________________________
const char *TBeamSimData::ImplFileName()
{
   return ::ROOT::GenerateInitInstanceLocal((const ::TBeamSimData*)nullptr)->GetImplFileName();
}

//______________________________________________________________________________
int TBeamSimData::ImplFileLine()
{
   return ::ROOT::GenerateInitInstanceLocal((const ::TBeamSimData*)nullptr)->GetImplFileLine();
}

//______________________________________________________________________________
TClass *TBeamSimData::Dictionary()
{
   fgIsA = ::ROOT::GenerateInitInstanceLocal((const ::TBeamSimData*)nullptr)->GetClass();
   return fgIsA;
}

//______________________________________________________________________________
TClass *TBeamSimData::Class()
{
   if (!fgIsA.load()) { R__LOCKGUARD(gInterpreterMutex); fgIsA = ::ROOT::GenerateInitInstanceLocal((const ::TBeamSimData*)nullptr)->GetClass(); }
   return fgIsA;
}

//______________________________________________________________________________
atomic_TClass_ptr TSimParameter::fgIsA(nullptr);  // static to hold class pointer

//______________________________________________________________________________
const char *TSimParameter::Class_Name()
{
   return "TSimParameter";
}

//______________________________________________________________________________
const char *TSimParameter::ImplFileName()
{
   return ::ROOT::GenerateInitInstanceLocal((const ::TSimParameter*)nullptr)->GetImplFileName();
}

//______________________________________________________________________________
int TSimParameter::ImplFileLine()
{
   return ::ROOT::GenerateInitInstanceLocal((const ::TSimParameter*)nullptr)->GetImplFileLine();
}

//______________________________________________________________________________
TClass *TSimParameter::Dictionary()
{
   fgIsA = ::ROOT::GenerateInitInstanceLocal((const ::TSimParameter*)nullptr)->GetClass();
   return fgIsA;
}

//______________________________________________________________________________
TClass *TSimParameter::Class()
{
   if (!fgIsA.load()) { R__LOCKGUARD(gInterpreterMutex); fgIsA = ::ROOT::GenerateInitInstanceLocal((const ::TSimParameter*)nullptr)->GetClass(); }
   return fgIsA;
}

//______________________________________________________________________________
atomic_TClass_ptr TRunSimParameter::fgIsA(nullptr);  // static to hold class pointer

//______________________________________________________________________________
const char *TRunSimParameter::Class_Name()
{
   return "TRunSimParameter";
}

//______________________________________________________________________________
const char *TRunSimParameter::ImplFileName()
{
   return ::ROOT::GenerateInitInstanceLocal((const ::TRunSimParameter*)nullptr)->GetImplFileName();
}

//______________________________________________________________________________
int TRunSimParameter::ImplFileLine()
{
   return ::ROOT::GenerateInitInstanceLocal((const ::TRunSimParameter*)nullptr)->GetImplFileLine();
}

//______________________________________________________________________________
TClass *TRunSimParameter::Dictionary()
{
   fgIsA = ::ROOT::GenerateInitInstanceLocal((const ::TRunSimParameter*)nullptr)->GetClass();
   return fgIsA;
}

//______________________________________________________________________________
TClass *TRunSimParameter::Class()
{
   if (!fgIsA.load()) { R__LOCKGUARD(gInterpreterMutex); fgIsA = ::ROOT::GenerateInitInstanceLocal((const ::TRunSimParameter*)nullptr)->GetClass(); }
   return fgIsA;
}

//______________________________________________________________________________
atomic_TClass_ptr TSimData::fgIsA(nullptr);  // static to hold class pointer

//______________________________________________________________________________
const char *TSimData::Class_Name()
{
   return "TSimData";
}

//______________________________________________________________________________
const char *TSimData::ImplFileName()
{
   return ::ROOT::GenerateInitInstanceLocal((const ::TSimData*)nullptr)->GetImplFileName();
}

//______________________________________________________________________________
int TSimData::ImplFileLine()
{
   return ::ROOT::GenerateInitInstanceLocal((const ::TSimData*)nullptr)->GetImplFileLine();
}

//______________________________________________________________________________
TClass *TSimData::Dictionary()
{
   fgIsA = ::ROOT::GenerateInitInstanceLocal((const ::TSimData*)nullptr)->GetClass();
   return fgIsA;
}

//______________________________________________________________________________
TClass *TSimData::Class()
{
   if (!fgIsA.load()) { R__LOCKGUARD(gInterpreterMutex); fgIsA = ::ROOT::GenerateInitInstanceLocal((const ::TSimData*)nullptr)->GetClass(); }
   return fgIsA;
}

//______________________________________________________________________________
atomic_TClass_ptr TDetectorSimParameter::fgIsA(nullptr);  // static to hold class pointer

//______________________________________________________________________________
const char *TDetectorSimParameter::Class_Name()
{
   return "TDetectorSimParameter";
}

//______________________________________________________________________________
const char *TDetectorSimParameter::ImplFileName()
{
   return ::ROOT::GenerateInitInstanceLocal((const ::TDetectorSimParameter*)nullptr)->GetImplFileName();
}

//______________________________________________________________________________
int TDetectorSimParameter::ImplFileLine()
{
   return ::ROOT::GenerateInitInstanceLocal((const ::TDetectorSimParameter*)nullptr)->GetImplFileLine();
}

//______________________________________________________________________________
TClass *TDetectorSimParameter::Dictionary()
{
   fgIsA = ::ROOT::GenerateInitInstanceLocal((const ::TDetectorSimParameter*)nullptr)->GetClass();
   return fgIsA;
}

//______________________________________________________________________________
TClass *TDetectorSimParameter::Class()
{
   if (!fgIsA.load()) { R__LOCKGUARD(gInterpreterMutex); fgIsA = ::ROOT::GenerateInitInstanceLocal((const ::TDetectorSimParameter*)nullptr)->GetClass(); }
   return fgIsA;
}

//______________________________________________________________________________
atomic_TClass_ptr TFragSimParameter::fgIsA(nullptr);  // static to hold class pointer

//______________________________________________________________________________
const char *TFragSimParameter::Class_Name()
{
   return "TFragSimParameter";
}

//______________________________________________________________________________
const char *TFragSimParameter::ImplFileName()
{
   return ::ROOT::GenerateInitInstanceLocal((const ::TFragSimParameter*)nullptr)->GetImplFileName();
}

//______________________________________________________________________________
int TFragSimParameter::ImplFileLine()
{
   return ::ROOT::GenerateInitInstanceLocal((const ::TFragSimParameter*)nullptr)->GetImplFileLine();
}

//______________________________________________________________________________
TClass *TFragSimParameter::Dictionary()
{
   fgIsA = ::ROOT::GenerateInitInstanceLocal((const ::TFragSimParameter*)nullptr)->GetClass();
   return fgIsA;
}

//______________________________________________________________________________
TClass *TFragSimParameter::Class()
{
   if (!fgIsA.load()) { R__LOCKGUARD(gInterpreterMutex); fgIsA = ::ROOT::GenerateInitInstanceLocal((const ::TFragSimParameter*)nullptr)->GetClass(); }
   return fgIsA;
}

//______________________________________________________________________________
atomic_TClass_ptr TNEBULASimParameter::fgIsA(nullptr);  // static to hold class pointer

//______________________________________________________________________________
const char *TNEBULASimParameter::Class_Name()
{
   return "TNEBULASimParameter";
}

//______________________________________________________________________________
const char *TNEBULASimParameter::ImplFileName()
{
   return ::ROOT::GenerateInitInstanceLocal((const ::TNEBULASimParameter*)nullptr)->GetImplFileName();
}

//______________________________________________________________________________
int TNEBULASimParameter::ImplFileLine()
{
   return ::ROOT::GenerateInitInstanceLocal((const ::TNEBULASimParameter*)nullptr)->GetImplFileLine();
}

//______________________________________________________________________________
TClass *TNEBULASimParameter::Dictionary()
{
   fgIsA = ::ROOT::GenerateInitInstanceLocal((const ::TNEBULASimParameter*)nullptr)->GetClass();
   return fgIsA;
}

//______________________________________________________________________________
TClass *TNEBULASimParameter::Class()
{
   if (!fgIsA.load()) { R__LOCKGUARD(gInterpreterMutex); fgIsA = ::ROOT::GenerateInitInstanceLocal((const ::TNEBULASimParameter*)nullptr)->GetClass(); }
   return fgIsA;
}

//______________________________________________________________________________
void TBeamSimData::Streamer(TBuffer &R__b)
{
   // Stream an object of class TBeamSimData.

   if (R__b.IsReading()) {
      R__b.ReadClassBuffer(TBeamSimData::Class(),this);
   } else {
      R__b.WriteClassBuffer(TBeamSimData::Class(),this);
   }
}

namespace ROOT {
   // Wrappers around operator new
   static void *new_TBeamSimData(void *p) {
      return  p ? new(p) ::TBeamSimData : new ::TBeamSimData;
   }
   static void *newArray_TBeamSimData(Long_t nElements, void *p) {
      return p ? new(p) ::TBeamSimData[nElements] : new ::TBeamSimData[nElements];
   }
   // Wrapper around operator delete
   static void delete_TBeamSimData(void *p) {
      delete ((::TBeamSimData*)p);
   }
   static void deleteArray_TBeamSimData(void *p) {
      delete [] ((::TBeamSimData*)p);
   }
   static void destruct_TBeamSimData(void *p) {
      typedef ::TBeamSimData current_t;
      ((current_t*)p)->~current_t();
   }
} // end of namespace ROOT for class ::TBeamSimData

//______________________________________________________________________________
void TSimParameter::Streamer(TBuffer &R__b)
{
   // Stream an object of class TSimParameter.

   if (R__b.IsReading()) {
      R__b.ReadClassBuffer(TSimParameter::Class(),this);
   } else {
      R__b.WriteClassBuffer(TSimParameter::Class(),this);
   }
}

namespace ROOT {
   // Wrapper around operator delete
   static void delete_TSimParameter(void *p) {
      delete ((::TSimParameter*)p);
   }
   static void deleteArray_TSimParameter(void *p) {
      delete [] ((::TSimParameter*)p);
   }
   static void destruct_TSimParameter(void *p) {
      typedef ::TSimParameter current_t;
      ((current_t*)p)->~current_t();
   }
} // end of namespace ROOT for class ::TSimParameter

//______________________________________________________________________________
void TRunSimParameter::Streamer(TBuffer &R__b)
{
   // Stream an object of class TRunSimParameter.

   if (R__b.IsReading()) {
      R__b.ReadClassBuffer(TRunSimParameter::Class(),this);
   } else {
      R__b.WriteClassBuffer(TRunSimParameter::Class(),this);
   }
}

namespace ROOT {
   // Wrappers around operator new
   static void *new_TRunSimParameter(void *p) {
      return  p ? new(p) ::TRunSimParameter : new ::TRunSimParameter;
   }
   static void *newArray_TRunSimParameter(Long_t nElements, void *p) {
      return p ? new(p) ::TRunSimParameter[nElements] : new ::TRunSimParameter[nElements];
   }
   // Wrapper around operator delete
   static void delete_TRunSimParameter(void *p) {
      delete ((::TRunSimParameter*)p);
   }
   static void deleteArray_TRunSimParameter(void *p) {
      delete [] ((::TRunSimParameter*)p);
   }
   static void destruct_TRunSimParameter(void *p) {
      typedef ::TRunSimParameter current_t;
      ((current_t*)p)->~current_t();
   }
} // end of namespace ROOT for class ::TRunSimParameter

//______________________________________________________________________________
void TSimData::Streamer(TBuffer &R__b)
{
   // Stream an object of class TSimData.

   if (R__b.IsReading()) {
      R__b.ReadClassBuffer(TSimData::Class(),this);
   } else {
      R__b.WriteClassBuffer(TSimData::Class(),this);
   }
}

namespace ROOT {
   // Wrappers around operator new
   static void *new_TSimData(void *p) {
      return  p ? new(p) ::TSimData : new ::TSimData;
   }
   static void *newArray_TSimData(Long_t nElements, void *p) {
      return p ? new(p) ::TSimData[nElements] : new ::TSimData[nElements];
   }
   // Wrapper around operator delete
   static void delete_TSimData(void *p) {
      delete ((::TSimData*)p);
   }
   static void deleteArray_TSimData(void *p) {
      delete [] ((::TSimData*)p);
   }
   static void destruct_TSimData(void *p) {
      typedef ::TSimData current_t;
      ((current_t*)p)->~current_t();
   }
} // end of namespace ROOT for class ::TSimData

//______________________________________________________________________________
void TDetectorSimParameter::Streamer(TBuffer &R__b)
{
   // Stream an object of class TDetectorSimParameter.

   if (R__b.IsReading()) {
      R__b.ReadClassBuffer(TDetectorSimParameter::Class(),this);
   } else {
      R__b.WriteClassBuffer(TDetectorSimParameter::Class(),this);
   }
}

namespace ROOT {
   // Wrappers around operator new
   static void *new_TDetectorSimParameter(void *p) {
      return  p ? new(p) ::TDetectorSimParameter : new ::TDetectorSimParameter;
   }
   static void *newArray_TDetectorSimParameter(Long_t nElements, void *p) {
      return p ? new(p) ::TDetectorSimParameter[nElements] : new ::TDetectorSimParameter[nElements];
   }
   // Wrapper around operator delete
   static void delete_TDetectorSimParameter(void *p) {
      delete ((::TDetectorSimParameter*)p);
   }
   static void deleteArray_TDetectorSimParameter(void *p) {
      delete [] ((::TDetectorSimParameter*)p);
   }
   static void destruct_TDetectorSimParameter(void *p) {
      typedef ::TDetectorSimParameter current_t;
      ((current_t*)p)->~current_t();
   }
} // end of namespace ROOT for class ::TDetectorSimParameter

//______________________________________________________________________________
void TFragSimParameter::Streamer(TBuffer &R__b)
{
   // Stream an object of class TFragSimParameter.

   if (R__b.IsReading()) {
      R__b.ReadClassBuffer(TFragSimParameter::Class(),this);
   } else {
      R__b.WriteClassBuffer(TFragSimParameter::Class(),this);
   }
}

namespace ROOT {
   // Wrappers around operator new
   static void *new_TFragSimParameter(void *p) {
      return  p ? new(p) ::TFragSimParameter : new ::TFragSimParameter;
   }
   static void *newArray_TFragSimParameter(Long_t nElements, void *p) {
      return p ? new(p) ::TFragSimParameter[nElements] : new ::TFragSimParameter[nElements];
   }
   // Wrapper around operator delete
   static void delete_TFragSimParameter(void *p) {
      delete ((::TFragSimParameter*)p);
   }
   static void deleteArray_TFragSimParameter(void *p) {
      delete [] ((::TFragSimParameter*)p);
   }
   static void destruct_TFragSimParameter(void *p) {
      typedef ::TFragSimParameter current_t;
      ((current_t*)p)->~current_t();
   }
} // end of namespace ROOT for class ::TFragSimParameter

//______________________________________________________________________________
void TNEBULASimParameter::Streamer(TBuffer &R__b)
{
   // Stream an object of class TNEBULASimParameter.

   if (R__b.IsReading()) {
      R__b.ReadClassBuffer(TNEBULASimParameter::Class(),this);
   } else {
      R__b.WriteClassBuffer(TNEBULASimParameter::Class(),this);
   }
}

namespace ROOT {
   // Wrappers around operator new
   static void *new_TNEBULASimParameter(void *p) {
      return  p ? new(p) ::TNEBULASimParameter : new ::TNEBULASimParameter;
   }
   static void *newArray_TNEBULASimParameter(Long_t nElements, void *p) {
      return p ? new(p) ::TNEBULASimParameter[nElements] : new ::TNEBULASimParameter[nElements];
   }
   // Wrapper around operator delete
   static void delete_TNEBULASimParameter(void *p) {
      delete ((::TNEBULASimParameter*)p);
   }
   static void deleteArray_TNEBULASimParameter(void *p) {
      delete [] ((::TNEBULASimParameter*)p);
   }
   static void destruct_TNEBULASimParameter(void *p) {
      typedef ::TNEBULASimParameter current_t;
      ((current_t*)p)->~current_t();
   }
} // end of namespace ROOT for class ::TNEBULASimParameter

namespace ROOT {
   static TClass *vectorlEintgR_Dictionary();
   static void vectorlEintgR_TClassManip(TClass*);
   static void *new_vectorlEintgR(void *p = nullptr);
   static void *newArray_vectorlEintgR(Long_t size, void *p);
   static void delete_vectorlEintgR(void *p);
   static void deleteArray_vectorlEintgR(void *p);
   static void destruct_vectorlEintgR(void *p);

   // Function generating the singleton type initializer
   static TGenericClassInfo *GenerateInitInstanceLocal(const vector<int>*)
   {
      vector<int> *ptr = nullptr;
      static ::TVirtualIsAProxy* isa_proxy = new ::TIsAProxy(typeid(vector<int>));
      static ::ROOT::TGenericClassInfo 
         instance("vector<int>", -2, "vector", 216,
                  typeid(vector<int>), ::ROOT::Internal::DefineBehavior(ptr, ptr),
                  &vectorlEintgR_Dictionary, isa_proxy, 0,
                  sizeof(vector<int>) );
      instance.SetNew(&new_vectorlEintgR);
      instance.SetNewArray(&newArray_vectorlEintgR);
      instance.SetDelete(&delete_vectorlEintgR);
      instance.SetDeleteArray(&deleteArray_vectorlEintgR);
      instance.SetDestructor(&destruct_vectorlEintgR);
      instance.AdoptCollectionProxyInfo(TCollectionProxyInfo::Generate(TCollectionProxyInfo::Pushback< vector<int> >()));

      ::ROOT::AddClassAlternate("vector<int>","std::vector<int, std::allocator<int> >");
      return &instance;
   }
   // Static variable to force the class initialization
   static ::ROOT::TGenericClassInfo *_R__UNIQUE_DICT_(Init) = GenerateInitInstanceLocal((const vector<int>*)nullptr); R__UseDummy(_R__UNIQUE_DICT_(Init));

   // Dictionary for non-ClassDef classes
   static TClass *vectorlEintgR_Dictionary() {
      TClass* theClass =::ROOT::GenerateInitInstanceLocal((const vector<int>*)nullptr)->GetClass();
      vectorlEintgR_TClassManip(theClass);
   return theClass;
   }

   static void vectorlEintgR_TClassManip(TClass* ){
   }

} // end of namespace ROOT

namespace ROOT {
   // Wrappers around operator new
   static void *new_vectorlEintgR(void *p) {
      return  p ? ::new((::ROOT::Internal::TOperatorNewHelper*)p) vector<int> : new vector<int>;
   }
   static void *newArray_vectorlEintgR(Long_t nElements, void *p) {
      return p ? ::new((::ROOT::Internal::TOperatorNewHelper*)p) vector<int>[nElements] : new vector<int>[nElements];
   }
   // Wrapper around operator delete
   static void delete_vectorlEintgR(void *p) {
      delete ((vector<int>*)p);
   }
   static void deleteArray_vectorlEintgR(void *p) {
      delete [] ((vector<int>*)p);
   }
   static void destruct_vectorlEintgR(void *p) {
      typedef vector<int> current_t;
      ((current_t*)p)->~current_t();
   }
} // end of namespace ROOT for class vector<int>

namespace ROOT {
   static TClass *vectorlEdoublegR_Dictionary();
   static void vectorlEdoublegR_TClassManip(TClass*);
   static void *new_vectorlEdoublegR(void *p = nullptr);
   static void *newArray_vectorlEdoublegR(Long_t size, void *p);
   static void delete_vectorlEdoublegR(void *p);
   static void deleteArray_vectorlEdoublegR(void *p);
   static void destruct_vectorlEdoublegR(void *p);

   // Function generating the singleton type initializer
   static TGenericClassInfo *GenerateInitInstanceLocal(const vector<double>*)
   {
      vector<double> *ptr = nullptr;
      static ::TVirtualIsAProxy* isa_proxy = new ::TIsAProxy(typeid(vector<double>));
      static ::ROOT::TGenericClassInfo 
         instance("vector<double>", -2, "vector", 216,
                  typeid(vector<double>), ::ROOT::Internal::DefineBehavior(ptr, ptr),
                  &vectorlEdoublegR_Dictionary, isa_proxy, 0,
                  sizeof(vector<double>) );
      instance.SetNew(&new_vectorlEdoublegR);
      instance.SetNewArray(&newArray_vectorlEdoublegR);
      instance.SetDelete(&delete_vectorlEdoublegR);
      instance.SetDeleteArray(&deleteArray_vectorlEdoublegR);
      instance.SetDestructor(&destruct_vectorlEdoublegR);
      instance.AdoptCollectionProxyInfo(TCollectionProxyInfo::Generate(TCollectionProxyInfo::Pushback< vector<double> >()));

      ::ROOT::AddClassAlternate("vector<double>","std::vector<double, std::allocator<double> >");
      return &instance;
   }
   // Static variable to force the class initialization
   static ::ROOT::TGenericClassInfo *_R__UNIQUE_DICT_(Init) = GenerateInitInstanceLocal((const vector<double>*)nullptr); R__UseDummy(_R__UNIQUE_DICT_(Init));

   // Dictionary for non-ClassDef classes
   static TClass *vectorlEdoublegR_Dictionary() {
      TClass* theClass =::ROOT::GenerateInitInstanceLocal((const vector<double>*)nullptr)->GetClass();
      vectorlEdoublegR_TClassManip(theClass);
   return theClass;
   }

   static void vectorlEdoublegR_TClassManip(TClass* ){
   }

} // end of namespace ROOT

namespace ROOT {
   // Wrappers around operator new
   static void *new_vectorlEdoublegR(void *p) {
      return  p ? ::new((::ROOT::Internal::TOperatorNewHelper*)p) vector<double> : new vector<double>;
   }
   static void *newArray_vectorlEdoublegR(Long_t nElements, void *p) {
      return p ? ::new((::ROOT::Internal::TOperatorNewHelper*)p) vector<double>[nElements] : new vector<double>[nElements];
   }
   // Wrapper around operator delete
   static void delete_vectorlEdoublegR(void *p) {
      delete ((vector<double>*)p);
   }
   static void deleteArray_vectorlEdoublegR(void *p) {
      delete [] ((vector<double>*)p);
   }
   static void destruct_vectorlEdoublegR(void *p) {
      typedef vector<double> current_t;
      ((current_t*)p)->~current_t();
   }
} // end of namespace ROOT for class vector<double>

namespace ROOT {
   static TClass *vectorlETBeamSimDatagR_Dictionary();
   static void vectorlETBeamSimDatagR_TClassManip(TClass*);
   static void *new_vectorlETBeamSimDatagR(void *p = nullptr);
   static void *newArray_vectorlETBeamSimDatagR(Long_t size, void *p);
   static void delete_vectorlETBeamSimDatagR(void *p);
   static void deleteArray_vectorlETBeamSimDatagR(void *p);
   static void destruct_vectorlETBeamSimDatagR(void *p);

   // Function generating the singleton type initializer
   static TGenericClassInfo *GenerateInitInstanceLocal(const vector<TBeamSimData>*)
   {
      vector<TBeamSimData> *ptr = nullptr;
      static ::TVirtualIsAProxy* isa_proxy = new ::TIsAProxy(typeid(vector<TBeamSimData>));
      static ::ROOT::TGenericClassInfo 
         instance("vector<TBeamSimData>", -2, "vector", 216,
                  typeid(vector<TBeamSimData>), ::ROOT::Internal::DefineBehavior(ptr, ptr),
                  &vectorlETBeamSimDatagR_Dictionary, isa_proxy, 4,
                  sizeof(vector<TBeamSimData>) );
      instance.SetNew(&new_vectorlETBeamSimDatagR);
      instance.SetNewArray(&newArray_vectorlETBeamSimDatagR);
      instance.SetDelete(&delete_vectorlETBeamSimDatagR);
      instance.SetDeleteArray(&deleteArray_vectorlETBeamSimDatagR);
      instance.SetDestructor(&destruct_vectorlETBeamSimDatagR);
      instance.AdoptCollectionProxyInfo(TCollectionProxyInfo::Generate(TCollectionProxyInfo::Pushback< vector<TBeamSimData> >()));

      ::ROOT::AddClassAlternate("vector<TBeamSimData>","std::vector<TBeamSimData, std::allocator<TBeamSimData> >");
      return &instance;
   }
   // Static variable to force the class initialization
   static ::ROOT::TGenericClassInfo *_R__UNIQUE_DICT_(Init) = GenerateInitInstanceLocal((const vector<TBeamSimData>*)nullptr); R__UseDummy(_R__UNIQUE_DICT_(Init));

   // Dictionary for non-ClassDef classes
   static TClass *vectorlETBeamSimDatagR_Dictionary() {
      TClass* theClass =::ROOT::GenerateInitInstanceLocal((const vector<TBeamSimData>*)nullptr)->GetClass();
      vectorlETBeamSimDatagR_TClassManip(theClass);
   return theClass;
   }

   static void vectorlETBeamSimDatagR_TClassManip(TClass* ){
   }

} // end of namespace ROOT

namespace ROOT {
   // Wrappers around operator new
   static void *new_vectorlETBeamSimDatagR(void *p) {
      return  p ? ::new((::ROOT::Internal::TOperatorNewHelper*)p) vector<TBeamSimData> : new vector<TBeamSimData>;
   }
   static void *newArray_vectorlETBeamSimDatagR(Long_t nElements, void *p) {
      return p ? ::new((::ROOT::Internal::TOperatorNewHelper*)p) vector<TBeamSimData>[nElements] : new vector<TBeamSimData>[nElements];
   }
   // Wrapper around operator delete
   static void delete_vectorlETBeamSimDatagR(void *p) {
      delete ((vector<TBeamSimData>*)p);
   }
   static void deleteArray_vectorlETBeamSimDatagR(void *p) {
      delete [] ((vector<TBeamSimData>*)p);
   }
   static void destruct_vectorlETBeamSimDatagR(void *p) {
      typedef vector<TBeamSimData> current_t;
      ((current_t*)p)->~current_t();
   }
} // end of namespace ROOT for class vector<TBeamSimData>

namespace ROOT {
   static TClass *maplEintcOTDetectorSimParametergR_Dictionary();
   static void maplEintcOTDetectorSimParametergR_TClassManip(TClass*);
   static void *new_maplEintcOTDetectorSimParametergR(void *p = nullptr);
   static void *newArray_maplEintcOTDetectorSimParametergR(Long_t size, void *p);
   static void delete_maplEintcOTDetectorSimParametergR(void *p);
   static void deleteArray_maplEintcOTDetectorSimParametergR(void *p);
   static void destruct_maplEintcOTDetectorSimParametergR(void *p);

   // Function generating the singleton type initializer
   static TGenericClassInfo *GenerateInitInstanceLocal(const map<int,TDetectorSimParameter>*)
   {
      map<int,TDetectorSimParameter> *ptr = nullptr;
      static ::TVirtualIsAProxy* isa_proxy = new ::TIsAProxy(typeid(map<int,TDetectorSimParameter>));
      static ::ROOT::TGenericClassInfo 
         instance("map<int,TDetectorSimParameter>", -2, "map", 99,
                  typeid(map<int,TDetectorSimParameter>), ::ROOT::Internal::DefineBehavior(ptr, ptr),
                  &maplEintcOTDetectorSimParametergR_Dictionary, isa_proxy, 0,
                  sizeof(map<int,TDetectorSimParameter>) );
      instance.SetNew(&new_maplEintcOTDetectorSimParametergR);
      instance.SetNewArray(&newArray_maplEintcOTDetectorSimParametergR);
      instance.SetDelete(&delete_maplEintcOTDetectorSimParametergR);
      instance.SetDeleteArray(&deleteArray_maplEintcOTDetectorSimParametergR);
      instance.SetDestructor(&destruct_maplEintcOTDetectorSimParametergR);
      instance.AdoptCollectionProxyInfo(TCollectionProxyInfo::Generate(TCollectionProxyInfo::MapInsert< map<int,TDetectorSimParameter> >()));

      ::ROOT::AddClassAlternate("map<int,TDetectorSimParameter>","std::map<int, TDetectorSimParameter, std::less<int>, std::allocator<std::pair<int const, TDetectorSimParameter> > >");
      return &instance;
   }
   // Static variable to force the class initialization
   static ::ROOT::TGenericClassInfo *_R__UNIQUE_DICT_(Init) = GenerateInitInstanceLocal((const map<int,TDetectorSimParameter>*)nullptr); R__UseDummy(_R__UNIQUE_DICT_(Init));

   // Dictionary for non-ClassDef classes
   static TClass *maplEintcOTDetectorSimParametergR_Dictionary() {
      TClass* theClass =::ROOT::GenerateInitInstanceLocal((const map<int,TDetectorSimParameter>*)nullptr)->GetClass();
      maplEintcOTDetectorSimParametergR_TClassManip(theClass);
   return theClass;
   }

   static void maplEintcOTDetectorSimParametergR_TClassManip(TClass* ){
   }

} // end of namespace ROOT

namespace ROOT {
   // Wrappers around operator new
   static void *new_maplEintcOTDetectorSimParametergR(void *p) {
      return  p ? ::new((::ROOT::Internal::TOperatorNewHelper*)p) map<int,TDetectorSimParameter> : new map<int,TDetectorSimParameter>;
   }
   static void *newArray_maplEintcOTDetectorSimParametergR(Long_t nElements, void *p) {
      return p ? ::new((::ROOT::Internal::TOperatorNewHelper*)p) map<int,TDetectorSimParameter>[nElements] : new map<int,TDetectorSimParameter>[nElements];
   }
   // Wrapper around operator delete
   static void delete_maplEintcOTDetectorSimParametergR(void *p) {
      delete ((map<int,TDetectorSimParameter>*)p);
   }
   static void deleteArray_maplEintcOTDetectorSimParametergR(void *p) {
      delete [] ((map<int,TDetectorSimParameter>*)p);
   }
   static void destruct_maplEintcOTDetectorSimParametergR(void *p) {
      typedef map<int,TDetectorSimParameter> current_t;
      ((current_t*)p)->~current_t();
   }
} // end of namespace ROOT for class map<int,TDetectorSimParameter>

namespace {
  void TriggerDictionaryInitialization_smdata_dict_Impl() {
    static const char* headers[] = {
"include/TBeamSimData.hh",
"include/TRunSimParameter.hh",
"include/TSimData.hh",
"include/TDetectorSimParameter.hh",
"include/TFragSimParameter.hh",
"include/TNEBULASimParameter.hh",
"include/TSimParameter.hh",
nullptr
    };
    static const char* includePaths[] = {
"..",
"/home/tbt/Software/root/include",
"/home/tbt/workspace/dpol/tbt_anaroot/src/include",
"/home/tbt/Software/root/include/",
"/home/tbt/workspace/dpol/smsimulator5.5/smg4lib/data/sources/",
nullptr
    };
    static const char* fwdDeclCode = R"DICTFWDDCLS(
#line 1 "smdata_dict dictionary forward declarations' payload"
#pragma clang diagnostic ignored "-Wkeyword-compat"
#pragma clang diagnostic ignored "-Wignored-attributes"
#pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
extern int __Cling_AutoLoading_Map;
class __attribute__((annotate("$clingAutoload$include/TBeamSimData.hh")))  TBeamSimData;
namespace std{template <typename _Tp> class __attribute__((annotate("$clingAutoload$bits/allocator.h")))  __attribute__((annotate("$clingAutoload$string")))  allocator;
}
class __attribute__((annotate("$clingAutoload$TSimParameter.hh")))  __attribute__((annotate("$clingAutoload$include/TRunSimParameter.hh")))  TSimParameter;
class __attribute__((annotate("$clingAutoload$include/TRunSimParameter.hh")))  TRunSimParameter;
class __attribute__((annotate("$clingAutoload$include/TSimData.hh")))  TSimData;
class __attribute__((annotate("$clingAutoload$include/TDetectorSimParameter.hh")))  TDetectorSimParameter;
class __attribute__((annotate("$clingAutoload$include/TFragSimParameter.hh")))  TFragSimParameter;
class __attribute__((annotate("$clingAutoload$include/TNEBULASimParameter.hh")))  TNEBULASimParameter;
)DICTFWDDCLS";
    static const char* payloadCode = R"DICTPAYLOAD(
#line 1 "smdata_dict dictionary payload"


#define _BACKWARD_BACKWARD_WARNING_H
// Inline headers
#include "include/TBeamSimData.hh"
#include "include/TRunSimParameter.hh"
#include "include/TSimData.hh"
#include "include/TDetectorSimParameter.hh"
#include "include/TFragSimParameter.hh"
#include "include/TNEBULASimParameter.hh"
#include "include/TSimParameter.hh"

#undef  _BACKWARD_BACKWARD_WARNING_H
)DICTPAYLOAD";
    static const char* classesHeaders[] = {
"TBeamSimData", payloadCode, "@",
"TBeamSimDataArray", payloadCode, "@",
"TDetectorSimParameter", payloadCode, "@",
"TFragSimParameter", payloadCode, "@",
"TNEBULASimParameter", payloadCode, "@",
"TRunSimParameter", payloadCode, "@",
"TSimData", payloadCode, "@",
"TSimParameter", payloadCode, "@",
nullptr
};
    static bool isInitialized = false;
    if (!isInitialized) {
      TROOT::RegisterModule("smdata_dict",
        headers, includePaths, payloadCode, fwdDeclCode,
        TriggerDictionaryInitialization_smdata_dict_Impl, {}, classesHeaders, /*hasCxxModule*/false);
      isInitialized = true;
    }
  }
  static struct DictInit {
    DictInit() {
      TriggerDictionaryInitialization_smdata_dict_Impl();
    }
  } __TheDictionaryInitializer;
}
void TriggerDictionaryInitialization_smdata_dict() {
  TriggerDictionaryInitialization_smdata_dict_Impl();
}
