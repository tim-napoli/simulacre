#include <iostream>
#include <vector>

#include <cstdint>
#include <cstring>

#include <windows.h>
#include <dbghelp.h>
#include <psapi.h>

#include "simulacre.h"




#define LOG_ERROR(_fmt, ...) \
      fprintf(stderr, "%s:%d: " _fmt, __FUNCTION__, __LINE__, __VA_ARGS__)




#define SIMULACRE_MAX_INDIRECT_CALLS   128




Simulacre::Simulacre(const std::string& _sModuleName)
   : m_sModuleName(_sModuleName)
   , m_hProcess(nullptr)
   , m_dwProcessBaseAddress(0)
   , m_vpIndirectCallsTable { }
   , m_vsSavedFunctions { }
{
   m_hProcess = GetCurrentProcess();
   if (!SymInitialize(m_hProcess, nullptr, FALSE)) {
      LOG_ERROR("unable to initialize symbols for current process (error %d)", GetLastError());
      throw;
   }

   char szProcessName[512] = "";
   if (_sModuleName.empty()) {
      GetModuleFileNameA(nullptr, szProcessName, sizeof(szProcessName) - 1);
   }
   else {
      strcpy_s(szProcessName, _sModuleName.c_str());
   }
#ifdef _WIN64
   m_dwProcessBaseAddress = SymLoadModule64(m_hProcess, nullptr, szProcessName, nullptr, 0, 0);
#else
   m_dwProcessBaseAddress = SymLoadModule(m_hProcess, nullptr, szProcessName, nullptr, 0, 0);
#endif
   if (m_dwProcessBaseAddress == 0) {
      LOG_ERROR("unable to load symbol table for module %s (error %d)", szProcessName, GetLastError());
      throw;
   }

   m_vpIndirectCallsTable.reserve(SIMULACRE_MAX_INDIRECT_CALLS);
}




Simulacre::~Simulacre()
{
   restoreOriginalFunctions();

   SymUnloadModule(m_hProcess, m_dwProcessBaseAddress);
   SymCleanup(m_hProcess);
}




ULONG64 Simulacre::getSymbolAddress(void* _pProcessAddress)
{
   HMODULE hModule = GetModuleHandleA((m_sModuleName.empty()) ? nullptr : m_sModuleName.c_str());
   if (hModule == nullptr) {
      return 0;
   }
   MODULEINFO sModuleInfo = { 0 };
   GetModuleInformation(m_hProcess, hModule, &sModuleInfo, sizeof(MODULEINFO));

   void* pModuleBaseAddress = (void*)sModuleInfo.lpBaseOfDll;
   return (ULONG64)((ULONG64)_pProcessAddress
                    - (ULONG64)pModuleBaseAddress
                    + (ULONG64)m_dwProcessBaseAddress);
}




void* Simulacre::symbolToProcessAddress(ULONG64 _ul64VirtualAddress)
{
   HMODULE hModule = GetModuleHandleA((m_sModuleName.empty()) ? nullptr : m_sModuleName.c_str());
   if (hModule == nullptr) {
      return nullptr;
   }
   MODULEINFO sModuleInfo = { 0 };
   GetModuleInformation(m_hProcess, hModule, &sModuleInfo, sizeof(MODULEINFO));

   void* pModuleBaseAddress = (void*)sModuleInfo.lpBaseOfDll;
   return (void*)(_ul64VirtualAddress - (ULONG64)m_dwProcessBaseAddress + (ULONG64)pModuleBaseAddress);
}




SYMBOL_INFO* Simulacre::getSymbol(void* _pProcessAddress)
{
   ULONG64 ul64SymbolAddress = getSymbolAddress(_pProcessAddress);
   if (!ul64SymbolAddress) {
      return nullptr;
   }

   SYMBOL_INFO* pSymbolInfo = (SYMBOL_INFO*)malloc(sizeof(SYMBOL_INFO) + 128);
   if (pSymbolInfo == nullptr) {
      return nullptr;
   }

   memset(pSymbolInfo, 0, sizeof(SYMBOL_INFO) + 128);
   pSymbolInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
   pSymbolInfo->MaxNameLen = 127;
   if (!SymFromAddr(m_hProcess, ul64SymbolAddress, 0, pSymbolInfo)) {
      LOG_ERROR("unable to find symbol address for symbol %p (error %d)", _pProcessAddress, GetLastError());
      free(pSymbolInfo);
      return nullptr;
   }

   return pSymbolInfo;
}




SYMBOL_INFO* Simulacre::getSymbolFromName(const std::string& _sSymbolName)
{
   SYMBOL_INFO* pSymbolInfo = (SYMBOL_INFO*)malloc(sizeof(SYMBOL_INFO) + 128);
   if (pSymbolInfo == nullptr) {
      return nullptr;
   }

   memset(pSymbolInfo, 0, sizeof(SYMBOL_INFO) + 128);
   pSymbolInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
   pSymbolInfo->MaxNameLen = 127;
   if (!SymFromName(m_hProcess, _sSymbolName.c_str(), pSymbolInfo)) {
      LOG_ERROR("unable to find symbol %s (error %d)", _sSymbolName.c_str(), GetLastError());
      free(pSymbolInfo);
      return nullptr;
   }

   return pSymbolInfo;
}




#pragma optimize( "", off )
HRESULT Simulacre::replaceFunctionCalls(void* _pFunctionAddress, size_t _sizeFunctionSize,
                                        void* _pOldFunctionAddress, void* _pNewFunctionAddress)
{
   // Read function code
   std::vector<uint8_t> vFunctionCode(_sizeFunctionSize, 0);
   memcpy(vFunctionCode.data(), _pFunctionAddress, _sizeFunctionSize);
   m_vsSavedFunctions.push_back({
      _pFunctionAddress, vFunctionCode
   });

   // Search and replace calls of `_pOldFunctionAddress` with calls of `_pNewFunctionAddress`
   for (size_t i = 0; i < vFunctionCode.size(); i++) {
      // TODO use intptr_t
#ifdef _WIN64
      int64_t iAbsoluteCodeAddress = (int64_t)(((char*)_pFunctionAddress) + i);
#else
      int32_t iAbsoluteCodeAddress = (int32_t)(((char*)_pFunctionAddress) + i);
#endif
      // Call near, relative, displacement relative to next instruction
      // Jump near, relative, displacement relative to next instruction (Release compilation)
      if (vFunctionCode[i] == 0xe8 || vFunctionCode[i] == 0xe9) {
         // TODO use intptr_t
         int32_t iCallRelativeAddress = 0;
         memcpy(&iCallRelativeAddress, vFunctionCode.data() + i + 1, sizeof(int32_t));
         int32_t iNextAbsoluteAddress = iAbsoluteCodeAddress + iCallRelativeAddress + 5;
         if (iNextAbsoluteAddress == (int32_t)_pOldFunctionAddress) {
            // Replace relative address of `_pOldFunctionAddress` with `_pNewFunctionAddress`
            int32_t iNextInstructionAddress = iAbsoluteCodeAddress + 5;
            int32_t iNewNextRelativeAddress = (int32_t)_pNewFunctionAddress - iNextInstructionAddress;
            memcpy(vFunctionCode.data() + i + 1, &iNewNextRelativeAddress, sizeof(int32_t));
         }
      }
#ifdef _WIN64
      else if (vFunctionCode[i] == 0xff && vFunctionCode[i + 1] == 0x15) {
         int64_t iRelativeAddressOfFunctionAddress = 0;
         memcpy(&iRelativeAddressOfFunctionAddress, vFunctionCode.data() + i + 2, sizeof(uint32_t));
         int64_t iAbsoluteAddressOfFunctionAddress = iAbsoluteCodeAddress + iRelativeAddressOfFunctionAddress + 6;
         int64_t iAbsoluteFunctionAddress = 0;
         memcpy(&iAbsoluteFunctionAddress, (void*)iAbsoluteAddressOfFunctionAddress, sizeof(uint64_t));
         if (iAbsoluteFunctionAddress == (int64_t)_pOldFunctionAddress) {
            // Replace the instruction with a near relative call.
            // We assume there the new function address will always be in the same code segment as the
            // patched function address. This does not appear to matter in 64 bits since CS=0.
            int64_t iNextInstructionAddress = (int64_t)_pFunctionAddress + i + 1 + 4;  // 1 + 4 = E8 /m32
            int32_t iRelativeAddress = (int32_t)((int64_t)_pNewFunctionAddress - iNextInstructionAddress);
            vFunctionCode[i + 0] = 0xe8;
            memcpy(vFunctionCode.data() + i + 1, &iRelativeAddress, 4);
            vFunctionCode[i + 5] = 0x90;  // NOP, could discard 32 highest bytes of RAX ?
         }
      }
      // REX.W FF, Jump far, absolute indirect, address given in m16:64.
      else if (vFunctionCode[i] == 0x48 && vFunctionCode[i + 1] == 0xff) {
         int32_t iRelativeAddressOfFunctionAddress = 0;
         memcpy(&iRelativeAddressOfFunctionAddress, vFunctionCode.data() + i + 3, sizeof(int32_t));
         int64_t iNextInstructionAddress = iAbsoluteCodeAddress + 7;
         int64_t iAbsoluteAddressOfFunctionAddress = iNextInstructionAddress + iRelativeAddressOfFunctionAddress;
         int64_t iAbsoluteFunctionAddress = 0;
         memcpy(&iAbsoluteFunctionAddress, (void*)iAbsoluteAddressOfFunctionAddress, sizeof(int64_t));
         if (iAbsoluteFunctionAddress == (int64_t)_pOldFunctionAddress) {
            // Replace the instruction with a near relative jmp.
            iNextInstructionAddress = (int64_t)_pFunctionAddress + i + 1 + 4; // 1 + 4 = E9 /m32
            int32_t iRelativeAddress = (int32_t)((int64_t)_pNewFunctionAddress - iNextInstructionAddress);
            vFunctionCode[i + 0] = 0xe9;
            memcpy(vFunctionCode.data() + i + 1, &iRelativeAddress, 4);
            vFunctionCode[i + 5] = 0x90;
            vFunctionCode[i + 6] = 0x90;
         }
      }
#else
      // Call near, absolute indirect, address given in r/m32
      else if (vFunctionCode[i] == 0xff && vFunctionCode[i + 1] == 0x15) {
         uint32_t iCallAbsoluteAddressAddress = 0;
         memcpy(&iCallAbsoluteAddressAddress, vFunctionCode.data() + i + 2, sizeof(uint32_t));
         void* pCallAbsoluteAddress = nullptr;
         memcpy(&pCallAbsoluteAddress, (void*)iCallAbsoluteAddressAddress, sizeof(void*));
         if (pCallAbsoluteAddress == _pOldFunctionAddress) {
            // Replace absolute address
            if (m_vpIndirectCallsTable.size() == m_vpIndirectCallsTable.capacity()) {
               LOG_ERROR("unable to patch absolute indirect call in %p: call table is full", _pFunctionAddress);
               return E_FAIL;
            }
            m_vpIndirectCallsTable.push_back(_pNewFunctionAddress);
            void* pIndirectAddr = (void*)&m_vpIndirectCallsTable.back();
            memcpy(vFunctionCode.data() + i + 2, &pIndirectAddr, sizeof(void*));
         }
      }
#endif
   }

   // Write patched code in process memory
   BOOL bWriteResult = WriteProcessMemory(
      m_hProcess, _pFunctionAddress, vFunctionCode.data(), vFunctionCode.size(), nullptr
   );
   if (!bWriteResult)
   {
      LOG_ERROR("unable to rewrite process memory for function %p (error %d)", _pFunctionAddress, GetLastError());
      m_vsSavedFunctions.pop_back();
      return E_FAIL;
   }

   return S_OK;
}




HRESULT Simulacre::mock(void* _pFunctionAddress, void* _pOldFunctionAddress, void* _pNewFunctionAddress)
{
   // Get function symbol in order to know its code size.
   SYMBOL_INFO* pSymbolInfo = getSymbol(_pFunctionAddress);
   if (pSymbolInfo == nullptr) {
      return E_FAIL;
   }
   size_t sizeFunctionSize = pSymbolInfo->Size;
   free(pSymbolInfo);

   return replaceFunctionCalls(
      _pFunctionAddress, sizeFunctionSize, _pOldFunctionAddress, _pNewFunctionAddress
   );
}




void* Simulacre::getVirtualMethodAddress(const std::string& _sVirtualMethodeName,
                                         size_t& _sizeVirtualMethodeSize)
{
   SYMBOL_INFO* pSymbol = getSymbolFromName(_sVirtualMethodeName.c_str());
   if (!pSymbol) {
      return nullptr;
   }
	void* pFunctionAddress = symbolToProcessAddress(pSymbol->Address);
   _sizeVirtualMethodeSize = pSymbol->Size;
   free(pSymbol);
   return pFunctionAddress;
}




HRESULT Simulacre::mockVirtualMethod(const std::string& _sVirtualMethodName,
                                     void* _pOldFunctionAddress, void* _pNewFunctionAddress)
{
   size_t sizeMethodSize = 0;
   void* pMethodAddress = getVirtualMethodAddress(_sVirtualMethodName, sizeMethodSize);
   if (pMethodAddress == nullptr) {
      return E_FAIL;
   }
   return replaceFunctionCalls(
      pMethodAddress, sizeMethodSize, _pOldFunctionAddress, _pNewFunctionAddress
   );
}




HRESULT Simulacre::restoreOriginalFunctions()
{
   for (auto& it : m_vsSavedFunctions) {
      BOOL bWriteResult = WriteProcessMemory(
         m_hProcess, it.pAddress, it.vu8Code.data(), it.vu8Code.size(), nullptr
      );
      if (!bWriteResult) {
         LOG_ERROR("unable to restore memory process for function %p (error %d)", it.pAddress, GetLastError());
         return E_FAIL;
      }
   }
   m_vsSavedFunctions.clear();
   return S_OK;
}