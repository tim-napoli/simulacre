#include <iostream>
#include <vector>

#include <cstdint>
#include <cstring>

#include <windows.h>
#include <dbghelp.h>
#include <psapi.h>

#include "cppmock.h"




CPPMock::CPPMock(const std::string& _sModuleName)
   : m_sModuleName(_sModuleName)
   , m_hProcess(nullptr)
   , m_dwProcessBaseAddress(0)
   , m_vui32CallIndirectTable { }
{
   m_hProcess = GetCurrentProcess();
   if (!SymInitialize(m_hProcess, nullptr, FALSE)) {
      std::cerr << "CPPMock::CPPMock(): unable to find symbols for current process" << std::endl;
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
      std::cerr << "CPPMock::CPPMock(): unable to load module" << std::endl;
      throw;
   }
}




CPPMock::~CPPMock()
{
   SymUnloadModule(m_hProcess, m_dwProcessBaseAddress);
   SymCleanup(m_hProcess);
}




ULONG64 CPPMock::getSymbolAddress(void* _pFunctionAddress)
{
   HMODULE hModule = GetModuleHandleA((m_sModuleName.empty()) ? nullptr : m_sModuleName.c_str());
   if (hModule == nullptr) {
      return 0;
   }
   MODULEINFO sModuleInfo = { 0 };
   GetModuleInformation(m_hProcess, hModule, &sModuleInfo, sizeof(MODULEINFO));

   void* pModuleBaseAddress = (void*)((DWORD)sModuleInfo.lpBaseOfDll);
   return (ULONG64)((ULONG64)_pFunctionAddress - (ULONG64)pModuleBaseAddress + (ULONG64)m_dwProcessBaseAddress);
}




SYMBOL_INFO* CPPMock::getSymbol(void* _pFunctionAddress)
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




HRESULT CPPMock::rewriteFunctionCodeCalls(void* _pFunctionAddress, size_t _sizeFunctionSize, void* _pOldFunctionAddress, void* _pNewFunctionAddress)
{
   // Lecture du code de la fonction
   std::vector<uint8_t> vFunctionCode(_sizeFunctionSize, 0);
   memcpy(vFunctionCode.data(), _pFunctionAddress, _sizeFunctionSize);

   // Recherche des calls référençants `_pOldFunctionAddress` et modification du code.
   for (size_t i = 0; i < vFunctionCode.size(); i++) {
      int32_t iAbsoluteCodeAddress = (int32_t)(((char*)_pFunctionAddress) + i);
      // Call near, relative, displacement relative to next instruction
      // Jump near, relative, displacement relative to next instruction (Release compilation)
      if (vFunctionCode[i] == 0xe8 || vFunctionCode[i] == 0xe9) {
         int32_t iCallRelativeAddress = 0;
         memcpy(&iCallRelativeAddress, vFunctionCode.data() + i + 1, sizeof(int32_t));
         int32_t iNextAbsoluteAddress = iAbsoluteCodeAddress + iCallRelativeAddress + 5;
         if (iNextAbsoluteAddress == (int32_t)_pOldFunctionAddress) {
            // On remplace l'addresse relative de `_pOldFunctionAddress` par `_pNewFunctionAddress`
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
            // On remplace l'adresse appelée
            m_vui32CallIndirectTable.push_back((uint32_t)_pNewFunctionAddress);
            uint32_t ui32IndirectAddr = (uint32_t)&m_vui32CallIndirectTable.back();
            memcpy(vFunctionCode.data() + i + 2, &ui32IndirectAddr, sizeof(uint32_t));
         }
      }
   }

   // Réécriture du code
   if (!WriteProcessMemory(m_hProcess, _pFunctionAddress, vFunctionCode.data(), vFunctionCode.size(), nullptr))
   {
      std::cerr << "CPPMock::mockFunction: unable to rewrite process memory for function "
                << std::hex << _pFunctionAddress
                << "(error " << GetLastError() <<  ")" << std::endl;
      return E_FAIL;
   }

   return S_OK;
}




HRESULT CPPMock::mockFunction(void* _pFunctionAddress, void* _pOldFunctionAddress, void* _pNewFunctionAddress)
{
   // On récupère le symbole qui correspond à la fonction afin de déterminer sa taille.
   SYMBOL_INFO* pSymbolInfo = getSymbol(_pFunctionAddress);
   if (pSymbolInfo == nullptr) {
      std::cerr << "CPPMock::mockFunction(): unable to find symbol address for function "
                << std::hex << _pFunctionAddress
                << " (error " << GetLastError() << ")" << std::endl;
      return E_FAIL;
   }
   size_t sizeFunctionSize = pSymbolInfo->Size;
   free(pSymbolInfo);

   return rewriteFunctionCodeCalls(_pFunctionAddress, sizeFunctionSize, _pOldFunctionAddress, _pNewFunctionAddress);
}




void* CPPMock::getVirtualMethodAddress(void* _pObjectInstance, const std::string& _sVirtualMethodeName, size_t& _sizeVirtualMethodeSize)
{
   void* pVtable = (void*)*(uint32_t*)_pObjectInstance;
   SYMBOL_INFO* pSymbolInfo = getSymbol(pVtable);
   if (!pSymbolInfo) {
      return nullptr;
   }
   size_t sizeVtable = (pSymbolInfo->Size - 4) / 4;
   free(pSymbolInfo);

   void* pAddr = nullptr;
   for (size_t i = 0; i < sizeVtable; i++) {
      void* pVFunctionAddress = (void*)*((int*)pVtable + i);
      SYMBOL_INFO* pFunctionSymbolInfo = getSymbol(pVFunctionAddress);
      if (pFunctionSymbolInfo == nullptr) {
         // NOTE perhaps we should fail here. Indeed, it would be a bad news to don't
         //      find functions symbols.
         continue;
      }
      std::string sSymbolName = pFunctionSymbolInfo->Name;
      size_t sizeVirtualMethodSize = pFunctionSymbolInfo->Size;
      free(pFunctionSymbolInfo);
      size_t sizeSuffixPosition = sSymbolName.find_last_of("::");
      if (sizeSuffixPosition == sSymbolName.npos) {
         continue;
      }
      std::string sMethodName = sSymbolName.substr(sizeSuffixPosition + 1);
      if (sMethodName.size() == _sVirtualMethodeName.size()
      &&  sMethodName.compare(_sVirtualMethodeName) == 0)
      {
         pAddr = pVFunctionAddress;
         _sizeVirtualMethodeSize = sizeVirtualMethodSize;
         break;
      }
   }

   return pAddr;
}




HRESULT CPPMock::mockVirtualMethod(void* _pObjectInstance, const std::string& _sVirtualMethodName, void* _pOldFunctionAddress, void* _pNewFunctionAddress)
{
   size_t sizeMethodSize = 0;
   void* pMethodAddress = getVirtualMethodAddress(_pObjectInstance, _sVirtualMethodName, sizeMethodSize);
   if (pMethodAddress == nullptr) {
      std::cerr << "CPPMock::mockVirtualMethod(): unable to find symbol address for function "
                << _sVirtualMethodName << std::endl;
      return E_FAIL;
   }

   return rewriteFunctionCodeCalls(pMethodAddress, sizeMethodSize, _pOldFunctionAddress, _pNewFunctionAddress);
}