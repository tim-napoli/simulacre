#include <iostream>
#include <vector>

#include <cstdint>
#include <cstring>

#include <windows.h>
#include <dbghelp.h>
#include <psapi.h>

#include "simulacre.h"




Simulacre::Simulacre(const std::string& _sModuleName)
   : m_sModuleName(_sModuleName)
   , m_hProcess(nullptr)
   , m_dwProcessBaseAddress(0)
   , m_vui32CallIndirectTable { }
   , m_vsSavedFunctions { }
{
   m_hProcess = GetCurrentProcess();
   if (!SymInitialize(m_hProcess, nullptr, FALSE)) {
      std::cerr << "Simulacre::Simulacre(): unable to find symbols for current process" << std::endl;
      throw;
   }

   SymSetOptions(SYMOPT_DEBUG);

   char szCurrentDirectory[2048] = "";
   GetCurrentDirectoryA(sizeof(szCurrentDirectory), szCurrentDirectory);

   char szSearchPath[2048] = "";
   SymGetSearchPath(m_hProcess, szSearchPath, sizeof(szSearchPath) - 1);

   char szProcessName[512] = "";
   if (_sModuleName.empty()) {
      GetModuleFileNameA(nullptr, szProcessName, sizeof(szProcessName) - 1);
   }
   else {
      strcpy_s(szProcessName, _sModuleName.c_str());
   }
   m_dwProcessBaseAddress = SymLoadModule(m_hProcess, nullptr, szProcessName, nullptr, 0, 0);
   if (m_dwProcessBaseAddress == 0) {
      std::cerr << "Simulacre::Simulacre(): unable to load module" << std::endl;
      throw;
   }

   m_vui32CallIndirectTable.reserve(128);
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

   void* pModuleBaseAddress = (void*)((DWORD)sModuleInfo.lpBaseOfDll);
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

   void* pModuleBaseAddress = (void*)((DWORD)sModuleInfo.lpBaseOfDll);
   return (void*)(_ul64VirtualAddress - (ULONG64)m_dwProcessBaseAddress + (ULONG64)pModuleBaseAddress);
}




SYMBOL_INFO* Simulacre::getSymbol(void* _pFunctionAddress)
{
   ULONG64 ul64SymbolAddress = getSymbolAddress(_pFunctionAddress);
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
      free(pSymbolInfo);
      return nullptr;
   }

   return pSymbolInfo;
}




HRESULT Simulacre::replaceFunctionCalls(void* _pFunctionAddress, size_t _sizeFunctionSize,
                                        void* _pOldFunctionAddress, void* _pNewFunctionAddress)
{
   // Read function code.
   std::vector<uint8_t> vFunctionCode(_sizeFunctionSize, 0);
   memcpy(vFunctionCode.data(), _pFunctionAddress, _sizeFunctionSize);

   m_vsSavedFunctions.push_back({
      _pFunctionAddress, vFunctionCode
   });

   // Search and replace calls of `_pOldFunctionAddress` with calls of `_pNewFunctionAddress`
   for (size_t i = 0; i < vFunctionCode.size(); i++) {
      int32_t iAbsoluteCodeAddress = (int32_t)(((char*)_pFunctionAddress) + i);
      // Call near, relative, displacement relative to next instruction
      // Jump near, relative, displacement relative to next instruction (Release compilation)
      if (vFunctionCode[i] == 0xe8 || vFunctionCode[i] == 0xe9) {
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
      // Call near, absolute indirect, address given in r/m32
      else if (vFunctionCode[i] == 0xff && vFunctionCode[i + 1] == 0x15) {
         uint32_t iCallAbsoluteAddressAddress = 0;
         memcpy(&iCallAbsoluteAddressAddress, vFunctionCode.data() + i + 2, sizeof(uint32_t));
         uint32_t iCallAbsoluteAddress = 0;
         memcpy(&iCallAbsoluteAddress, (void*)iCallAbsoluteAddressAddress, sizeof(uint32_t));
         if (iCallAbsoluteAddress == (uint32_t)_pOldFunctionAddress) {
            // Replace absolute address
            if (m_vui32CallIndirectTable.size() == m_vui32CallIndirectTable.capacity()) {
               std::cerr << "Simulacre::replaceFunctionCalls: Failure to replace absolute indirect call :"
                         << "call table is full"
                         << std::endl;
               return E_FAIL;
            }
            m_vui32CallIndirectTable.push_back((uint32_t)_pNewFunctionAddress);
            uint32_t ui32IndirectAddr = (uint32_t)&m_vui32CallIndirectTable.back();
            memcpy(vFunctionCode.data() + i + 2, &ui32IndirectAddr, sizeof(uint32_t));
         }
      }
   }

   // Write patched code in process memory
   BOOL bWriteResult = WriteProcessMemory(
      m_hProcess, _pFunctionAddress, vFunctionCode.data(), vFunctionCode.size(), nullptr
   );
   if (!bWriteResult)
   {
      std::cerr << "Simulacre::replaceFunctionCalls: unable to rewrite process memory for function "
                << std::hex << _pFunctionAddress << "(error " << GetLastError() <<  ")" << std::endl;
      m_vsSavedFunctions.pop_back();
      return E_FAIL;
   }

   return S_OK;
}




HRESULT Simulacre::mock(void* _pFunctionAddress, void* _pOldFunctionAddress, void* _pNewFunctionAddress)
{
   // On récupère le symbole qui correspond à la fonction afin de déterminer sa taille.
   SYMBOL_INFO* pSymbolInfo = getSymbol(_pFunctionAddress);
   if (pSymbolInfo == nullptr) {
      std::cerr << "Simulacre::mock(): unable to find symbol address for function "
                << std::hex << _pFunctionAddress
                << " (error " << GetLastError() << ")" << std::endl;
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
      std::cerr << "CPPMock::mockVirtualMethod(): unable to find symbol address for function "
                << _sVirtualMethodName << std::endl;
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
      if (!bWriteResult)
      {
         std::cerr << "Simulacre::restoreOriginalFunctions: unable to restore function code of"
                   << std::hex << it.pAddress << "(error " << GetLastError() <<  ")" << std::endl;
         return E_FAIL;
      }
   }
   m_vsSavedFunctions.clear();
   return S_OK;
}